/**
 * servergame.cpp
 * XbJazz stub -- ServerGame network multiplayer not supported on Xbox.
 *
 * Virtual method signatures decoded from LNK2001 decorated names:
 *   step(unsigned int)       -> int
 *   setLevel(char*)          -> int
 *   setCheckpoint(int,int)   -> void
 *   send(unsigned char*)     -> void
 *   score(unsigned char)     -> void
 */

#include "game.h"
#include "gamemode.h"

ServerGame::ServerGame(GameModeType mode, char* levelName, difficultyType difficulty) {
    (void)mode; (void)levelName; (void)difficulty;
    /* Network not supported -- this constructor should never be reached */
}

ServerGame::~ServerGame() {
    /* base class destructor chained automatically */
}

int ServerGame::step(unsigned int ticks) {
    (void)ticks;
    return E_N_OTHER;
}

int ServerGame::setLevel(char* levelName) {
    (void)levelName;
    return E_N_OTHER;
}

void ServerGame::setCheckpoint(int x, int y) {
    (void)x; (void)y;
}

void ServerGame::send(unsigned char* data) {
    (void)data;
}

void ServerGame::score(unsigned char player) {
    (void)player;
}