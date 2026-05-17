/**
 * clientgame.cpp
 * XbJazz -- ClientGame for relay-based multiplayer.
 *
 * Key differences from OJ original:
 *  - No throw/catch in constructor (RXDK unsafe)
 *  - net->join() returns our already-connected relay socket
 *  - MT_G_PROPS already in stream (relay sent it after OJXB_MAPSEL)
 *  - Level data comes from host via relay as MT_G_LEVEL chunks
 */

#include "game.h"
#include "file.h"
#include "font.h"
#include "video.h"
#include "network.h"
#include "player.h"
#include "loop.h"
#include "controls.h"
#include "setup.h"
#include "util.h"
#include <string.h>

ClientGame::ClientGame(char* address) {
    unsigned char buffer[BUFFER_LENGTH];
    unsigned int  timeout;
    int           count, ret;
    GameModeType  modeType;

    file = NULL;
    received = 0;
    sock = -1;

    sock = net->join(address);
    if (sock < 0) return;

    /* Wait for MT_G_PROPS from relay */
    count = 0;
    timeout = globalTicks + T_SCHECK + T_TIMEOUT;

    while (count < MTL_G_PROPS) {
        if (loop(NORMAL_LOOP) == E_QUIT) {
            net->close(sock); sock = -1; return;
        }
        if (controls.release(C_ESCAPE)) {
            net->close(sock); sock = -1; return;
        }
        SDL_Delay(T_MENU_FRAME);
        video.clearScreen(0);
        fontmn2->showStringCentered("CONNECTING...");

        ret = net->recv(sock, buffer + count, MTL_G_PROPS - count);
        if (ret > 0) count += ret;
        if (globalTicks > timeout) {
            net->close(sock); sock = -1; return;
        }
    }

    if (buffer[1] != MT_G_PROPS || buffer[2] != 1) {
        net->close(sock); sock = -1; return;
    }

    modeType = GameModeType(buffer[3]);
    difficulty = static_cast<difficultyType>(buffer[4]);
    maxPlayers = buffer[5];
    nPlayers = buffer[6];
    clientID = buffer[7];

    if (nPlayers > maxPlayers) {
        net->close(sock); sock = -1; return;
    }

    mode = createMode(modeType);
    if (!mode) { net->close(sock); sock = -1; return; }

    nPlayers = 0;
    players = new Player[maxPlayers];

    levelFile = createString(LEVEL_FILE);
    file = NULL;

    ret = setLevel(NULL);
    if (ret < 0) {
        net->close(sock);
        if (file) delete file;
        delete mode;
        sock = -1;
        return;
    }

    /* Send MT_G_PJOIN */
    buffer[0] = MTL_G_PJOIN + strlen(setup.characterName);
    buffer[1] = MT_G_PJOIN;
    buffer[2] = clientID;
    buffer[3] = 0;
    buffer[4] = 0;
    memcpy(buffer + 5, setup.characterCols, 4);
    memcpy(buffer + 9, setup.characterName, strlen(setup.characterName) + 1);
    send(buffer);

    /* Wait for acknowledgement (our own PJOIN echoed back) */
    localPlayer = NULL;
    while (!localPlayer) {
        if (loop(NORMAL_LOOP) == E_QUIT) {
            net->close(sock);
            if (file) delete file;
            delete mode;
            sock = -1;
            return;
        }
        if (controls.release(C_ESCAPE)) {
            net->close(sock);
            if (file) delete file;
            delete mode;
            sock = -1;
            return;
        }
        video.clearScreen(0);
        fontmn2->showStringCentered("JOINING GAME");
        ret = step(0);
        if (ret < 0) {
            net->close(sock);
            if (file) delete file;
            delete mode;
            sock = -1;
            return;
        }
    }
}

ClientGame::~ClientGame() {
    net->close(sock);
    if (file) delete file;
    if (mode) delete mode;
}

