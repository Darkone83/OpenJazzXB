/**
 * unistd.h  (XbJazz stub)
 * POSIX header not available on Xbox / RXDK.
 * _getcwd doesn't exist in Xbox CRT -- return fixed game data path instead.
 */

#ifndef _UNISTD_H
#define _UNISTD_H

#include <io.h>      /* _access */

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

 /* access() -- maps to RXDK's _access */
static inline int access(const char* path, int mode) {
    return _access(path, mode);
}

/* getcwd() -- RXDK has no CWD concept; return "D:\" as safe default.
   file.cpp calls this when PathMgr::add() receives an empty-string path.
   On Xbox the real paths come from XboxPlatform::AddGamePaths(). */
static inline char* getcwd(char* buf, int size) {
    if (buf && size > 3) {
        buf[0] = 'D'; buf[1] = ':'; buf[2] = '\\'; buf[3] = '\0';
    }
    return buf;
}

/* isatty -- no terminal on Xbox */
static inline int isatty(int fd) { (void)fd; return 0; }

#endif /* _UNISTD_H */