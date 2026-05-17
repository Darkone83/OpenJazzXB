/**
 * setupmenu.cpp
 * XbJazz -- Setup and network lobby menus.
 */

#include "menu.h"
#include <string.h>
#include "plasma.h"
#include "controls.h"
#include "font.h"
#include "video.h"
#include "sound.h"
#include "loop.h"
#include "setup.h"
#include "util.h"
#include "xb_config.h"
#include "xb_input.h"
#include "xb_textentry.h"
#include "xb_netgame.h"

extern void XbVideoApplyConfig(void);

/* -----------------------------------------------------------------------
   Stubs for OJ PC-only methods
   ----------------------------------------------------------------------- */
int SetupMenu::setupKeyboard() { return message("NOT AVAILABLE ON XBOX"); }
int SetupMenu::setupJoystick() { return message("NOT AVAILABLE ON XBOX"); }
int SetupMenu::setupVideo() { return message("NOT AVAILABLE ON XBOX"); }

/* -----------------------------------------------------------------------
   Net glue externs (plain C from xb_net_glue.cpp)
   ----------------------------------------------------------------------- */
struct XbNetRoomC {
	unsigned char room_id, nPlayers, maxPlayers, status;
	char hostName[17];
};
struct XbNetLobbyStateC {
	int nRooms;
	XbNetRoomC rooms[8];
	int mySlot;
	char players[2][17];
	int nPlayers;
};

extern "C" {
	int  XbNet_GetStateC(void);
	void XbNet_BeginResolveC(const char* host);
	void XbNet_PollC(void);
	void XbNet_ResetC(void);
	void XbNet_GetLocalIPC(char* buf, int len);
	int  XbNet_ConnectC(const char* name);
	int  XbNet_LobbyPollC(void* state);
	void XbNet_JoinRoomC(unsigned char room_id);
	void XbNet_DisconnectC(void);
	int  XbNet_IsConnectedC(void);
	void XbNet_SendMapselectC(const char* levelFile, unsigned char difficulty);
}

#define XBNETC_IDLE      0
#define XBNETC_LINK_WAIT 1
#define XBNETC_DNS_WAIT  2
#define XBNETC_READY     3
#define XBNETC_NO_LINK   4
#define XBNETC_DNS_FAIL  5

/* -----------------------------------------------------------------------
   Button name helper
   ----------------------------------------------------------------------- */
static const char* btnName(unsigned short btn) {
	switch (btn) {
	case XB_BTN_A:     return "A";
	case XB_BTN_B:     return "B";
	case XB_BTN_X:     return "X";
	case XB_BTN_Y:     return "Y";
	case XB_BTN_LTRIG: return "L-Trigger";
	case XB_BTN_RTRIG: return "R-Trigger";
	case XB_BTN_WHITE: return "White";
	case XB_BTN_BLACK: return "Black";
	case XB_BTN_BACK:  return "Back";
	default:           return "?";
	}
}

static const unsigned short MAPPABLE_BTNS[] = {
	XB_BTN_A, XB_BTN_X, XB_BTN_Y,
	XB_BTN_LTRIG, XB_BTN_RTRIG,
	XB_BTN_WHITE, XB_BTN_BLACK
};
#define MAPPABLE_COUNT 7

/* -----------------------------------------------------------------------
   xbSetupControls
   ----------------------------------------------------------------------- */
