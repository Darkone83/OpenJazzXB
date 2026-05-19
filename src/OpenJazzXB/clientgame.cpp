/**
 * clientgame.cpp
 * XbJazz port of OpenJazz ClientGame.
 *
 * Xbox network client path.
 *
 * Important fixes in this version:
 * - Initializes receive/player state before network work.
 * - Pulls the client level filename from XbNetG_GetClientLevelFile().
 * - Does not clear localPlayer after setLevel().
 * - Does not call addLevelPlayer() until baseLevel exists.
 * - Protects fake File* sentinel from delete.
 * - Adds packet length sanity checking.
 * - Returns E_N_DISCONNECT cleanly when the host/relay drops.
 */

#include "game.h"
#include "gamemode.h"
#include "controls.h"
#include "font.h"
#include "video.h"
#include "network.h"
#include "player.h"
#include "loop.h"
#include "setup.h"
#include "util.h"
#include "xb_net_glue.h"

#include <string.h>

extern "C" void XbNetLog_Write(const char* msg);  /* diagnostic */


ClientGame::ClientGame(char* address) {

    /*
     * Initialize all state before any network receive / error path.
     */
    file = NULL;
    received = 0;
    clientID = 0;
    maxPlayers = 0;
    nPlayers = 0;
    localPlayer = NULL;
    players = NULL;
    mode = NULL;
    sock = -1;

    unsigned char buffer[BUFFER_LENGTH];
    unsigned int  timeout;
    int           count, ret;
    GameModeType  modeType;

    sock = net->join(address);
    if (sock < 0) throw sock;


    /* ── Wait for MT_G_PROPS ──────────────────────────────────────────── */

    count = 0;
    timeout = globalTicks + T_SCHECK + T_TIMEOUT;

    while (count < MTL_G_PROPS) {

        if (loop(NORMAL_LOOP) == E_QUIT) {
            net->close(sock);
            throw E_QUIT;
        }

        if (controls.release(C_ESCAPE)) {
            net->close(sock);
            throw E_RETURN;
        }

        SDL_Delay(T_MENU_FRAME);
        video.clearScreen(0);
        fontmn2->showStringCentered("WAITING FOR REPLY");

        ret = net->recv(sock, buffer + count, MTL_G_PROPS - count);
        if (ret < 0) {
            net->close(sock);
            throw E_N_DISCONNECT;
        }
        if (ret > 0) count += ret;

        if (globalTicks > timeout) {
            net->close(sock);
            throw E_TIMEOUT;
        }
    }

    if (buffer[1] != MT_G_PROPS) {
        net->close(sock);
        throw E_DATA;
    }

    if (buffer[2] != 1) {
        net->close(sock);
        throw E_VERSION;
    }


    /* ── Parse game parameters ────────────────────────────────────────── */

    modeType = GameModeType(buffer[3]);
    difficulty = static_cast<difficultyType>(buffer[4]);
    maxPlayers = buffer[5];
    nPlayers = buffer[6];
    clientID = buffer[7];

    if (maxPlayers == 0) {
        net->close(sock);
        throw E_DATA;
    }

    if (nPlayers > maxPlayers) {
        net->close(sock);
        throw E_DATA;
    }

    mode = createMode(modeType);
    if (!mode) {
        net->close(sock);
        throw E_DATA;
    }


    /* ── Create player array ──────────────────────────────────────────── */

    nPlayers = 0;
    players = new Player[maxPlayers];


    /* ── Set client level filename from MAPSEL ────────────────────────── */

    /*
     * The lobby receives OJXB_MAPSEL before ClientGame is constructed.
     * Pull the stored filename here so Game::play() has a real levelFile.
     */
    {
        char xbLevelFile[16];

        xbLevelFile[0] = 0;
        XbNetG_GetClientLevelFile(xbLevelFile, 16);

        if (!xbLevelFile[0]) {
            net->close(sock);
            delete mode;
            mode = NULL;
            throw E_DATA;
        }

        levelFile = createString(xbLevelFile);
    }


    /* ── Set level / drain level transfer ─────────────────────────────── */

    file = NULL;

    XbNetLog_Write("pre-setLevel");
    ret = setLevel(NULL);
    XbNetLog_Write("post-setLevel");

    if (ret < 0) {
        net->close(sock);

        if (file && file != reinterpret_cast<File*>(1))
            delete file;

        file = NULL;

        if (mode)
            delete mode;

        mode = NULL;

        throw ret;
    }


    /* ── Announce ourselves ───────────────────────────────────────────── */

    XbNetLog_Write("pre-PJOIN-send");

    buffer[0] = (unsigned char)(MTL_G_PJOIN + strlen(setup.characterName));
    buffer[1] = MT_G_PJOIN;
    buffer[2] = (unsigned char)clientID;
    buffer[3] = 0;   /* player slot, assigned by server */
    buffer[4] = 0;   /* team, assigned by server */

    memcpy(buffer + 5, setup.characterCols, 4);
    memcpy(buffer + 9, setup.characterName, strlen(setup.characterName) + 1);

    send(buffer);


    /* ── Wait for PJOIN acknowledgement / localPlayer assignment ─────── */

    /*
     * Do NOT clear localPlayer here.
     *
     * In the relay flow, MT_G_PJOIN can arrive during setLevel().
     * If that happens, localPlayer may already be valid here.
     */
    XbNetLog_Write("entering localPlayer loop");

    while (!localPlayer) {

        if (loop(NORMAL_LOOP) == E_QUIT) {
            net->close(sock);

            if (file && file != reinterpret_cast<File*>(1))
                delete file;

            file = NULL;

            if (mode)
                delete mode;

            mode = NULL;

            throw E_QUIT;
        }

        if (controls.release(C_ESCAPE)) {
            net->close(sock);

            if (file && file != reinterpret_cast<File*>(1))
                delete file;

            file = NULL;

            if (mode)
                delete mode;

            mode = NULL;

            throw E_RETURN;
        }

        video.clearScreen(0);
        fontmn2->showStringCentered("JOINING GAME");

        ret = step(0);

        if (ret < 0) {
            net->close(sock);

            if (file && file != reinterpret_cast<File*>(1))
                delete file;

            file = NULL;

            if (mode)
                delete mode;

            mode = NULL;

            throw ret;
        }
    }
}


