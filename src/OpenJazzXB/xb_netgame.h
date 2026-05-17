#pragma once
/* Set these before calling XbNetGameLaunch() */
extern int g_netIsHost;
extern int g_netEpisode;
extern int g_netLevel;
extern int g_netDifficulty;
int XbNetGameLaunch();