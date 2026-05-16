/**
 * xb_audio.cpp
 * XbJazz -- Xbox DirectSound streaming audio backend.
 *
 * ISOLATION RULE: <xtl.h> + <dsound.h> only from Xbox side.
 * OJ's audioCallback (psmplug + SFX mix) runs in the audio thread here.
 *
 * Based directly on XbTyrian loudness.cpp pattern:
 *   - RING_BUFFER_COUNT chunks of CALLBACK_BYTES each, looping
 *   - AudioThread tracks play cursor, fills completed chunks
 *   - CRITICAL_SECTION serialises callback vs main-thread lock/unlock
 */

#include <xtl.h>
#include <dsound.h>
#include "xb_audio.h"

 /* -----------------------------------------------------------------------
    Ring buffer geometry  (matches XbTyrian)
    ----------------------------------------------------------------------- */
#define RING_BUFFER_COUNT   4
static int s_callbackBytes = 0;   /* set in XbAudioInit */
static int s_ringBytes = 0;

/* -----------------------------------------------------------------------
   State
   ----------------------------------------------------------------------- */
static LPDIRECTSOUND        s_pDS = NULL;
static LPDIRECTSOUNDBUFFER  s_pBuf = NULL;
static HANDLE               s_hThread = NULL;
static HANDLE               s_hStop = NULL;
static CRITICAL_SECTION     s_cs;
static bool                 s_csInit = false;
static DWORD                s_writeChunk = 0;

static XbAudioCallback      s_callback = NULL;

/* -----------------------------------------------------------------------
   AudioThread -- feeds the DirectSound ring buffer (XbTyrian pattern)
   ----------------------------------------------------------------------- */
static DWORD WINAPI AudioThread(LPVOID)
{
    while (WaitForSingleObject(s_hStop, 2) == WAIT_TIMEOUT)
    {
        DWORD playCursor = 0, writeCursor = 0;
        if (FAILED(s_pBuf->GetCurrentPosition(&playCursor, &writeCursor)))
            continue;

        const DWORD playChunk = playCursor / (DWORD)s_callbackBytes;

        while (s_writeChunk != playChunk)
        {
            void* ptr1 = NULL, * ptr2 = NULL;
            DWORD  bytes1 = 0, bytes2 = 0;
            DWORD  writePos = s_writeChunk * (DWORD)s_callbackBytes;

            HRESULT hr = s_pBuf->Lock(writePos, (DWORD)s_callbackBytes,
                &ptr1, &bytes1, &ptr2, &bytes2, 0);
            if (FAILED(hr)) break;

            EnterCriticalSection(&s_cs);
            if (s_callback)
            {
                s_callback((unsigned char*)ptr1, (int)bytes1);
                if (ptr2 && bytes2)
                    s_callback((unsigned char*)ptr2, (int)bytes2);
            }
            else
            {
                memset(ptr1, 0, bytes1);
                if (ptr2) memset(ptr2, 0, bytes2);
            }
            LeaveCriticalSection(&s_cs);

            s_pBuf->Unlock(ptr1, bytes1, ptr2, bytes2);
            s_writeChunk = (s_writeChunk + 1) % (DWORD)RING_BUFFER_COUNT;
        }

        Sleep(2);
    }
    return 0;
}

/* -----------------------------------------------------------------------
   XbAudioInit
   ----------------------------------------------------------------------- */
bool XbAudioInit(int channels, int freq, int samples, XbAudioCallback cb)
{
    s_callback = cb;

    /* bytes per callback chunk: samples * channels * 2 (S16) */
    s_callbackBytes = samples * channels * 2;
    s_ringBytes = s_callbackBytes * RING_BUFFER_COUNT;

    InitializeCriticalSection(&s_cs);
    s_csInit = true;

    /* ---- DirectSound device ---- */
    if (FAILED(DirectSoundCreate(NULL, &s_pDS, NULL)))
    {
        OutputDebugStringA("XbJazz audio: DirectSoundCreate failed\n");
        return false;
    }

    /* ---- Streaming buffer ---- */
    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = (WORD)channels;
    wfx.nSamplesPerSec = (DWORD)freq;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    DSBUFFERDESC dsbd;
    memset(&dsbd, 0, sizeof(dsbd));
    dsbd.dwSize = sizeof(dsbd);
    dsbd.dwFlags = 0;           /* Xbox: 0, not DSBCAPS_LOCSOFTWARE */
    dsbd.dwBufferBytes = (DWORD)s_ringBytes;
    dsbd.lpwfxFormat = &wfx;

    if (FAILED(s_pDS->CreateSoundBuffer(&dsbd, &s_pBuf, NULL)))
    {
        OutputDebugStringA("XbJazz audio: CreateSoundBuffer failed\n");
        s_pDS->Release(); s_pDS = NULL;
        return false;
    }

    /* ---- Pre-fill ring with generated audio (avoids initial choppy burst) ---- */
    void* ptr1 = NULL, * ptr2 = NULL;
    DWORD bytes1 = 0, bytes2 = 0;
    if (SUCCEEDED(s_pBuf->Lock(0, (DWORD)s_ringBytes,
        &ptr1, &bytes1, &ptr2, &bytes2, 0)))
    {
        EnterCriticalSection(&s_cs);
        if (s_callback)
        {
            s_callback((unsigned char*)ptr1, (int)bytes1);
            if (ptr2 && bytes2)
                s_callback((unsigned char*)ptr2, (int)bytes2);
        }
        else
        {
            memset(ptr1, 0, bytes1);
            if (ptr2) memset(ptr2, 0, bytes2);
        }
        LeaveCriticalSection(&s_cs);
        s_pBuf->Unlock(ptr1, bytes1, ptr2, bytes2);
    }
    s_writeChunk = 0;

    /* ---- Start looping playback ---- */
    s_pBuf->Play(0, 0, DSBPLAY_LOOPING);

    /* ---- Launch audio thread at high priority ---- */
    s_hStop = CreateEvent(NULL, TRUE, FALSE, NULL);
    s_hThread = CreateThread(NULL, 0, AudioThread, NULL, 0, NULL);
    if (s_hThread)
        SetThreadPriority(s_hThread, THREAD_PRIORITY_HIGHEST);

    OutputDebugStringA("XbJazz audio: DirectSound streaming started\n");
    return true;
}

/* -----------------------------------------------------------------------
   XbAudioDeinit
   ----------------------------------------------------------------------- */
void XbAudioDeinit(void)
{
    if (s_hStop)
    {
        SetEvent(s_hStop);
        if (s_hThread)
        {
            WaitForSingleObject(s_hThread, 2000);
            CloseHandle(s_hThread); s_hThread = NULL;
        }
        CloseHandle(s_hStop); s_hStop = NULL;
    }

    if (s_pBuf) { s_pBuf->Stop(); s_pBuf->Release(); s_pBuf = NULL; }
    if (s_pDS) { s_pDS->Release();  s_pDS = NULL; }
    if (s_csInit) { DeleteCriticalSection(&s_cs); s_csInit = false; }
}

/* -----------------------------------------------------------------------
   XbAudioLock / XbAudioUnlock  (replaces SDL_LockAudio / SDL_UnlockAudio)
   ----------------------------------------------------------------------- */
void XbAudioLock(void) { if (s_csInit) EnterCriticalSection(&s_cs); }
void XbAudioUnlock(void) { if (s_csInit) LeaveCriticalSection(&s_cs); }