ClientGame::~ClientGame() {

    net->close(sock);

    /*
     * setLevel() uses reinterpret_cast<File*>(1) as a fake receive sentinel.
     * Never delete that fake pointer.
     */
    if (file && file != reinterpret_cast<File*>(1))
        delete file;

    file = NULL;

    if (mode)
        delete mode;

    mode = NULL;
}


int ClientGame::setLevel(char* /*fileName*/) {

    int ret;

    video.setPalette(menuPalette);

    /*
     * Wait for level data to start arriving.
     *
     * Xbox deviation:
     * We do not write level packets to a temp file. The Xbox already has
     * local game files. We only drain MT_G_LEVEL packets so timing/state
     * remains compatible with the original OpenJazz network flow.
     */
    while (!file && levelFile) {

        if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
        if (controls.release(C_ESCAPE))  return E_RETURN;

        SDL_Delay(T_MENU_FRAME);
        video.clearScreen(0);
        fontmn2->showStringCentered("WAITING FOR SERVER");

        ret = step(0);
        if (ret < 0) return ret;
    }

    /*
     * Wait for level data to finish.
     * Final empty MT_G_LEVEL packet clears the sentinel in step().
     */
    while (file && levelFile) {

        if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
        if (controls.release(C_ESCAPE))  return E_RETURN;

        SDL_Delay(T_MENU_FRAME);
        video.clearScreen(0);
        fontmn2->showStringCentered("RECEIVING LEVEL");

        ret = step(0);
        if (ret < 0) return ret;
    }

    return E_NONE;
}


void ClientGame::send(unsigned char* buffer) {

    net->send(sock, buffer);
}


