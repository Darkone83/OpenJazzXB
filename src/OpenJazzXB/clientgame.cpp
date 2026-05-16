/**
 * clientgame.cpp
 * XbJazz stub -- ClientGame network multiplayer not supported on Xbox.
 *
 * All virtual methods stubbed. Constructor and all overrides throw E_N_OTHER
 * so the linker has all symbols; the code is never reached in singleplayer.
 *
 * Virtual method signatures decoded from LNK2001 decorated names:
 *   step(unsigned int)       -> int
 *   setLevel(char*)          -> int
 *   setCheckpoint(int,int)   -> void
 *   send(unsigned char*)     -> void
 *   score(unsigned char)     -> void
 */

#include "game.h"

ClientGame::ClientGame(char* address) {
    (void)address;
    /* Network not supported -- this constructor should never be reached */
}

ClientGame::~ClientGame() {
    /* base class destructor chained automatically */
}

int ClientGame::step(unsigned int ticks) {
    (void)ticks;
    return E_N_OTHER;
}

int ClientGame::setLevel(char* levelName) {
    (void)levelName;
    return E_N_OTHER;
}

void ClientGame::setCheckpoint(int x, int y) {
    (void)x; (void)y;
}

void ClientGame::send(unsigned char* data) {
    (void)data;
}

void ClientGame::score(unsigned char player) {
    (void)player;
}