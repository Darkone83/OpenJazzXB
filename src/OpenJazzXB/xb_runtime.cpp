/**
 * xb_runtime.cpp
 * XbJazz -- C++ runtime stubs not provided by RXDK in current project config.
 *
 * -----------------------------------------------------------------------
 * PHASE 1 ONLY -- READ BEFORE PHASE 2
 * -----------------------------------------------------------------------
 * __CxxFrameHandler3 is stubbed here so the project links for Phase 1.
 * With this stub, C++ exception handling (throw/catch) does NOT work at
 * runtime.  Before Phase 2 hardware testing:
 *   1. Open XbDiag project -> Linker -> Input -> Additional Dependencies
 *   2. Find the .lib that provides __CxxFrameHandler3 (vccrt.lib etc.)
 *   3. Add it to OpenJazzXB's linker additional dependencies
 *   4. Remove the __CxxFrameHandler3 stub below
 * -----------------------------------------------------------------------
 */

 /* -----------------------------------------------------------------------
    __ftol2_sse -- float/double to integer conversion
    MSVC generates calls to this for (int)floatValue casts.
    Xbox RXDK runtime does not provide it.
    Implementation: x87 FPU truncation via fistp (no SSE required).
    Takes ST(0) from the FPU stack, returns 64-bit result in EDX:EAX.
    ----------------------------------------------------------------------- */
    /* __ftol2_sse -- matches XbTyrian crt_compat.cpp exactly.
       ST(0) holds the float when the compiler calls this function.
       fistp pops ST(0) and stores as 32-bit int; return in EAX. */
extern "C" long __cdecl __ftol2_sse()
{
    long result;
    __asm fistp dword ptr result
    return result;
}
/* /alternatename maps the bare CRT name __ftol2_sse to our __cdecl
 * decorated symbol ___ftol2_sse. Needed for XBE static linking
 * (/export creates a DLL export entry, not a symbol alias). */
#pragma comment(linker, "/alternatename:__ftol2_sse=___ftol2_sse")

 /* -----------------------------------------------------------------------
    C++14 sized delete operators -- not in RXDK runtime
    ----------------------------------------------------------------------- */
void __cdecl operator delete  (void* p, unsigned int) { operator delete(p); }
void __cdecl operator delete[](void* p, unsigned int) { operator delete[](p); }

/* -----------------------------------------------------------------------
   __std_terminate
   ----------------------------------------------------------------------- */
extern "C" void __cdecl __std_terminate() {
    *((volatile int*)0) = 0xDEAD;
    for (;;) {}
}

/* -----------------------------------------------------------------------
   __std_exception_copy / __std_exception_destroy
   OJ uses int exceptions, not std::exception -- safe no-ops.
   ----------------------------------------------------------------------- */
extern "C" void __cdecl __std_exception_copy(const void*, void*) {}
extern "C" void __cdecl __std_exception_destroy(void*) {}

/* -----------------------------------------------------------------------
   std::_Xlength_error
   ----------------------------------------------------------------------- */
namespace std {
    void __cdecl _Xlength_error(char const*) {
        *((volatile int*)0) = 0xBAD1;
        for (;;) {}
    }
}

/* -----------------------------------------------------------------------
   eh vector constructor / destructor iterators
   ----------------------------------------------------------------------- */
extern "C" void __stdcall __ehvec_ctor(
    void* arr, unsigned int sz, unsigned int cnt,
    void(__thiscall* ctor)(void*), void(__thiscall*)(void*))
{
    unsigned char* p = (unsigned char*)arr;
    for (unsigned int i = 0; i < cnt; i++, p += sz) ctor(p);
}

extern "C" void __stdcall __ehvec_dtor(
    void* arr, unsigned int sz, unsigned int cnt,
    void(__thiscall* dtor)(void*))
{
    unsigned char* p = (unsigned char*)arr + sz * cnt;
    for (unsigned int i = cnt; i > 0; i--) { p -= sz; dtor(p); }
}

#pragma comment(linker, "/alternatename:??_L@YGXPAXIIP6EX0@Z1@Z=__ehvec_ctor@20")
#pragma comment(linker, "/alternatename:??_M@YGXPAXIIP6EX0@Z@Z=__ehvec_dtor@16")

/* -----------------------------------------------------------------------
   __CxxFrameHandler3 -- STUB
   With this stub throw/catch does not work at runtime.
   ----------------------------------------------------------------------- */
extern "C" int __cdecl __CxxFrameHandler3(void*, void*, void*, void*) {
    return 1;   /* ExceptionContinueSearch */
}

/* -----------------------------------------------------------------------
   POSIX stubs -- _getcwd / _access
   MSVC headers define getcwd/access as INLINE wrappers around _getcwd/_access,
   so we must stub the underscore versions to intercept the actual calls.
   ----------------------------------------------------------------------- */

   /* _getcwd: Xbox CWD = XBE root = D:\ */
extern "C" char* __cdecl _getcwd(char* buf, int size) {
    if (buf && size >= 4) {
        buf[0] = 'D'; buf[1] = ':'; buf[2] = '\\'; buf[3] = '\0';
    }
    return buf;
}

/* _access: path accessibility check.
   R_OK (4) = readable  -- always true on Xbox (DVD + HDD both readable).
   W_OK (2) = writable  -- only T:\ and E:\ are writable; D:\ is DVD.
   Returns 0 = accessible, -1 = not accessible. */
#ifndef W_OK
#define W_OK 2
#define R_OK 4
#endif
extern "C" int __cdecl _access(const char* path, int mode) {
    if (!path) return -1;
    if (mode & W_OK) {
        if (path[0] == 'T' || path[0] == 't') return 0;
        if (path[0] == 'E' || path[0] == 'e') return 0;
        return -1;
    }
    return 0;
}