int ClientGame::setLevel(char* fileName) {
    (void)fileName;
    int ret;

    video.setPalette(menuPalette);

    while (!file && levelFile) {
        if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
        if (controls.release(C_ESCAPE)) return E_RETURN;
        SDL_Delay(T_MENU_FRAME);
        video.clearScreen(0);
        fontmn2->showStringCentered("WAITING FOR SERVER");
        ret = step(0);
        if (ret < 0) return ret;
    }

    while (file && levelFile) {
        if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
        if (controls.release(C_ESCAPE)) return E_RETURN;
        SDL_Delay(T_MENU_FRAME);
        video.clearScreen(0);
        fontmn2->showNumber(file->tell(), (canvasW >> 2) + 56, canvasH >> 1);
        fontmn2->showString("bytes", (canvasW >> 2) + 64, canvasH >> 1);
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

    if (received == 0) {
        length = net->recv(sock, recvBuffer, 1);
        if (length > 0) received++;
    }

    if (received > 0) {
        length = net->recv(sock, recvBuffer + received,
            recvBuffer[0] - received);
        if (length > 0) received += length;

        if (received >= recvBuffer[0]) {
            switch (recvBuffer[1] & MCMASK) {

            case MC_GAME:
                if (recvBuffer[1] == MT_G_LEVEL) {
                    bool firstMsg = !file;
                    if (!file) {
                        try {
                            file = new File(levelFile, PATH_TYPE_TEMP, true);
                        }
                        catch (int e) { return e; }
                    }
                    if (file) {
                        file->seek((recvBuffer[2] << 8) + recvBuffer[3], true);
                        for (int i = 4; i < recvBuffer[0]; i++)
                            file->storeChar(recvBuffer[i]);
                    }
                    if (recvBuffer[0] == MTL_G_LEVEL) {
                        if (firstMsg) {
                            delete[] levelFile;
                            levelFile = NULL;
                        }
                        delete file;
                        file = NULL;
                    }
                }

                if ((recvBuffer[1] == MT_G_PJOIN) &&
                    (recvBuffer[3] < maxPlayers)) {
                    int pn;
                    for (pn = nPlayers; pn <= recvBuffer[3]; pn++) {
                        players[pn].init(this,
                            reinterpret_cast<char*>(recvBuffer + 9),
                            recvBuffer + 5, recvBuffer[4]);
                        addLevelPlayer(players + pn);
                    }
                    nPlayers = recvBuffer[3] + 1;
                    if (recvBuffer[2] == clientID)
                        localPlayer = players + recvBuffer[3];
                }

                if ((recvBuffer[1] == MT_G_PQUIT) &&
                    (recvBuffer[2] < nPlayers)) {
                    players[recvBuffer[2]].deinit();
                    for (int i = recvBuffer[2]; i < nPlayers; i++)
                        memcpy(static_cast<void*>(players + i),
                            players + i + 1, sizeof(Player));
                    memset(static_cast<void*>(players + nPlayers), 0, sizeof(Player));
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
                    for (int i = 0; i < nPlayers; i++)
                        if (players[i].getTeam() == recvBuffer[2])
                            players[i].teamScore++;
                }

                if (recvBuffer[1] == MT_G_LTYPE)
                    levelType = (LevelType)recvBuffer[2];

                break;

            case MC_LEVEL:
                if (baseLevel) baseLevel->receive(recvBuffer);
                break;

            case MC_PLAYER:
                if (recvBuffer[2] < maxPlayers)
                    players[recvBuffer[2]].receive(recvBuffer);
                break;
            }
            received = 0;
        }
    }

    if (ticks >= checkTime) {
        if (!net->isConnected(sock)) {
            if (file) delete file;
            file = NULL;
            return E_N_DISCONNECT;
        }
        checkTime = ticks + T_CCHECK;
    }

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
    buffer[2] = gridX & 0xFF;
    buffer[3] = gridY & 0xFF;
    buffer[4] = (gridX >> 8) & 0xFF;
    buffer[5] = (gridY >> 8) & 0xFF;
    send(buffer);
}