/**
 * sound.h
 * XbJazz port — SDL audio headers replaced.
 * Interface identical to original.
 *
 * Original copyright: Copyright (c) 2005-2010 AJ Thomson
 */

#ifndef OJ_SOUND_H
#define OJ_SOUND_H

#include "OpenJazz.h"

 /* -----------------------------------------------------------------------
    Sound effect indices
    ----------------------------------------------------------------------- */
namespace SE {
    enum Type {
        NONE = 0,
        INVULN,
        MACHGUN,
        BOOM,
        OW,
        YUM,
        FIRE,
        UPLOOP,
        ONEUP,
        PHOTON,
        WAIT,
        ORB,
        JUMPA,
        GODLIKE,
        YEAHOO,
        BIRDY,
        FLAMER,
        ELECTR,
        SPRING,
        ROCKET,
        STOP,
        BLOCK,
        CUSTOM_22,
        CUSTOM_23,
        CUSTOM_24,
        CUSTOM_25,
        CUSTOM_26,
        CUSTOM_27,
        CUSTOM_28,
        CUSTOM_29,
        CUSTOM_30,
        CUSTOM_31,
        CUSTOM_32,
        MAX
    };
}

#define MAX_VOLUME 100

/* MusicTempo — replaces enum class MusicTempo for MSVC 2003 */
struct MusicTempo {
    enum _T { NORMAL = 128, FAST = 80 };
    _T val;
    MusicTempo() : val(NORMAL) {}
    MusicTempo(_T v) : val(v) {}
    operator _T()         const { return val; }
    bool operator==(const MusicTempo& o) const { return val == o.val; }
    bool operator!=(_T v)         const { return val != v; }
    bool operator==(_T v)         const { return val == v; }
};

/* -----------------------------------------------------------------------
   Audio functions — implemented in sound.cpp (Phase 1: all no-ops)
   ----------------------------------------------------------------------- */
void openAudio();
void closeAudio();
void playMusic(const char* fileName, bool restart = false);
void pauseMusic(bool pause);
void stopMusic();
int  getMusicVolume();
void setMusicVolume(int volume);
MusicTempo getMusicTempo();
void setMusicTempo(MusicTempo tempo);
bool loadSounds(const char* fileName);
void resampleSound(int index, const char* name, int rate);
void resampleSounds();
void freeSounds();
void playSound(SE::Type se);
bool isSoundPlaying(SE::Type se);
int  getSoundVolume();
void setSoundVolume(int volume);

inline void playConfirmSound() { playSound(SE::ORB); }
inline bool isValidSoundIndex(SE::Type index) {
    return (index >= SE::NONE && index < SE::MAX);
}

#endif /* OJ_SOUND_H */