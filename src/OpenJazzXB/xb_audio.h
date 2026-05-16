/**
 * xb_audio.h
 * XbJazz -- Xbox DirectSound audio backend bridge.
 * No <xtl.h> -- safe to include from sound.cpp.
 *
 * Architecture (XbTyrian pattern):
 *   DirectSoundCreate → ring buffer → AudioThread → calls XbAudioCallback
 *   CRITICAL_SECTION replaces SDL_LockAudio / SDL_UnlockAudio
 */
#ifndef XB_AUDIO_H
#define XB_AUDIO_H

typedef void (*XbAudioCallback)(unsigned char* stream, int len);

#ifdef __cplusplus
extern "C" {
#endif

	/* openAudio calls this after setting up the callback and audioSpec */
	bool XbAudioInit(int channels, int freq, int samples, XbAudioCallback cb);
	void XbAudioDeinit(void);
	void XbAudioLock(void);
	void XbAudioUnlock(void);

#ifdef __cplusplus
}
#endif

#endif