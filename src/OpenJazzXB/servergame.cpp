/**
 * servergame.cpp
 * XbJazz -- ServerGame for relay-based multiplayer.
 *
 * Key differences from OJ original:
 *  - No throw/catch in constructor (RXDK unsafe) -- errors returned via sock<0
 *  - net->accept() returns fake handles; relay handles real connections
 *  - net->send/recv ignore sock param, all traffic via relay socket
 *  - MT_G_PROPS already sent by relay on OJXB_MAPSEL -- skip re-sending it
 */

#include "game.h"
#include "file.h"
#include "font.h"
#include "video.h"
#include "network.h"
#include "player.h"
#include "setup.h"
#include "util.h"
#include <string.h>

ServerGame::ServerGame(GameModeType modeType, char* firstLevel,
    difficultyType gameDifficulty) {
    int count;

    sock = net->host();
    if (sock < 0) { sock = -1; return; }

    nPlayers = 1;
    localPlayer = players = new Player[MAX_PLAYERS];
    localPlayer->init(this, setup.characterName, setup.characterCols, 0);

    for (count = 0; count < MAX_CLIENTS; count++)
        clientPlayer[count] = clientStatus[count] = -1;

    levelFile = NULL;
    levelData = NULL;

    count = setLevel(firstLevel);
    if (count < 0) {
        net->close(sock);
        if (levelData) delete[] levelData;
        sock = -1;
        return;
    }

    difficulty = gameDifficulty;
    mode = createMode(modeType);
}

ServerGame::~ServerGame() {
    int count;
    for (count = 0; count < MAX_CLIENTS; count++)
        if (clientStatus[count] != -1) net->close(clientSock[count]);
    net->close(sock);
    if (levelData) delete[] levelData;
    if (mode) delete mode;
}

int ServerGame::setLevel(char* fileName) {
    File* file;
    int count;

    if (levelFile) delete[] levelFile;
    if (levelData) delete[] levelData;

    for (count = 0; count < MAX_CLIENTS; count++)
        if (clientStatus[count] != -1) clientStatus[count] = 0;

    if (!fileName) { levelFile = NULL; levelData = NULL; return E_NONE; }

    try {
        file = new File(fileName, PATH_TYPE_GAME);
    }
    catch (int e) {
        levelFile = NULL; levelData = NULL; return e;
    }

    levelFile = createString(fileName);
    levelSize = file->getSize();
    levelData = file->loadBlock(levelSize);
    delete file;

    levelType = getLevelType(fileName);

    if (levelType == LT_JJ1) {
        count = levelSize - 5;
        while (levelData[count - 1] != 3) count--;
        levelData[count] = fileName[strlen(fileName) - 3];
        levelData[count + 1] = fileName[strlen(fileName) - 2];
        levelData[count + 2] = fileName[strlen(fileName) - 1];
    }

    return E_NONE;
}

void ServerGame::send(unsigned char* buffer) {
    int count;
    for (count = 0; count < MAX_CLIENTS; count++) {
        if ((clientStatus[count] != -1) &&
            (((buffer[1] & MCMASK) != MC_PLAYER) ||
                (buffer[2] != clientPlayer[count])))
            net->send(clientSock[count], buffer);
    }
}

/* Returns true if any client is still waiting for level data */
bool ServerGame::levelDataPending() {
    int count;
    for (count = 0; count < MAX_CLIENTS; count++)
        if (clientStatus[count] >= 0) return true;
    return false;
}

