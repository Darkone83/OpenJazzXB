/**
 * sound.cpp
 * XbJazz -- Xbox audio: psmplug music + PCM SFX via DirectSound streaming.
 *
 * Architecture:
 *   audioCallback() (this file) fills a buffer with psmplug music + mixed SFX.
 *   xb_audio_bridge() wraps audioCallback for XbAudioInit's function-pointer API.
 *   xb_audio.cpp (xtl.h-only TU) runs a DirectSound ring-buffer thread that
 *   calls xb_audio_bridge() periodically -- same pattern as XbTyrian loudness.cpp.
 *
 * No <xtl.h> here -- sound.cpp only needs psmplug, File, and standard headers.
 */

#include "file.h"
#include "sound.h"
#include "util.h"
#include "log.h"
#include "platforms.h"
#include "xb_sdl_compat.h"   /* SDL_AudioSpec, SDL_MIX_MAXVOLUME */
#define AUDIO_S16SYS  SDL_AUDIO_S16SYS   /* SDL1.2 name for Xbox build */
#include "xb_audio.h"        /* XbAudioInit/Deinit/Lock/Unlock */
#include "psmplug.h"

 /* Xbox RXDK: inline x87 float-to-int -- each asm on its own line */
static __inline int Ftoi(float x) {
    int r;
    __asm fld x
    __asm fistp dword ptr r
    return r;
}
static __inline int Ftoid(double x) {
    int r;
    __asm fld x
    __asm fistp dword ptr r
    return r;
}


#include <string.h>
#include <stdlib.h>

/* -----------------------------------------------------------------------
   Configuration
   ----------------------------------------------------------------------- */
#ifndef SOUND_FREQ
#  define SOUND_FREQ    44100
#endif
#ifndef SOUND_SAMPLES
#  define SOUND_SAMPLES 2048
#endif

   /* psmplug quality -- match OJ "high" preset */
#define MUSIC_RESAMPLEMODE MODPLUG_RESAMPLE_LINEAR
#define MUSIC_FLAGS        0

/* -----------------------------------------------------------------------
   Types
   ----------------------------------------------------------------------- */
typedef struct {
    unsigned char* data;
    char* name;
    int            length;
} RawSound;

typedef struct {
    unsigned char* data;
    int            length;
    int            position;
} Sound;

/* -----------------------------------------------------------------------
   Module-private state
   ----------------------------------------------------------------------- */
static RawSound* rawSounds = nullptr;
static int           nRawSounds = 0;
static Sound         sounds[SE::MAX];
static bool          soundsLoaded = false;
static ModPlugFile* musicFile = nullptr;
static SDL_AudioSpec audioSpec;
static bool          musicPaused = false;
static int           musicVolume = MAX_VOLUME;         /* 100% */
static int           soundVolume = (MAX_VOLUME * 3) / 4; /* 75% */
static char* currentMusic = nullptr;
static MusicTempo    musicTempo = MusicTempo::NORMAL;

/* -----------------------------------------------------------------------
   Lock / Unlock helpers (wrap XbAudio CRITICAL_SECTION)
   ----------------------------------------------------------------------- */
static void LockAudio() { XbAudioLock(); }
static void UnlockAudio() { XbAudioUnlock(); }

/* -----------------------------------------------------------------------
   SDL_MixAudio_S16 -- saturating signed-16 mix at volume 0-128
   ----------------------------------------------------------------------- */
static void SDL_MixAudio_S16(unsigned char* dst, const unsigned char* src,
    int len, int vol) {
    const short* s = (const short*)src;
    short* d = (short*)dst;
    for (int i = 0, n = len / 2; i < n; i++) {
        int v = d[i] + (s[i] * vol / 128);
        d[i] = (short)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
    }
}

/* -----------------------------------------------------------------------
   audioCallback -- called from AudioThread in xb_audio.cpp
   Fills `len` bytes: psmplug music first, then SFX mixed on top.
   ----------------------------------------------------------------------- */
static void audioCallback(void* /*userdata*/, unsigned char* stream, int len) {
    memset(stream, 0, (size_t)len);

    if (musicFile && !musicPaused)
        ModPlug_Read(musicFile, stream, len);

    if (!soundsLoaded) return;

    for (int i = SE::NONE; i < SE::MAX; i++) {
        if (!sounds[i].data || sounds[i].position < 0) continue;

        int rest = sounds[i].length - sounds[i].position;
        int length = (len < rest) ? len : rest;
        int position = sounds[i].position;

        if (len < rest) sounds[i].position += len;
        else            sounds[i].position = -1;

        SDL_MixAudio_S16(stream, sounds[i].data + position, length,
            soundVolume * SDL_MIX_MAXVOLUME / MAX_VOLUME);
    }
}

