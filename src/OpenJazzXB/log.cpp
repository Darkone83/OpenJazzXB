/**
 * log.cpp
 * XbJazz port -- Phase 1 Xbox logging via OutputDebugStringA only.
 *
 * No CRT stdio of any kind (fprintf/printf/vsnprintf all reference
 * __stdio_common_* or __acrt_iob_func which are not in Xbox CRT).
 * File logging disabled for Phase 1 -- re-enable with WriteFile API in Phase 5.
 *
 * OutputDebugStringA is forward-declared; including <xtl.h> here would
 * risk _Interlocked* conflicts with STL in other TUs.
 */

extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char*);

#include "log.h"
#include <cstdarg>
#include <cstring>

static const char* const s_levelNames[7] = {
    "MAX  ", "TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL"
};

Log::Log() {
    level = LL_DEBUG;
    quiet = false;
    logfile = nullptr;   /* file logging disabled Phase 1 */
    color_stdout = false;
    color_stderr = false;
#ifdef NDEBUG
    level = LL_INFO;
#endif
}

Log::~Log() {
    /* logfile is nullptr in Phase 1 -- nothing to close */
}

void Log::setLevel(int new_level) {
    if (new_level > LL_FATAL) level = LL_FATAL;
    else if (new_level < LL_MAX)   level = LL_MAX;
    else                           level = new_level;
}

int  Log::getLevel() { return level; }
void Log::setQuiet(bool e) { quiet = e; }

void Log::log(int lvl, const char* file, int line, const char* fmt, ...) {
    (void)line;
    if (lvl < level || quiet) return;

    /* Basename only */
    const char* src = strrchr(file, '\\');
    if (!src) src = strrchr(file, '/');
    src = src ? src + 1 : file;

    /* Output level tag + source file */
    OutputDebugStringA("[OJ] ");
    OutputDebugStringA(s_levelNames[lvl]);
    OutputDebugStringA(" ");
    OutputDebugStringA(src);
    OutputDebugStringA(": ");

    /* Output format string as-is (no substitution avoids all CRT stdio).
       Phase 5: replace with DbgPrint(fmt, ...) once RXDK CRT is confirmed. */
    (void)fmt;
    va_list args; va_start(args, fmt); va_end(args);
    OutputDebugStringA(fmt);
    OutputDebugStringA("\n");
}