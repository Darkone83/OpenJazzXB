/**
 * xb_netgame.cpp
 * Launches ServerGame / ClientGame from the network lobby.
 * OJ TU -- no <xtl.h>. Can include game.h safely.
 */

#include "game.h"
#include "network.h"
#include "setup.h"
#include "util.h"

 /* Globals set by setupmenu, consumed by XbNetGameLaunch */
int g_netIsHost = 0;
int g_netEpisode = 0;
int g_netLevel = 0;
int g_netDifficulty = 1;

int XbNetGameLaunch() {
    int ret = E_NONE;
    if (g_netIsHost) {
        /* Tell network layer how many clients to fake-accept. */
        XbNet_SetExpectedClients(1);
        char* fn = createFileName("LEVEL", g_netLevel, g_netEpisode + 1);
        ServerGame* sg = NULL;
        try {
            sg = new ServerGame(M_SINGLE, fn, (difficultyType)g_netDifficulty);
        }
        catch (int e) { ret = e; }
        delete[] fn;
        if (sg) {
            /* Pump step() to send level data to client before play() runs.
             * ServerGame::step() sends MT_G_LEVEL chunks; client is waiting
             * in ClientGame::setLevel() until all chunks arrive. */
            unsigned int deadline = SDL_GetTicks() + 10000;
            while (sg->levelDataPending() && SDL_GetTicks() < deadline) {
                sg->step(SDL_GetTicks());
                SDL_Delay(16);
            }
            try {
                ret = sg->play();
            }
            catch (int e) { ret = e; }
            delete sg;
        }
    }
    else {
        try {
            Game* g = new ClientGame(netAddress);
            ret = g->play();
            delete g;
        }
        catch (int e) { ret = e; }
    }
    return ret;
}