/* -----------------------------------------------------------------------
   xb_audio_bridge -- adapts audioCallback to the XbAudioInit callback type
   ----------------------------------------------------------------------- */
static void xb_audio_bridge(unsigned char* stream, int len) {
    audioCallback(nullptr, stream, len);
}

/* -----------------------------------------------------------------------
   openAudio
   ----------------------------------------------------------------------- */
void openAudio() {
    memset(&audioSpec, 0, sizeof(audioSpec));
    audioSpec.freq = SOUND_FREQ;
    audioSpec.format = AUDIO_S16SYS;
    audioSpec.channels = 2;
    audioSpec.samples = SOUND_SAMPLES;

    bool ok = XbAudioInit(audioSpec.channels, audioSpec.freq,
        audioSpec.samples, xb_audio_bridge);
    if (!ok) {
        LOG_ERROR("XbJazz: XbAudioInit failed -- no audio");
        return;
    }

    LOG_DEBUG("Audio: %dHz %d-bit %dch %d samples",
        audioSpec.freq, 16, audioSpec.channels, audioSpec.samples);

    soundsLoaded = loadSounds("SOUNDS.000");
    /* audio thread already running after XbAudioInit */
}

/* -----------------------------------------------------------------------
   closeAudio
   ----------------------------------------------------------------------- */
void closeAudio() {
    stopMusic();
    XbAudioDeinit();

    if (rawSounds) {
        for (int i = 0; i < nRawSounds; i++) {
            free(rawSounds[i].data);
            free(rawSounds[i].name);
        }
        free(rawSounds);
        rawSounds = nullptr;
        nRawSounds = 0;
    }

    if (soundsLoaded) freeSounds();
    soundsLoaded = false;
}

/* -----------------------------------------------------------------------
   playMusic
   ----------------------------------------------------------------------- */
void playMusic(const char* fileName, bool restart) {
    if (currentMusic && strcmp(fileName, currentMusic) == 0 && !restart)
        return;

    stopMusic();
    LockAudio();

    File* file = new File(fileName, PATH_TYPE_GAME);
    if (!file->isOpen()) {
        LOG_WARN("playMusic: cannot open %s", fileName);
        delete file;
        UnlockAudio();
        return;
    }

    free(currentMusic);
    currentMusic = createString(fileName);

    int            size = file->getSize();
    file->seek(0, true);
    unsigned char* psmData = file->loadBlock(size);
    delete file;

    ModPlug_Settings settings;
    memset(&settings, 0, sizeof(settings));
    settings.mFlags = MUSIC_FLAGS;
    settings.mChannels = audioSpec.channels;
    settings.mBits = 16;
    settings.mFrequency = audioSpec.freq;
    settings.mResamplingMode = MUSIC_RESAMPLEMODE;
    settings.mReverbDepth = 25;
    settings.mReverbDelay = 40;
    settings.mBassAmount = 50;
    settings.mBassRange = 10;
    settings.mSurroundDepth = 50;
    settings.mSurroundDelay = 40;
    settings.mLoopCount = -1;   /* infinite loop */

    ModPlug_SetSettings(&settings);
    musicFile = ModPlug_Load(psmData, size);
    free(psmData);

    if (!musicFile) {
        LOG_ERROR("playMusic: ModPlug_Load failed for %s", fileName);
        free(currentMusic);
        currentMusic = nullptr;
    }

    setMusicVolume(musicVolume);
    musicPaused = false;
    UnlockAudio();
}

void pauseMusic(bool pause) { musicPaused = pause; }

void stopMusic() {
    LockAudio();
    if (musicFile) {
        ModPlug_Unload(musicFile);
        musicFile = nullptr;
    }
    free(currentMusic);
    currentMusic = nullptr;
    UnlockAudio();
}

int  getMusicVolume() { return musicVolume; }

void setMusicVolume(int volume) {
    musicVolume = (volume < 0 ? 0 : volume > MAX_VOLUME ? MAX_VOLUME : volume);
    if (!musicFile) return;
    ModPlug_SetMasterVolume(musicFile, (unsigned int)Ftoi(musicVolume * 5.12f));
}

MusicTempo getMusicTempo() { return musicTempo; }

void setMusicTempo(MusicTempo tempo) {
    musicTempo = tempo;
    if (!musicFile) return;
    ModPlug_SetMusicTempoFactor(musicFile, (unsigned int)tempo);
}