int ClientGame::step(unsigned int ticks) {

    int length;

    /*
     * Immediate disconnect guard.
     * This catches host/relay drops even when ticks == 0 during join/setup
     * loops, where the old periodic check would never run.
     */
    if (!net->isConnected(sock)) {
        if (file && file != reinterpret_cast<File*>(1))
            delete file;

        file = NULL;
        return E_N_DISCONNECT;
    }

    /* ── Receive from server ──────────────────────────────────────────── */

    if (received == 0) {

        length = net->recv(sock, recvBuffer, 1);

        if (length < 0) {
            if (file && file != reinterpret_cast<File*>(1))
                delete file;

            file = NULL;
            return E_N_DISCONNECT;
        }

        if (length > 0) {

            received++;

            /*
             * Basic packet length validation.
             * This prevents stream desync or bad relay data from causing
             * recvBuffer overrun / invalid pointer math.
             */
            if ((recvBuffer[0] < 2) || (recvBuffer[0] > BUFFER_LENGTH)) {
                received = 0;
                return E_DATA;
            }
        }
    }

    if (received > 0) {

        length = net->recv(sock, recvBuffer + received,
            recvBuffer[0] - received);

        if (length < 0) {
            if (file && file != reinterpret_cast<File*>(1))
                delete file;

            file = NULL;
            return E_N_DISCONNECT;
        }

        if (length > 0)
            received += length;

        if (received >= recvBuffer[0]) {

            switch (recvBuffer[1] & MCMASK) {

            case MC_GAME:

                if (recvBuffer[1] == MT_G_LEVEL) {

                    /*
                     * Xbox deviation:
                     * Drain level packets silently.
                     *
                     * file == NULL means not yet receiving.
                     * file == reinterpret_cast<File*>(1) means receiving.
                     */
                    if (!file)
                        file = reinterpret_cast<File*>(1);

                    if (recvBuffer[0] == MTL_G_LEVEL) {

                        /*
                         * Final empty packet -- done draining.
                         *
                         * Do NOT clear levelFile. On Xbox, levelFile is the
                         * real local level filename used by Game::play().
                         */
                        file = NULL;
                    }

                    break;
                }

                if ((recvBuffer[1] == MT_G_PJOIN) &&
                    (recvBuffer[3] < maxPlayers)) {

                    int pn;

                    for (pn = nPlayers; pn <= recvBuffer[3]; pn++) {

                        players[pn].init(this,
                            reinterpret_cast<char*>(recvBuffer + 9),
                            recvBuffer + 5,
                            recvBuffer[4]);

                        /*
                         * During relay handshake, PJOIN can arrive before
                         * JJ1Level/baseLevel exists. Do not attach a
                         * LevelPlayer until the level is actually live.
                         */
                        if (baseLevel)
                            addLevelPlayer(players + pn);
                    }

                    /*
                     * Only advance nPlayers. Never reduce it here.
                     */
                    if (pn > nPlayers)
                        nPlayers = pn;

                    if (recvBuffer[2] == clientID)
                        localPlayer = players + recvBuffer[3];
                }

                if ((recvBuffer[1] == MT_G_PQUIT) &&
                    (recvBuffer[2] < nPlayers)) {

                    /*
                     * In relay co-op, slot 0 is the host.
                     * If the host leaves, this client cannot continue.
                     */
                    if (recvBuffer[2] == 0) {
                        if (file && file != reinterpret_cast<File*>(1))
                            delete file;

                        file = NULL;
                        return E_N_DISCONNECT;
                    }

                    players[recvBuffer[2]].deinit();

                    int i;

                    for (i = recvBuffer[2]; i < nPlayers - 1; i++) {
                        memcpy(static_cast<void*>(players + i),
                            players + i + 1,
                            sizeof(Player));
                    }

                    nPlayers--;

                    memset(static_cast<void*>(players + nPlayers),
                        0,
                        sizeof(Player));
                }

                if (recvBuffer[1] == MT_G_CHECK) {

                    checkX = recvBuffer[2];
                    checkY = recvBuffer[3];

                    if (recvBuffer[0] > 4) {
                        checkX += recvBuffer[4] << 8;
                        checkY += recvBuffer[5] << 8;
                    }
                }

                if (recvBuffer[1] == MT_G_SCORE) {

                    int i;

                    for (i = 0; i < nPlayers; i++) {
                        if (players[i].getTeam() == recvBuffer[2])
                            players[i].teamScore++;
                    }
                }

                if (recvBuffer[1] == MT_G_LTYPE)
                    levelType = (LevelType)recvBuffer[2];

                break;

            case MC_LEVEL:

                XbNetLog_Write("MC_LEVEL rx");

                if (baseLevel)
                    baseLevel->receive(recvBuffer);

                break;

            case MC_PLAYER:

                XbNetLog_Write("MC_PLAYER rx");

                if (recvBuffer[2] < maxPlayers)
                    players[recvBuffer[2]].receive(recvBuffer);

                XbNetLog_Write("MC_PLAYER done");

                break;
            }

            received = 0;
        }
    }


    /* ── Connection check ─────────────────────────────────────────────── */

    if (ticks >= checkTime) {

        if (!(net->isConnected(sock))) {

            if (file && file != reinterpret_cast<File*>(1))
                delete file;

            file = NULL;

            return E_N_DISCONNECT;
        }

        checkTime = ticks + T_CCHECK;
    }


    /* ── Send local player state ──────────────────────────────────────── */

    if (localPlayer && (ticks >= sendTime)) {

        unsigned char sendBuffer[BUFFER_LENGTH];

        sendBuffer[0] = MTL_P_TEMP;
        sendBuffer[1] = MT_P_TEMP;
        sendBuffer[2] = 0;

        localPlayer->send(sendBuffer);
        send(sendBuffer);

        sendTime = ticks + T_CSEND;
    }

    return E_NONE;
}


void ClientGame::score(unsigned char team) {

    unsigned char buffer[MTL_G_SCORE];

    buffer[0] = MTL_G_SCORE;
    buffer[1] = MT_G_SCORE;
    buffer[2] = team;

    send(buffer);
}


void ClientGame::setCheckpoint(int gridX, int gridY) {

    unsigned char buffer[MTL_G_CHECK];

    buffer[0] = MTL_G_CHECK;
    buffer[1] = MT_G_CHECK;
    buffer[2] = (unsigned char)(gridX & 0xFF);
    buffer[3] = (unsigned char)(gridY & 0xFF);
    buffer[4] = (unsigned char)((gridX >> 8) & 0xFF);
    buffer[5] = (unsigned char)((gridY >> 8) & 0xFF);

    send(buffer);
}
