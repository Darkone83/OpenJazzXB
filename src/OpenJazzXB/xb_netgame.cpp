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
        /* Tell network layer how many clients to fake-accept.
         * For MVP: always 1 (2-player game). */
        XbNet_SetExpectedClients(1);
        char* fn = createFileName("LEVEL", g_netLevel, g_netEpisode + 1);
        try {
            Game* g = new ServerGame(M_SINGLE, fn,
                (difficultyType)g_netDifficulty);
            ret = g->play();
            delete g;
        }
        catch (int e) { ret = e; }
        delete[] fn;
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