/* -----------------------------------------------------------------------
   loadSounds / resampleSounds / resampleSound / freeSounds
   ----------------------------------------------------------------------- */
bool loadSounds(const char* fileName) {
    File* file = new File(fileName, PATH_TYPE_GAME);
    if (!file->isOpen()) { delete file; return false; }

    char* identifier1 = file->loadString(3);
    char  identifier2 = file->loadChar();
    if (strncmp(identifier1, "sfx", 2) != 0 || identifier2 != 0x1A) {
        LOG_ERROR("Sound data not valid!");
        free(identifier1);
        delete file;
        return false;
    }
    free(identifier1);

    file->seek(file->getSize() - 4, true);
    int headerOffset = file->loadInt();

    nRawSounds = (file->getSize() - headerOffset) / 18;
    LOG_TRACE("Loading %d sounds...", nRawSounds);

    if (nRawSounds >= SE::MAX) {
        LOG_WARN("Too many sounds (%d >= %d), clamping", nRawSounds, (int)SE::MAX);
        nRawSounds = SE::MAX - 1;
    }

    rawSounds = (RawSound*)malloc(nRawSounds * sizeof(RawSound));
    if (!rawSounds) { delete file; return false; }
    memset(rawSounds, 0, nRawSounds * sizeof(RawSound));

    for (int i = 0; i < nRawSounds; i++) {
        file->seek(headerOffset + (i * 18), true);
        rawSounds[i].name = file->loadString(12);
        int offset = file->loadInt();
        rawSounds[i].length = file->loadShort();
        file->seek(offset, true);
        rawSounds[i].data = file->loadBlock(rawSounds[i].length);
    }

    delete file;
    resampleSounds();
    return true;
}

void resampleSound(int index, const char* name, int rate) {
    int se = index + 1;
    if (se < 0 || se >= SE::MAX) return;
    if (!name || !strlen(name)) {
        /* Clear under lock so AudioThread never sees stale pointer */
        LockAudio();
        if (sounds[se].data) { free(sounds[se].data); sounds[se].data = nullptr; }
        UnlockAudio();
        return;
    }

    for (int i = 0; i < nRawSounds; i++) {
        if (strcmp(name, rawSounds[i].name) != 0) continue;

        /* Build the resampled buffer OUTSIDE the lock (slow work, no shared state) */
        int srcSamples = rawSounds[i].length;
        int dstSamples = Ftoid((double)srcSamples * audioSpec.freq / rate + 0.5);
        int dstBytes = dstSamples * audioSpec.channels * 2; /* S16 stereo */

        unsigned char* newBuf = (unsigned char*)malloc(dstBytes);
        if (!newBuf) return;

        signed char* src = (signed char*)rawSounds[i].data;
        signed short* dst = (signed short*)newBuf;

        for (int s = 0; s < dstSamples; s++) {
            int srcIdx = Ftoid((double)s * rate / audioSpec.freq);
            if (srcIdx >= srcSamples) srcIdx = srcSamples - 1;
            short v = (short)((int)src[srcIdx] * 256);
            for (int c = 0; c < audioSpec.channels; c++)
                dst[s * audioSpec.channels + c] = v;
        }

        /* Swap under lock so AudioThread never sees a half-updated slot */
        LockAudio();
        if (sounds[se].data) free(sounds[se].data);
        sounds[se].data = newBuf;
        sounds[se].length = dstBytes;
        sounds[se].position = -1;
        UnlockAudio();
        return;
    }
    LOG_WARN("Cannot find sound %s!", name);
}

void resampleSounds() {
    for (int i = 0; i < nRawSounds; i++)
        resampleSound(i, rawSounds[i].name, 11025);
}

void freeSounds() {
    LockAudio();
    for (int i = SE::NONE; i < SE::MAX; i++) {
        if (sounds[i].data) { free(sounds[i].data); sounds[i].data = nullptr; }
    }
    UnlockAudio();
}

/* -----------------------------------------------------------------------
   playSound / isSoundPlaying / volume accessors
   ----------------------------------------------------------------------- */
void playSound(SE::Type index) {
    if (!soundsLoaded || index == SE::NONE) return;
    if (index < 0 || index >= SE::MAX)      return;
    if (!sounds[index].data)                return;
    sounds[index].position = 0;
}

bool isSoundPlaying(SE::Type index) {
    if (!soundsLoaded || index < 0 || index >= SE::MAX) return false;
    return (sounds[index].position > 0);
}

int  getSoundVolume() { return soundVolume; }
void setSoundVolume(int volume) {
    soundVolume = (volume < 0 ? 0 : volume > MAX_VOLUME ? MAX_VOLUME : volume);
}