static int xbSetupControls() {
	const char* actionNames[5] = {
		"primary fire", "secondary fire", "alt jump / swim",
		"change weapon", "stats overlay"
	};
	unsigned short* actionBtns[5] = {
		&g_xbConfig.controls.btnFire,
		&g_xbConfig.controls.btnFireAlt,
		&g_xbConfig.controls.btnJumpAlt,
		&g_xbConfig.controls.btnChange,
		&g_xbConfig.controls.btnStats
	};
	int chosen = 0;
	bool waiting = false;
	Plasma plasma;
	{ int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
	video.setPalette(menuPalette);

	while (true) {
		if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
		if (!waiting) {
			if (controls.release(C_ESCAPE)) { XbConfigSave(); return E_NONE; }
			if (controls.release(C_UP))     chosen = (chosen + 4) % 5;
			if (controls.release(C_DOWN))   chosen = (chosen + 1) % 5;
			if (controls.release(C_ENTER))  waiting = true;
		}
		else {
			unsigned short btns = XbInputGetButtons();
			int bi;
			for (bi = 0; bi < MAPPABLE_COUNT; bi++) {
				if (btns & MAPPABLE_BTNS[bi]) {
					*actionBtns[chosen] = MAPPABLE_BTNS[bi];
					waiting = false;
					playConfirmSound();
					break;
				}
			}
		}

		SDL_Delay(T_MENU_FRAME);
		plasma.draw();

		fontmn2->showString("CONTROLS", canvasW >> 1, 8, alignX::Center);

		const int labelX = 12;
		const int valueX = (canvasW >> 1) + 20;
		const int rowH = 18;
		const int startY = 30;
		int i;
		for (i = 0; i < 5; i++) {
			int itemY = startY + i * rowH;
			if (i == chosen)
				video.drawRect(labelX - 2, itemY - 1, canvasW - labelX, rowH - 1, 79, false);
			fontmn2->showString(actionNames[i], labelX, itemY);
			if (waiting && i == chosen)
				fontmn2->showString("press button...", valueX, itemY);
			else
				fontmn2->showString(btnName(*actionBtns[i]), valueX, itemY);
		}

		fontbig->showString("a=jump  start=pause", 3, canvasH, alignX::Left, alignY::Bottom);
		fontbig->showString("a=remap  b=back", canvasW - 3, canvasH, alignX::Right, alignY::Bottom);
	}
	return E_NONE;
}

/* -----------------------------------------------------------------------
   xbSetupVideoXbox
   ----------------------------------------------------------------------- */
static int xbSetupVideoXbox() {
	const char* modeNames[4] = { "auto",    "480p",    "720p",    "480i" };
	const char* aspectNames[4] = { "4:3",     "stretch", "pixel",   "fill" };
	const char* filterNames[3] = { "sharp",   "smooth",  "scale2x" };
	const char* scanNames[4] = { "off",     "light",   "medium",  "heavy" };
	int chosen = 0;
	Plasma plasma;
	{ int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
	video.setPalette(menuPalette);

	while (true) {
		if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
		if (controls.release(C_ESCAPE)) { XbConfigSave(); return E_NONE; }
		if (controls.release(C_UP))   chosen = (chosen + 3) % 4;
		if (controls.release(C_DOWN)) chosen = (chosen + 1) % 4;
		if (controls.release(C_LEFT)) {
			switch (chosen) {
			case 0: g_xbConfig.videoMode = (XbVideoMode)(((int)g_xbConfig.videoMode + 3) % 4); break;
			case 1: g_xbConfig.aspectMode = (XbAspectMode)(((int)g_xbConfig.aspectMode + 3) % 4); break;
			case 2: g_xbConfig.filterMode = (XbFilterMode)(((int)g_xbConfig.filterMode + 2) % 3); break;
			case 3: g_xbConfig.scanlines = (XbScanlines)(((int)g_xbConfig.scanlines + 3) % 4); break;
			}
			XbVideoApplyConfig(); playConfirmSound();
		}
		if (controls.release(C_RIGHT)) {
			switch (chosen) {
			case 0: g_xbConfig.videoMode = (XbVideoMode)(((int)g_xbConfig.videoMode + 1) % 4); break;
			case 1: g_xbConfig.aspectMode = (XbAspectMode)(((int)g_xbConfig.aspectMode + 1) % 4); break;
			case 2: g_xbConfig.filterMode = (XbFilterMode)(((int)g_xbConfig.filterMode + 1) % 3); break;
			case 3: g_xbConfig.scanlines = (XbScanlines)(((int)g_xbConfig.scanlines + 1) % 4); break;
			}
			XbVideoApplyConfig(); playConfirmSound();
		}

		SDL_Delay(T_MENU_FRAME);
		plasma.draw();

		fontmn2->showString("VIDEO OPTIONS", canvasW >> 1, 8, alignX::Center);

		const int labelX = (canvasW >> 1) - 88;
		const int valueX = (canvasW >> 1) + 64;
		const char* rowLabels[4] = { "resolution < >", "aspect     < >", "filter     < >", "scanlines  < >" };
		const char* rowValues[4] = {
			modeNames[(int)g_xbConfig.videoMode],
			aspectNames[(int)g_xbConfig.aspectMode],
			filterNames[(int)g_xbConfig.filterMode],
			scanNames[(int)g_xbConfig.scanlines]
		};
		int i;
		for (i = 0; i < 4; i++) {
			int itemY = (canvasH >> 1) - 28 + i * 18;
			if (i == chosen)
				video.drawRect(labelX - 2, itemY - 1, canvasW - labelX, 16, 79, false);
			fontmn2->showString(rowLabels[i], labelX, itemY);
			fontmn2->showString(rowValues[i], valueX, itemY);
		}

		fontmn2->showString("resolution change needs restart",
			canvasW >> 1, (canvasH >> 1) + 44, alignX::Center);
		fontbig->showString("b = back", canvasW - 3, canvasH, alignX::Right, alignY::Bottom);
	}
	return E_NONE;
}

/* -----------------------------------------------------------------------
   xbSetupNetwork -- player name and server address
   ----------------------------------------------------------------------- */
static int xbSetupNetwork() {
	const char* optLabels[2] = { "player name", "server" };
	int chosen = 0;
	char nameBuf[16];
	char addrBuf[64];
	Plasma plasma;

	while (true) {
		{ int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
		video.setPalette(menuPalette);

		if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
		if (controls.release(C_ESCAPE)) return E_NONE;
		if (controls.release(C_UP))   chosen = (chosen + 1) % 2;
		if (controls.release(C_DOWN)) chosen = (chosen + 1) % 2;

		if (controls.release(C_ENTER)) {
			int ret;
			if (chosen == 0) {
				strncpy(nameBuf, g_xbConfig.playerName, sizeof(nameBuf) - 1);
				nameBuf[sizeof(nameBuf) - 1] = '\0';
				ret = xbTextEntry("PLAYER NAME", nameBuf, 12, XB_ENTRY_NAME);
				if (ret == E_QUIT) return E_QUIT;
				if (ret == E_NONE) {
					strncpy(g_xbConfig.playerName, nameBuf, sizeof(g_xbConfig.playerName) - 1);
					XbConfigSave();
				}
			}
			else {
				strncpy(addrBuf, g_xbConfig.serverAddr, sizeof(addrBuf) - 1);
				addrBuf[sizeof(addrBuf) - 1] = '\0';
				ret = xbTextEntry("SERVER ADDRESS", addrBuf, 20, XB_ENTRY_IP);
				if (ret == E_QUIT) return E_QUIT;
				if (ret == E_NONE) {
					strncpy(g_xbConfig.serverAddr, addrBuf, sizeof(g_xbConfig.serverAddr) - 1);
					XbConfigSave();
				}
			}
		}

		SDL_Delay(T_MENU_FRAME);
		plasma.draw();

		fontmn2->showString("NETWORK", canvasW >> 1, 8, alignX::Center);

		const int rowH = 26;
		const int baseY = (canvasH >> 1) - 22;
		const char* vals[2] = { g_xbConfig.playerName, g_xbConfig.serverAddr };
		int i;
		for (i = 0; i < 2; i++) {
			int labelY = baseY + i * rowH;
			int valueY = labelY + 12;
			if (i == chosen)
				video.drawRect(8, labelY - 1, canvasW - 16, rowH, 79, false);
			fontmn2->showString(optLabels[i], 12, labelY);
			fontbig->showString(vals[i], 20, valueY);
		}

		fontbig->showString("a=edit  b=back", canvasW - 3, canvasH, alignX::Right, alignY::Bottom);
	}
	return E_NONE;
}

/* -----------------------------------------------------------------------
   Error screen helper
   ----------------------------------------------------------------------- */
static void showNetError(const char* msg) {
	Plasma p;
	{ int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
	video.setPalette(menuPalette);
	int t = 0;
	while (t++ < 180) {
		if (loop(NORMAL_LOOP) == E_QUIT) return;
		if (controls.release(C_ESCAPE)) return;
		SDL_Delay(T_MENU_FRAME); p.draw();
		fontmn2->showString(msg, canvasW >> 1, canvasH >> 1, alignX::Center);
		fontbig->showString("b=back", canvasW - 3, canvasH, alignX::Right, alignY::Bottom);
	}
}

/* -----------------------------------------------------------------------
   xbJoinGame -- pre-flight -> DNS -> connect -> lobby -> room -> game
   ----------------------------------------------------------------------- */
static int xbJoinGame() {
	Plasma plasma;
	{ int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
	video.setPalette(menuPalette);

	/* ── Pre-flight ─────────────────────────────────────────────────────── */
	while (true) {
		if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
		if (controls.release(C_ESCAPE)) return E_NONE;
		if (controls.release(C_ENTER)) break;

		SDL_Delay(T_MENU_FRAME);
		plasma.draw();
		fontmn2->showString("JOIN NETWORK GAME", canvasW >> 1, 8, alignX::Center);

		const int rowH = 26;
		const int baseY = (canvasH >> 1) - 22;
		const char* pfLabels[2] = { "player name", "server" };
		const char* pfVals[2] = { g_xbConfig.playerName, g_xbConfig.serverAddr };
		int pfi;
		for (pfi = 0; pfi < 2; pfi++) {
			int labelY = baseY + pfi * rowH;
			int valueY = labelY + 12;
			video.drawRect(8, labelY - 1, canvasW - 16, rowH, 0, true);
			video.drawRect(8, labelY - 1, canvasW - 16, rowH, 79, false);
			fontmn2->showString(pfLabels[pfi], 12, labelY);
			fontbig->showString(pfVals[pfi], 20, valueY);
		}
		fontbig->showString("a=connect", 3, canvasH, alignX::Left, alignY::Bottom);
		fontbig->showString("b=back", canvasW - 3, canvasH, alignX::Right, alignY::Bottom);
	}

	/* ── DNS resolve ─────────────────────────────────────────────────────── */
	{ int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
	video.setPalette(menuPalette);
	XbNet_BeginResolveC(g_xbConfig.serverAddr);

	while (true) {
		if (loop(NORMAL_LOOP) == E_QUIT) { XbNet_ResetC(); return E_QUIT; }
		if (controls.release(C_ESCAPE)) { XbNet_ResetC(); return E_NONE; }
		XbNet_PollC();
		int state = XbNet_GetStateC();
		if (state == XBNETC_READY) break;
		if (state == XBNETC_NO_LINK || state == XBNETC_DNS_FAIL) {
			showNetError(state == XBNETC_NO_LINK ? "no network link" : "could not reach server");
			XbNet_ResetC(); return E_NONE;
		}
		const char* stateLabels[] = { "idle","checking link...","resolving server...","ready","no link","dns failed" };
		SDL_Delay(T_MENU_FRAME); plasma.draw();
		fontmn2->showString("JOIN NETWORK GAME", canvasW >> 1, 8, alignX::Center);
		fontmn2->showString(stateLabels[state > 5 ? 0 : state], canvasW >> 1, canvasH >> 1, alignX::Center);
		fontbig->showString("b=cancel", canvasW - 3, canvasH, alignX::Right, alignY::Bottom);
	}

	/* ── TCP connect + HELLO ─────────────────────────────────────────────── */
	if (XbNet_ConnectC(g_xbConfig.playerName) < 0) {
		showNetError("could not connect to server");
		XbNet_ResetC(); return E_NONE;
	}

	/* ── Lobby: room list ────────────────────────────────────────────────── */
	{ int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
	video.setPalette(menuPalette);

	XbNetLobbyStateC lobby;
	memset(&lobby, 0, sizeof(lobby));
	int chosen = 0;
	bool gotList = false;

	while (true) {
		if (loop(NORMAL_LOOP) == E_QUIT) { XbNet_DisconnectC(); XbNet_ResetC(); return E_QUIT; }
		if (controls.release(C_ESCAPE)) { XbNet_DisconnectC(); XbNet_ResetC(); return E_NONE; }

		int pr = XbNet_LobbyPollC(&lobby);
		if (pr < 0) { XbNet_ResetC(); return E_NONE; }
		if (pr > 0) gotList = true;

		int totalOptions = lobby.nRooms + 1;
		if (chosen >= totalOptions) chosen = 0;
		if (controls.release(C_UP))   chosen = (chosen + totalOptions - 1) % totalOptions;
		if (controls.release(C_DOWN)) chosen = (chosen + 1) % totalOptions;

		if (controls.release(C_ENTER)) {
			unsigned char rid = (chosen < lobby.nRooms) ? lobby.rooms[chosen].room_id : 0;
			int isHost = (rid == 0);
			XbNet_JoinRoomC(rid);

			/* ── In-room waiting screen ──────────────────────────────────── */
			{ int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
			video.setPalette(menuPalette);
			memset(&lobby, 0, sizeof(lobby));

			int episode = 0, levelNum = 0, diffChoice = 1, pickRow = 0;
			const char* diffNames[4] = { "easy","normal","hard","turbo" };
			Plasma roomPlasma;

			while (true) {
				if (loop(NORMAL_LOOP) == E_QUIT) { XbNet_DisconnectC(); XbNet_ResetC(); return E_QUIT; }
				if (controls.release(C_ESCAPE)) { XbNet_DisconnectC(); XbNet_ResetC(); return E_NONE; }

				int rr = XbNet_LobbyPollC(&lobby);
				if (rr < 0) { XbNet_ResetC(); return E_NONE; }

				if (isHost) {
					if (controls.release(C_UP))    pickRow = (pickRow + 2) % 3;
					if (controls.release(C_DOWN))  pickRow = (pickRow + 1) % 3;
					if (controls.release(C_LEFT)) {
						if (pickRow == 0) episode = (episode + 9) % 10;
						if (pickRow == 1) levelNum = (levelNum + 9) % 10;
						if (pickRow == 2) diffChoice = (diffChoice + 3) % 4;
					}
					if (controls.release(C_RIGHT)) {
						if (pickRow == 0) episode = (episode + 1) % 10;
						if (pickRow == 1) levelNum = (levelNum + 1) % 10;
						if (pickRow == 2) diffChoice = (diffChoice + 1) % 4;
					}
					if (controls.release(C_ENTER)) {
						char lvlFile[12];
						lvlFile[0] = 'L'; lvlFile[1] = 'E'; lvlFile[2] = 'V';
						lvlFile[3] = 'E'; lvlFile[4] = 'L';
						lvlFile[5] = '0' + levelNum;
						lvlFile[6] = '.';
						lvlFile[7] = '0' + (episode / 100) % 10;
						lvlFile[8] = '0' + (episode / 10) % 10;
						lvlFile[9] = '0' + episode % 10;
						lvlFile[10] = '\0';
						XbNet_SendMapselectC(lvlFile, (unsigned char)diffChoice);
						g_netIsHost = 1;
						g_netEpisode = episode;
						g_netLevel = levelNum;
						g_netDifficulty = diffChoice;
						XbNetGameLaunch();
						XbNet_DisconnectC(); XbNet_ResetC();
						/* Restore plasma palette after game exit */
						{ int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
						video.setPalette(menuPalette);
						return E_NONE;
					}
				}
				else {
					if (rr == 2) {
						g_netIsHost = 0;
						XbNetGameLaunch();
						XbNet_DisconnectC(); XbNet_ResetC();
						/* Restore plasma palette after game exit */
						{ int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
						video.setPalette(menuPalette);
						return E_NONE;
					}
				}

				SDL_Delay(T_MENU_FRAME);
				roomPlasma.draw();
				fontmn2->showString("ROOM", canvasW >> 1, 8, alignX::Center);

				int pi2;
				for (pi2 = 0; pi2 < lobby.nPlayers && pi2 < 2; pi2++) {
					int py = 24 + pi2 * 14;
					char sl[4] = { (char)('1' + pi2), '.', ' ', '\0' };
					fontmn2->showString(sl, 12, py);
					fontmn2->showString(lobby.players[pi2], 28, py);
					if (pi2 == 0) fontbig->showString("host", canvasW - 3, py, alignX::Right);
				}

				if (isHost) {
					const int pickY = (canvasH >> 1) - 18;
					char epStr[4] = { (char)('0' + episode),  '\0', '\0', '\0' };
					char lvStr[4] = { (char)('0' + levelNum), '\0', '\0', '\0' };
					const char* pickLabels[3] = { "episode", "level", "difficulty" };
					const char* pickVals[3] = { epStr, lvStr, diffNames[diffChoice] };
					int ri;
					for (ri = 0; ri < 3; ri++) {
						int ry = pickY + ri * 16;
						if (ri == pickRow) video.drawRect(8, ry - 1, canvasW - 16, 14, 79, false);
						fontmn2->showString(pickLabels[ri], 12, ry);
						fontmn2->showString(pickVals[ri], canvasW - 3, ry, alignX::Right);
					}
					fontbig->showString("a=start game", 3, canvasH, alignX::Left, alignY::Bottom);
				}
				else {
					fontmn2->showString("waiting for host...", canvasW >> 1, canvasH >> 1, alignX::Center);
				}
				fontbig->showString("b=leave", canvasW - 3, canvasH, alignX::Right, alignY::Bottom);
			}
		}

		SDL_Delay(T_MENU_FRAME);
		plasma.draw();
		fontmn2->showString("NETWORK LOBBY", canvasW >> 1, 8, alignX::Center);

		if (!gotList) {
			fontmn2->showString("waiting...", canvasW >> 1, canvasH >> 1, alignX::Center);
		}
		else {
			const int rowH2 = 16;
			const int baseY2 = 22;
			int i;
			for (i = 0; i < lobby.nRooms; i++) {
				int itemY = baseY2 + i * rowH2;
				if (i == chosen) video.drawRect(8, itemY - 1, canvasW - 16, rowH2 - 1, 79, false);
				fontmn2->showString(lobby.rooms[i].hostName, 12, itemY);
				char cnt[4] = { (char)('0' + lobby.rooms[i].nPlayers), '/', (char)('0' + lobby.rooms[i].maxPlayers), '\0' };
				fontbig->showString(cnt, canvasW >> 1, itemY);
				fontbig->showString(lobby.rooms[i].status ? "busy" : "open", canvasW - 3, itemY, alignX::Right);
			}
			int createY = baseY2 + lobby.nRooms * rowH2;
			if (lobby.nRooms == 0) {
				fontmn2->showString("no rooms", canvasW >> 1, createY, alignX::Center);
				createY += rowH2;
			}
			if (chosen == lobby.nRooms) video.drawRect(8, createY - 1, canvasW - 16, rowH2 - 1, 79, false);
			fontmn2->showString("+ new room", 12, createY);
		}
		fontbig->showString("a=join  b=back", canvasW - 3, canvasH, alignX::Right, alignY::Bottom);
	}
	return E_NONE;
}

/* -----------------------------------------------------------------------
   setupAudio
   ----------------------------------------------------------------------- */
int SetupMenu::setupAudio() {
	int x, y;
	bool soundActive = false;
	Plasma plasma;
	{ int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
	video.setPalette(menuPalette);

	while (true) {
		if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
		if (controls.release(C_ESCAPE)) return E_NONE;
		if (controls.release(C_ENTER))  return E_NONE;

		if (controls.getCursor(x, y)) {
			if ((x < 100) && (y >= canvasH - 12) && controls.wasCursorReleased()) return E_NONE;
			x -= (canvasW >> 1) + 48;
			y -= canvasH >> 1;
			if ((x >= 0) && (x < (MAX_VOLUME >> 1)) && (y >= 0) && (y < 11))  setMusicVolume(x << 1);
			if ((x >= 0) && (x < (MAX_VOLUME >> 1)) && (y >= 16) && (y < 27))  setSoundVolume(x << 1);
			if (controls.wasCursorReleased()) playConfirmSound();
		}

		if (controls.release(C_UP))   soundActive = !soundActive;
		if (controls.release(C_DOWN)) soundActive = !soundActive;
		if (controls.release(C_LEFT)) {
			if (soundActive) setSoundVolume(getSoundVolume() - 4);
			else             setMusicVolume(getMusicVolume() - 4);
			playConfirmSound();
		}
		if (controls.release(C_RIGHT)) {
			if (soundActive) setSoundVolume(getSoundVolume() + 4);
			else             setMusicVolume(getMusicVolume() + 4);
			playConfirmSound();
		}

		SDL_Delay(T_MENU_FRAME);
		plasma.draw();
		fontmn2->showString("AUDIO OPTIONS", canvasW >> 1, (canvasH >> 1) - 80, alignX::Center);

		if (!soundActive) video.drawRect((canvasW >> 1) - 90, (canvasH >> 1) - 1, 92, 14, 79, false);
		fontmn2->showString("music volume", (canvasW >> 1) - 88, canvasH >> 1);
		video.drawRect((canvasW >> 1) + 48, canvasH >> 1, getMusicVolume() >> 1, 11, 175);

		if (soundActive) video.drawRect((canvasW >> 1) - 90, (canvasH >> 1) + 15, 92, 14, 79, false);
		fontmn2->showString("effect volume", (canvasW >> 1) - 88, (canvasH >> 1) + 16);
		video.drawRect((canvasW >> 1) + 48, (canvasH >> 1) + 16, getSoundVolume() >> 1, 11, 175);

		showEscString();
	}
	return E_NONE;
}

/* -----------------------------------------------------------------------
   setupMain
   ----------------------------------------------------------------------- */
int SetupMenu::setupMain() {
	const char* options[6] = {
		"controls", "video", "audio", "network setup", "join game", "gameplay"
	};
	const char* setupModsOff[4] = {
		"slow motion: off", "extra items: take", "bird limit: one", "hud style: classic"
	};
	const char* setupModsOn[4] = {
		"slow motion: on", "extra items: leave", "bird limit: no", "hud style: old fps"
	};
	const char* setupMods[4];
	int ret, option = 0, suboption;

	setupMods[0] = setup.slowMotion ? setupModsOn[0] : setupModsOff[0];
	setupMods[1] = setup.leaveUnneeded ? setupModsOn[1] : setupModsOff[1];
	setupMods[2] = setup.manyBirds ? setupModsOn[2] : setupModsOff[2];
	setupMods[3] = (setup.hudStyle == hudType::FPS) ? setupModsOn[3] : setupModsOff[3];

	{ int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
	video.setPalette(menuPalette);

	while (true) {
		ret = generic("SETUP OPTIONS", options, 6, option);
		if (ret == E_RETURN) return E_NONE;
		if (ret < 0) return ret;

		switch (option) {
		case 0:
			if (xbSetupControls() == E_QUIT) return E_QUIT;
			break;
		case 1:
			if (xbSetupVideoXbox() == E_QUIT) return E_QUIT;
			break;
		case 2:
			if (setupAudio() == E_QUIT) return E_QUIT;
			break;
		case 3:
			if (xbSetupNetwork() == E_QUIT) return E_QUIT;
			break;
		case 4:
			if (xbJoinGame() == E_QUIT) return E_QUIT;
			break;
		case 5: {
			bool wasSlow = setup.slowMotion;
			suboption = 0;
			while (true) {
				ret = generic("GAME OPTIONS", setupMods, 4, suboption);
				if (ret == E_QUIT) return E_QUIT;
				if (ret < 0) break;
				setupMods[suboption] = (setupMods[suboption] == setupModsOff[suboption])
					? setupModsOn[suboption] : setupModsOff[suboption];
				setup.slowMotion = (setupMods[0] == setupModsOn[0]);
				setup.leaveUnneeded = (setupMods[1] == setupModsOn[1]);
				setup.manyBirds = (setupMods[2] == setupModsOn[2]);
				setup.hudStyle = (setupMods[3] == setupModsOn[3]) ? hudType::FPS : hudType::Classic;
			}
			break;
		}
		}
	}
	return E_NONE;
}