int ServerGame::step(unsigned int ticks) {
    unsigned char sendBuffer[BUFFER_LENGTH];
    int count, pcount, length;

    for (count = 0; count < MAX_CLIENTS; count++) {

        /* ── Send level data to newly accepted clients ─────────────────── */
        if (clientStatus[count] >= 0) {

            if (clientStatus[count] == 0) {
                /* Send level type first */
                sendBuffer[0] = MTL_G_LTYPE;
                sendBuffer[1] = MT_G_LTYPE;
                sendBuffer[2] = levelType;
                net->send(clientSock[count], sendBuffer);
            }

            length = levelSize - clientStatus[count];
            if (length > 251) length = 251;

            sendBuffer[0] = MTL_G_LEVEL + length;
            sendBuffer[1] = MT_G_LEVEL;
            sendBuffer[2] = clientStatus[count] >> 8;
            sendBuffer[3] = clientStatus[count] & 255;
            memcpy(sendBuffer + 4, levelData + clientStatus[count], length);
            length = net->send(clientSock[count], sendBuffer);

            if (length == MTL_G_LEVEL) clientStatus[count] = -2;
            else if (length > 0) clientStatus[count] += length - MTL_G_LEVEL;
        }

        /* ── Receive from operational clients ──────────────────────────── */
        if ((clientStatus[count] == -2) && (received[count] == 0)) {
            length = net->recv(clientSock[count], recvBuffers[count], 1);
            if (length > 0) received[count]++;
        }

        if ((clientStatus[count] == -2) && (received[count] > 0)) {
            length = net->recv(clientSock[count],
                recvBuffers[count] + received[count],
                recvBuffers[count][0] - received[count]);
            if (length > 0) received[count] += length;

            if (received[count] >= recvBuffers[count][0]) {
                switch (recvBuffers[count][1] & MCMASK) {

                case MC_GAME:
                    if ((recvBuffers[count][1] == MT_G_PJOIN) &&
                        (clientPlayer[count] == -1)) {
                        recvBuffers[count][4] = mode->chooseTeam();
                        players[nPlayers].init(this,
                            reinterpret_cast<char*>(recvBuffers[count] + 9),
                            recvBuffers[count] + 5, recvBuffers[count][4]);
                        addLevelPlayer(players + nPlayers);
                        recvBuffers[count][3] = clientPlayer[count] = nPlayers;
                        nPlayers++;
                    }
                    if (recvBuffers[count][1] == MT_G_CHECK) {
                        checkX = recvBuffers[count][2];
                        checkY = recvBuffers[count][3];
                        if (recvBuffers[count][0] > 4) {
                            checkX += recvBuffers[count][4] << 8;
                            checkY += recvBuffers[count][5] << 8;
                        }
                    }
                    if (recvBuffers[count][1] == MT_G_SCORE) {
                        for (pcount = 0; pcount < nPlayers; pcount++)
                            if (players[pcount].getTeam() == recvBuffers[count][2])
                                players[pcount].teamScore++;
                    }
                    break;

                case MC_LEVEL:
                    if (baseLevel) baseLevel->receive(recvBuffers[count]);
                    break;

                case MC_PLAYER:
                    if (clientPlayer[count] != -1) {
                        recvBuffers[count][2] = clientPlayer[count];
                        players[clientPlayer[count]].receive(recvBuffers[count]);
                    }
                    break;
                }
                send(recvBuffers[count]);
                received[count] = 0;
            }
        }

        /* ── Accept new connections (relay-aware) ──────────────────────── */
        if (ticks >= checkTime) {
            if (clientStatus[count] == -1 && levelData) {
                clientSock[count] = net->accept(sock);
                if (clientSock[count] != -1) {
                    clientPlayer[count] = -1;
                    received[count] = 0;

                    /* Relay already sent MT_G_PROPS -- skip it.
                     * Just initiate level data sending. */
                    clientStatus[count] = 0;

                    /* Send checkpoint */
                    sendBuffer[0] = MTL_G_CHECK;
                    sendBuffer[1] = MT_G_CHECK;
                    sendBuffer[2] = checkX & 0xFF;
                    sendBuffer[3] = checkY & 0xFF;
                    sendBuffer[4] = (checkX >> 8) & 0xFF;
                    sendBuffer[5] = (checkY >> 8) & 0xFF;
                    net->send(clientSock[count], sendBuffer);

                    /* Send existing players */
                    sendBuffer[1] = MT_G_PJOIN;
                    for (pcount = 0; pcount < nPlayers; pcount++) {
                        sendBuffer[0] = MTL_G_PJOIN + strlen(players[pcount].getName());
                        sendBuffer[2] = count;
                        sendBuffer[3] = pcount;
                        sendBuffer[4] = players[pcount].getTeam();
                        memcpy(sendBuffer + 5, players[pcount].getCols(), PCOLOURS);
                        memcpy(sendBuffer + 9, players[pcount].getName(),
                            strlen(players[pcount].getName()) + 1);
                        net->send(clientSock[count], sendBuffer);
                    }
                }
            }
            else if (clientStatus[count] != -1) {
                if (!net->isConnected(clientSock[count])) {
                    net->close(clientSock[count]);
                    clientStatus[count] = -1;
                    if (clientPlayer[count] != -1) {
                        nPlayers--;
                        players[clientPlayer[count]].deinit();
                        for (pcount = clientPlayer[count]; pcount < nPlayers; pcount++)
                            memcpy(static_cast<void*>(players + pcount),
                                players + pcount + 1, sizeof(Player));
                        memset(static_cast<void*>(players + nPlayers), 0, sizeof(Player));
                        sendBuffer[0] = MTL_G_PQUIT;
                        sendBuffer[1] = MT_G_PQUIT;
                        sendBuffer[2] = clientPlayer[count];
                        send(sendBuffer);
                        clientPlayer[count] = -1;
                    }
                }
            }
        }
    }

    if (ticks >= checkTime) checkTime = ticks + T_SCHECK;

    if (ticks >= sendTime) {
        sendBuffer[0] = MTL_P_TEMP;
        sendBuffer[1] = MT_P_TEMP;
        for (count = 0; count < nPlayers; count++) {
            sendBuffer[2] = count;
            players[count].send(sendBuffer);
            send(sendBuffer);
        }
        sendTime = ticks + T_SSEND;
    }

    return E_NONE;
}

void ServerGame::score(unsigned char team) {
    unsigned char buffer[MTL_G_SCORE];
    int count;
    buffer[0] = MTL_G_SCORE;
    buffer[1] = MT_G_SCORE;
    buffer[2] = team;
    send(buffer);
    for (count = 0; count < nPlayers; count++)
        if (players[count].getTeam() == team) players[count].teamScore++;
}

void ServerGame::setCheckpoint(int gridX, int gridY) {
    unsigned char buffer[MTL_G_CHECK];
    buffer[0] = MTL_G_CHECK;
    buffer[1] = MT_G_CHECK;
    buffer[2] = gridX & 0xFF;
    buffer[3] = gridY & 0xFF;
    buffer[4] = (gridX >> 8) & 0xFF;
    buffer[5] = (gridY >> 8) & 0xFF;
    send(buffer);
    checkX = gridX;
    checkY = gridY;
}