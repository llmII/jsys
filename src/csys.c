/* Licensed as per $PROJECT_ROOT/LICENSE */

/* License and copyright for code derived from wahern/lunix
 * (https://github.com/wahern/lunix/blob/master/src/unix.c) is MIT and as
 * follows:
 *
 Copyright (c) 2014-2015  William Ahern <william@25thandClement.com>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to
 deal in the Software without restriction, including without limitation the
 rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 IN THE SOFTWARE.
 */

/*****************************************************************************
 * Includes for specific functions:
 *
 * Aftr this comment, each include should be like: `include <header> COMMENT`
 * COMMENT should include what functions come from that header that we want.
 *
 * Lines are 78 columns.
 ****************************************************************************/

/* There will be at least 3 modules:
 *   sys/nix      - all posixy stuff
 *   sys/windows/ - all win32
 *   sys/         - likely all in Janet, wrapping the necessary functions from
 *                - other sys submodules in an interface to work across
 *                - systems (or raise error for "not supported on this OS") */

/*============================================================================
 * Includes                                                                  *
 ===========================================================================*/
/* Windows not currently supported, but equivalents where it makes sense are
 * great in the sys/unified module */
#ifndef JANET_WINDOWS
#define _GNU_SOURCE    /* unhide fileno, dup3, chroot, others */
#include <errno.h>     /* errno */
#include <unistd.h>    /* chown(2) chroot(2) dup2(2) fork(2)
                        * setegid(2) seteuid(2) setgid(2)
                        * setuid(2) setsid(2) getpid(2) getppid(2) */
#include <fcntl.h>     /* F_GETLK F_SETLK - fcntl(2) open(2) */
#include <pwd.h>       /* struct passwd - getpwnam_r(3) */
#include <grp.h>       /* struct group - getgrnam(3) */
#include <stdio.h>     /* fileno(3) */
#else
#include <Windows.h>
#include <processthreadsapi.h> /* GetCurrentProcessID:getpid(2) */
#endif

#include <time.h>      /* strftime(3)  */
#include <janet.h>

/*============================================================================
 * Macros
 ===========================================================================*/
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define NOT_IMPL_ERR(name, janet_name)                      \
    "(" TOSTRING(janet_name)                                \
    ") is not implemented for this system. Erroring cfun <" \
    TOSTRING(name) ">"
/* NOTE: use a semicolon at the end of the usage of DEF_NOT_IMPL macro,
 * otherwise formatting breaks sometimes */
#define DEF_NOT_IMPL(name, janet_name)                              \
    JANET_FN(name, janet_name,                                      \
             "NOT IMPLEMENTED ON THIS SYSTEM") {                    \
        janet_panic(NOT_IMPL_ERR(name, janet_name));                \
        return janet_wrap_string(NOT_IMPL_ERR(name, janet_name));   \
    }
#define sys_errnof(error, ...)                                      \
    janet_panicf(error ", error: %s", __VA_ARGS__, strerror(errno))
#define sys_errno(error) janet_panicf(error ", error: %s", strerror(errno))

#ifndef JANET_WINDOWS
#define SYS_IMPL "nix"
#else
#define SYS_IMPL "windows"
#endif

#define SYS_UNAME(name) "sys/" SYS_IMPL "/" name
#define SYS_FUSAGE0(name) "(" SYS_UNAME(name) ")"
#define SYS_FUSAGE(name, rest) "(" SYS_UNAME(name) rest ")"
/* Definition for:
 *   GLIBC_PREREQ, HAVE_DUP3 __NetBSD_Prereq__, NETBSD_PREREQ, FREEBSD_PREREQ
 *   U_CLOEXEC, U_SYSFLAGS, U_FIXFLAGS, U_FDFLAGS, U_FLFLAGS,
 *   GETGRGID_R_NETBSD, U_GETPWUID_R_NETBSD, GETPWNAM_R_NETBSD
 *   GETGRNAM_R_NETBSD
 * from:
 *   https://github.com/wahern/lunix/blob/master/src/unix.c */
#ifndef GETGRGID_R_NETBSD
#define GETGRGID_R_NETBSD __NetBSD__
#endif

#ifndef GETPWUID_R_NETBSD
#define GETPWUID_R_NETBSD GETGRGID_R_NETBSD
#endif

#ifndef GETPWNAM_R_NETBSD
#define GETPWNAM_R_NETBSD GETGRGID_R_NETBSD
#endif

#ifndef GETGRNAM_R_NETBSD
#define GETGRNAM_R_NETBSD GETGRGID_R_NETBSD
#endif

#define UCLIBC_PREREQ(M, m, p)                              \
    (__UCLIBC__ &&                                          \
     (__UCLIBC_MAJOR__ > M ||                               \
      (__UCLIBC_MAJOR__ == M && __UCLIBC_MINOR__ > m) ||    \
      (__UCLIBC_MAJOR__ == M && __UCLIBC_MINOR__ == m       \
       && __UCLIBC_SUBLEVEL__ >= p)))

#ifndef __NetBSD_Prereq__
#define __NetBSD_Prereq__(M, m, p) 0
#endif

#if defined __GLIBC_PREREQ
#define GLIBC_PREREQ(M, m) (__GLIBC__ && __GLIBC_PREREQ(M, m) && !__UCLIBC__)
#else
#define GLIBC_PREREQ(M, m) 0
#endif

#define NETBSD_PREREQ(M, m) __NetBSD_Prereq__(M, m, 0)

#define FREEBSD_PREREQ(M, m)                                            \
    (__FreeBSD_version && __FreeBSD_version >= ((M) * 1000) + ((m) * 100))
#ifndef HAVE_DUP3
#define HAVE_DUP3                                                       \
    (GLIBC_PREREQ(2,9) || FREEBSD_PREREQ(10,0) || NETBSD_PREREQ(6,0)    \
     || UCLIBC_PREREQ(0,9,34))
#endif

#ifndef O_CLOEXEC
#define U_CLOEXEC (1LL << 32)
#else
#define U_CLOEXEC (O_CLOEXEC)
#endif

#define U_SYSFLAGS ((1LL << 32) - 1)
#define U_FIXFLAGS (U_CLOEXEC|O_NONBLOCK)
#define U_FDFLAGS  (U_CLOEXEC)  /* file descriptor flags */
#define U_FLFLAGS  (~U_FDFLAGS) /* file status flags */

/*============================================================================
 * Forward declarations                                                      *
 ===========================================================================*/
/* *nix: unistd.h, *: ? */
JANET_CFUN(cfun_chown);
JANET_CFUN(cfun_chroot);
JANET_CFUN(cfun_dup2);
JANET_CFUN(cfun_fork);
JANET_CFUN(cfun_setegid);
JANET_CFUN(cfun_seteuid);
JANET_CFUN(cfun_setgid);
JANET_CFUN(cfun_setuid);
JANET_CFUN(cfun_setsid);
/* windows: GetCurrentProcessID | processthreadsapi.h/Windows.h? */
JANET_CFUN(cfun_getpid);
JANET_CFUN(cfun_getppid);

/* *nix: fcntl.h, *: ? */
JANET_CFUN(cfun_fcntl);

/* *nix: pwd.h, *: ? */
JANET_CFUN(cfun_getpwnam);

/* *nix: grp.h, *: ? */
JANET_CFUN(cfun_getgrnam);

/* *nix: stdio.h *: ? */
JANET_CFUN(cfun_file_handle);
int file_to_fd(Janet *, int);

/* *nix: time.h *: ? */
JANET_CFUN(cfun_strftime);

/*============================================================================
 * Function definitions
 ===========================================================================*/
#ifndef JANET_WINDOWS /* *nix */
/* *nix: unistd.h, *: ? */
JANET_FN(cfun_chown, SYS_FUSAGE("chown", " uid gid path-or-file"),
         "-> _true|throws error_\n\n"
         "\t`uid`          **:number**\n\n"
         "\t`gid`          **:number**\n\n"
         "\t`path-or-file` **:string|:core/file**\n\n"
         "Changes the ownership of the file at `path-or-file` over to "
         "the user defined by `uid` and the group defined by `gid`."
) {
    janet_fixarity(argc, 3);

    uid_t uid = janet_getinteger(argv, 0);
    gid_t gid = janet_getinteger(argv, 1);

    if(janet_checktype(argv[0], JANET_ABSTRACT)) {
        int fd;
        if (-1 != (fd = file_to_fd(argv, 0))) {
            if (0 != fchown(fd, uid, gid)) {
                sys_errno("Failed to chown with file handle");
                return janet_wrap_boolean(0);
            }
        } else {
            janet_panic("Invalid file handle");
        }
    } else {
        const char *where = (char *)janet_getstring(argv, 0);
        if (0 != chown(where, uid, gid)) {
            sys_errnof("Failed to chown with path: %s", where);
            return janet_wrap_boolean(0);
        }
    }

    return janet_wrap_boolean(1);
}

JANET_FN(cfun_chroot, SYS_FUSAGE("chroot", " path"),
         "-> _true|throws error_\n\n"
         "\t`path` **:string**\n\n"
         "Changes the root (what is considered '/') to `path`.") {
    janet_fixarity(argc, 1);

    const char *where = (char *) janet_getstring(argv, 0);

    if (0 != chroot(where)) {
        sys_errnof("Failed to chroot to path: %s", where);
        return janet_wrap_boolean(0);
    }

    return janet_wrap_boolean(1);
}

/* Definition from:
 *   https://github.com/wahern/lunix/blob/master/src/unix.c */
static int u_getflags(int fd, int64_t *flags) {
    int _flags;

    if (-1 == (_flags = fcntl(fd, F_GETFL)))
        return errno;

    *flags = _flags;

    /* F_GETFL isn't defined to return O_CLOEXEC */
    if (!(*flags & U_CLOEXEC)) {
        if (-1 == (_flags = fcntl(fd, F_GETFD)))
            return errno;

        if (_flags & FD_CLOEXEC)
            *flags |= U_CLOEXEC;
    }

    return 0;
}

/* Definition from:
 *   https://github.com/wahern/lunix/blob/master/src/unix.c */
static int u_setflag(int fd, int64_t flag, int enable) {
    int flags;

    if (flag & U_CLOEXEC) {
        if (-1 == (flags = fcntl(fd, F_GETFD)))
            return errno;

        if (enable)
            flags |= FD_CLOEXEC;
        else
            flags &= ~FD_CLOEXEC;

        if (0 != fcntl(fd, F_SETFD, flags))
            return errno;
    } else {
        if (-1 == (flags = fcntl(fd, F_GETFL)))
            return errno;

        if (enable)
            flags |= flag;
        else
            flags &= ~flag;

        if (0 != fcntl(fd, F_SETFL, flags))
            return errno;
    }

    return 0;
}

/* Definition from:
 *   https://github.com/wahern/lunix/blob/master/src/unix.c */
static int u_fixflags(int fd, int64_t flags) {
    int64_t _flags = 0;
    int error;

    if (flags & U_FIXFLAGS) {
        if ((error = u_getflags(fd, &_flags)))
            return error;

        if ((flags & U_CLOEXEC) && !(_flags & U_CLOEXEC)) {
            if ((error = u_setflag(fd, U_CLOEXEC, 1)))
                return error;
        }

        if ((flags & O_NONBLOCK) && !(_flags & O_NONBLOCK)) {
            if ((error = u_setflag(fd, O_NONBLOCK, 1)))
                return error;
        }
    }

    return 0;
}

/* Definition from:
 *   https://github.com/wahern/lunix/blob/master/src/unix.c */
static int u_dup2(int fd, int fd2, int64_t flags) {
    int error;

    /*
     * NB: Set the file status flags first because we won't be able
     * roll-back state after dup'ing the descriptor.
     *
     * Why? On error we would want to close the new descriptor to avoid
     * a descriptor leak. But we wouldn't want to close the descriptor
     * if it was already open. (That would be unsafe for countless
     * reasons.) But there's no thread-safe way to know whether the
     * descriptor number was open or not at the time of the dup--that's
     * a classic TOCTTOU problem.
     *
     * Because the new descriptor will share the same file status flags,
     * the most robust thing to do is to try to set them first. Then
     * we'll set the file descriptor flags last and ignore any errors.
     * It would be odd if any implementation had a failure mode setting
     * descriptor flags, anyhow. (By contrast, status flags could easily
     * fail--O_NONBLOCK might be rejected for certain file types.)
     */
    if ((error = u_fixflags(fd, U_FLFLAGS & flags)))
        return error;

    flags &= ~U_FLFLAGS;

#if HAVE_DUP3
    if (-1 == dup3(fd, fd2, U_SYSFLAGS & flags))
        return errno;

    flags &= ~U_CLOEXEC; /* dup3 might not handle any other flags */
#elif defined F_DUP2FD && defined F_DUP2FD_CLOEXEC
    if (-1 == fcntl(fd, (flags & U_CLOEXEC)? F_DUP2FD_CLOEXEC : F_DUP2FD, fd2))
        return errno;

    flags &= ~U_CLOEXEC;
#else
    if (-1 == dup2(fd, fd2))
        return errno;
#endif

    (void)u_fixflags(fd2, flags);

    return 0;
}

/* Referred to:
 * https://github.com/wahern/lunix/blob/master/src/unix.c */
/* NOTE: only operate on int fds with this one, as it isn't inoften that this
 * be called with the purpose being to redirect stdout, stdin, and stderr,
 * well known descriptor numbers. `sys.janet` should provide a wrapper
 * handling path names or translation of JanetStreams to int fds, along with
 * handling pure int fds. */
/* NOTE: Janet side code is responsible for resetting (dyn :out|:in|:err) in
 * the event it redirects them! */
JANET_FN(cfun_dup2, SYS_FUSAGE("dup2", "fd-to fd-from"),
         "-> _true|throws error_\n\n"
         "\t`fd-to`   **:number**\n\n"
         "\t`fd-from` **:number** _optional_\n\n"
         "Redirects the descriptor identified in `fd-from` to the file "
         "identified in `fd-to`.") {
    janet_arity(argc, 2, 3);

    int32_t old = janet_getinteger(argv, 0);
    int32_t new = janet_getinteger(argv, 1);
    int64_t flags = janet_optinteger64(argv, argc, 2, 0);

    int err;
    if((err = u_dup2(old, new, flags))) {
        errno = err;
        sys_errno("Failed in call to dup2");
        return janet_wrap_boolean(0);
    }

    return janet_wrap_boolean(1);
}

JANET_FN(cfun_fork, SYS_FUSAGE0("fork"),
         "-> _:number pid|throws error_\n\n"
         "Creates a fork of this proccess.") {
    janet_fixarity(argc, 0);

    pid_t pid;

    if (-1 == (pid = fork())) {
        sys_errno("Failed to fork");
    }

    /* If we actually have an error, this shouldn't be reached... */
    return janet_wrap_integer(pid);
}

JANET_FN(cfun_setegid, SYS_FUSAGE("setegid", " gid"),
         "-> _true|throws error_\n\n"
         "\t`gid` **:number**\n\n"
         "Sets the effective operating group id of this process to `gid`.") {
    janet_fixarity(argc, 1);

    gid_t gid = janet_getinteger(argv, 0);

    if (0 != setegid(gid)) {
        sys_errnof("Failed to set effective group id: %d", gid);
        return janet_wrap_boolean(0);
    }

    return janet_wrap_boolean(1);
}

JANET_FN(cfun_seteuid, SYS_FUSAGE("seteuid", " uid"),
         "-> _true|throws error_\n\n"
         "\t`uid` **:number**\n\n"
         "Sets the effective operating user id of this process to `uid`.") {
    janet_fixarity(argc, 1);

    uid_t uid = janet_getinteger(argv, 0);

    if (0 != seteuid(uid)) {
        sys_errnof("Failed to set effective user id: %d", uid);
        return janet_wrap_boolean(0);
    }

    return janet_wrap_boolean(1);
}

JANET_FN(cfun_setgid, SYS_FUSAGE("setgid", " gid"),
         "-> _true|throws error_\n\n"
         "\t`gid` **:number**\n\n"
         "Sets the operating group id of this process to `gid`.") {
    janet_fixarity(argc, 1);

    gid_t gid = janet_getinteger(argv, 0);

    if (0 != setegid(gid)) {
        sys_errnof("Failed to set group id: %d", gid);
        return janet_wrap_boolean(0);
    }

    return janet_wrap_boolean(1);
}

JANET_FN(cfun_setuid, SYS_FUSAGE("setuid", " uid"),
         "-> _true|throws error_\n\n"
         "\t`uid` **:number**\n\n"
         "Sets the operating user id of this process to `uid`.") {
    janet_fixarity(argc, 1);

    uid_t uid = janet_getinteger(argv, 0);

    if (0 != seteuid(uid)) {
        sys_errnof("Failed to set user id: %d", uid);
        return janet_wrap_boolean(0);
    }

    return janet_wrap_boolean(1);
}

JANET_FN(cfun_setsid, SYS_FUSAGE0("setsid"),
         "-> _:number pid|throws error_\n\n"
         "Create a new session with no controlling terminal, becoming "
         "the session leader and process group leader of a new process "
         "group. Should definitely double check your setsid(2) man page.") {
    janet_fixarity(argc, 0);

    pid_t pid;

    if (-1 == (pid = setsid())) {
        sys_errno("Failed to become session leader");
        return janet_wrap_boolean(0);
    }

    return janet_wrap_integer(pid);
}

JANET_FN(cfun_getpid, SYS_FUSAGE0("getpid"),
         "-> _:number pid_\n\n"
         "Returns the PID of the current process.") {
    return janet_wrap_integer(getpid());
}

JANET_FN(cfun_getppid, SYS_FUSAGE0("getppid"),
         "-> _:number pid_\n\n"
         "Returns the PID of the parent of the current process.") {
    return janet_wrap_integer(getppid());
}

/* *nix: fcntl.h, *: ? */
/* Only supporting F_GETLK, F_SETLK, and F_SETLK at this moment
 * Operates on open file handles only (os/open) or (file/open)
 * Using fcntl for lock files exclusively (no other methods), it supports
 * getting the pid of the process holding the lock, which the other
 * interfaces may not. */
JANET_FN(cfun_fcntl, SYS_FUSAGE("fcntl", " file flag"),
         "-> _:number pid|true|throws error_\n\n"
         "\t`file` **:core/file**\n\n"
         "\t`flag` **:keyword** _:get-lock|:set-lock|:wait-lock_\n\n"
         "Only supporting file locking operations at this time, fnctl allows "
         "you to either lock, or wait to get lock, or figure out the process "
         "id currently holding the lock of the `file` dependent upon the "
         "keyword `flag` passed to this. For all operations excepting "
         "`:get-lock` returns true on success or throws an error on failure. "
         "For operation `:get-lock` returns the pid of the process holding "
         "the lock on success and throws an error on failure.") {
    janet_fixarity(argc, 2);

    if(janet_checktype(argv[0], JANET_ABSTRACT)) {
        int op = 0;
        int fd;
        if(-1 != (fd = file_to_fd(argv, 0))) {
            if (janet_keyeq(argv[1], "get-lock"))
                op = F_GETLK;
            else if (janet_keyeq(argv[1], "set-lock"))
                op = F_SETLK;
            else if (janet_keyeq(argv[1], "wait-lock"))
                op = F_SETLKW;
            else {
                janet_panic("Slot #2 must be a keyword equal to :get-lock "
                            "| :set-lock | :wait-lock");
                return janet_wrap_boolean(0);
            }
        } else {
            janet_panic("Invalid File Handle");
            return janet_wrap_boolean(0);
        }

        /* set up lock descriptor */
        struct flock ld = { 0 };
        ld.l_type = F_WRLCK;
        ld.l_whence = SEEK_SET;

        if (-1 == fcntl(fd, op, &ld)) {
            switch (op) {
            case F_GETLK:
                sys_errno("Failed to get pid of process"
                          " locking this file");
                break;
            case F_SETLK:
                sys_errno("Failed to acquire a file lock");
                break;
            case F_SETLKW:
                sys_errno("Failed while waiting to  acquire a file lock");
                break;
            }
        }

        switch (op) {
        case F_GETLK:
            return janet_wrap_integer(ld.l_pid);
            break;
        default:
            return janet_wrap_boolean(1);
        }
    } else {
        janet_panic("Slot #1 must be a valid File Handle");
        return janet_wrap_boolean(0);
    }

    return janet_wrap_boolean(1);
}

/* *nix: pwd.h, *: ? */
/* uses janet struct for a thing with fields... */
/* Definition from:
 *   https://github.com/wahern/lunix/blob/master/src/unix.c */
static int u_getpwuid_r(
    uid_t uid, struct passwd *pwd, char *buf, size_t bufsiz,
    struct passwd **res) {
#if GETPWUID_R_NETBSD
    int error;
    errno = 0;
    return (error = getpwuid_r(uid, pwd, buf, bufsiz, res))? error : errno;
#else
    return getpwuid_r(uid, pwd, buf, bufsiz, res);
#endif
}

/* Definition from:
 *   https://github.com/wahern/lunix/blob/master/src/unix.c */
static int u_getpwnam_r(
    const char *nam, struct passwd *pwd, char *buf,
    size_t bufsiz, struct passwd **res) {
#if GETPWNAM_R_NETBSD
    int error;
    errno = 0;
    return (error = getpwnam_r(nam, pwd, buf, bufsiz, res))? error : errno;
#else
    return getpwnam_r(nam, pwd, buf, bufsiz, res);
#endif
}

static int sys_getpwuid(
    uid_t uid, struct passwd **ent, char *buf, size_t len) {
    int err;
    struct passwd tmp;

    while((err = u_getpwuid_r(uid, &tmp, buf, len, ent))) {
        if (err != ERANGE)
            return err;
        if (janet_srealloc(buf, sizeof(char) * (len + 128)))
            return err;

        len += 128;
        /* TODO: Limit to a reasonable maximal bound */
        *ent = NULL;
    }

    return 0;
}

static int sys_getpwnam(
    const char* user, struct passwd **ent, char *buf, size_t len) {
    int err;
    struct passwd tmp;

    while((err = u_getpwnam_r(user, &tmp, buf, len, ent))) {
        if (err != ERANGE)
            return err;
        if (janet_srealloc(buf, sizeof(char) * (len + 128)))
            return err;

        len += 128;
        /* TODO: Limit to a reasonable maximal bound */
        *ent = NULL;
    }

    return 0;
}

/* Definition from:
 *   https://github.com/wahern/lunix/blob/master/src/unix.c */
static int u_getgrgid_r(
    gid_t gid, struct group *grp, char *buf, size_t bufsiz,
    struct group **res) {
#if GETGRGID_R_NETBSD
	int error;
	errno = 0;
	return (error = getgrgid_r(gid, grp, buf, bufsiz, res))? error : errno;
#else
	return getgrgid_r(gid, grp, buf, bufsiz, res);
#endif
}

/* Definition from:
 *   https://github.com/wahern/lunix/blob/master/src/unix.c */
static int u_getgrnam_r(
    const char *nam, struct group *grp, char *buf, size_t bufsiz,
    struct group **res) {
#if GETGRNAM_R_NETBSD
	int error;
	errno = 0;
	return (error = getgrnam_r(nam, grp, buf, bufsiz, res))? error : errno;
#else
	return getgrnam_r(nam, grp, buf, bufsiz, res);
#endif
}

static int sys_getgrid(
    gid_t gid, struct group **ent, char *buf, size_t len) {
    int err;
    struct group tmp;

    while ((err = u_getgrgid_r(gid, &tmp, buf, len, ent))) {
        if (err != ERANGE)
            return err;
        if (janet_srealloc(buf, sizeof(char) * (len + 128)))
            return err;

        len += 128;
        /* TODO: Limit to a reasonable maximal bound */
        *ent = NULL;
    }

    return 0;
}

static int sys_getgrnam(
    const char *name, struct group **ent, char *buf, size_t len) {
    int err;
    struct group tmp;

    while ((err = u_getgrnam_r(name, &tmp, buf, len, ent))) {
        if (err != ERANGE)
            return err;
        if (janet_srealloc(buf, sizeof(char) * (len + 128)))
            return err;

        len += 128;
        /* TODO: Limit to a reasonable maximal bound */
        *ent = NULL;
    }

    return 0;
}

int sys_name_opt(Janet *argv) {
    return janet_checktype(argv[0], JANET_NUMBER) ? 1 : 0;
}

/* TODO: maybe a nicer doc string? */
JANET_FN(cfun_getpwnam, SYS_FUSAGE0("getpwnam id-or-username"),
         "-> _:struct user-details|throws error\n\n"
         "\t`id-or-username` **:number|:string**\n\n"
         "\t**user-details** {:user-name `:string` :password `:string` "
         ":user-id `:number` :group-id `:number` :home-directory `:string` "
         ":shell `:string` :gecos `:string`}\n\n"
         "Gets the details on a username specified by id or by username with "
         "`id-or-username`.") {
    janet_fixarity(argc, 1);

    struct passwd *ent;
    char  *buf = (char *)janet_smalloc(sizeof(char) * 128);

    /* did we get an ID? or a Name? */
    switch(sys_name_opt(argv)) {
        case 1: /* is int */
            if(0 != sys_getpwuid(janet_getinteger(argv, 0), &ent, buf, 128)) {
                sys_errnof("No entry for uid: %d", janet_getinteger(argv, 0));
                return janet_wrap_boolean(0);
            }
            break;
        case 0: /* is string */
            if(0 != sys_getpwnam(janet_getcstring(argv, 0), &ent, buf, 128)) {
                sys_errnof("No entry for username: %s",
                           janet_getcstring(argv, 0));
                return janet_wrap_boolean(0);
            }
            break;
    }

    /* extract info */
    /* TODO: support the time_t fields? */
    JanetKV *ret = janet_struct_begin(7);
    janet_struct_put(ret,
                     janet_ckeywordv("user-name"),
                     janet_wrap_string(janet_cstring(ent->pw_name)));
    janet_struct_put(ret,
                     janet_ckeywordv("password"),
                     janet_wrap_string(janet_cstring(ent->pw_passwd)));
    janet_struct_put(ret,
                     janet_ckeywordv("user-id"),
                     janet_wrap_integer(ent->pw_uid));
    janet_struct_put(ret,
                     janet_ckeywordv("group-id"),
                     janet_wrap_integer(ent->pw_gid));
    janet_struct_put(ret,
                     janet_ckeywordv("home-directory"),
                     janet_wrap_string(janet_cstring(ent->pw_dir)));
    janet_struct_put(ret,
                     janet_ckeywordv("shell"),
                     janet_wrap_string(janet_cstring(ent->pw_shell)));
    janet_struct_put(ret,
                     janet_ckeywordv("gecos"),
                     janet_wrap_string(janet_cstring(ent->pw_gecos)));

    /* copy out above! */
    janet_sfree(buf);

    return janet_wrap_struct(janet_struct_end(ret));
}

/* *nix: grp.h, *: ? */
JANET_FN(cfun_getgrnam, SYS_FUSAGE("getgrnam", " id-or-groupname"),
         "-> _:struct group-details|thows error_\n\n"
         "\t`id-or-groupname` **:number|:string**\n\n"
         "\t**user-details** {:group-name `:string` :password `:string` "
         ":group-id `:number`}\n\n"
         "Gets the details on a group specified by id or by group name with "
         "`id-or-username`") {
    janet_fixarity(argc, 1);

    struct group *ent;
    char  *buf = (char *)janet_smalloc(sizeof(char) * 128);

    /* did we get an ID? or a Name? */
    switch(sys_name_opt(argv)) {
        case 1: /* is int */
            if(0 != sys_getgrid(janet_getinteger(argv, 0), &ent, buf, 128)) {
                sys_errnof("No entry for uid: %d", janet_getinteger(argv, 0));
                return janet_wrap_boolean(0);
            }
            break;
        case 0: /* is string */
            if(0 != sys_getgrnam(janet_getcstring(argv, 0), &ent, buf, 128)) {
                sys_errnof("No entry for username: %s",
                           janet_getcstring(argv, 0));
                return janet_wrap_boolean(0);
            }
            break;
    }

    /* extract info */
    /* TODO: support the time_t fields? */
    JanetKV *ret = janet_struct_begin(4);
    janet_struct_put(ret,
                     janet_ckeywordv("group-name"),
                     janet_wrap_string(janet_cstring(ent->gr_name)));
    janet_struct_put(ret,
                     janet_ckeywordv("password"),
                     janet_wrap_string(janet_cstring(ent->gr_passwd)));
    janet_struct_put(ret,
                     janet_ckeywordv("group-id"),
                     janet_wrap_integer(ent->gr_gid));

    /* handle members as tuple */
    int len = 0;
    for(; ent->gr_mem[len]; len++)
        ;;

    Janet *mem = janet_malloc(sizeof(Janet) * len);

    for(int i = 0; ent->gr_mem[i]; i++) {
       mem[i] = janet_wrap_string(janet_cstring(ent->gr_mem[i]));
    }

    janet_struct_put(ret,
                    janet_ckeywordv("group-members"),
                    janet_wrap_tuple(janet_tuple_n(mem, len)));

    const JanetStruct r = janet_struct_end(ret);

    /* copy out above! */
    janet_sfree(buf);

    return janet_wrap_struct(r);
}

/* nix: stdio.h, *: ?*/
int file_to_fd(Janet *argv, int idx) {
    if(janet_checkfile(argv[idx]))
        return fileno(janet_unwrapfile(argv[idx], NULL));
    return -1;
}

JANET_FN(cfun_fileno, SYS_FUSAGE("fileno", " file"),
         "-> _:number|throws error_\n\n"
         "\t`file` **:core/file**\n\n"
         "Returns the integer file descriptor from a :core/file.") {
    int ret;
    if (-1 != (ret = file_to_fd(argv , 0)))
        return janet_wrap_integer(ret);

    janet_panic("Invalid File Handle");
    return janet_wrap_boolean(0);
}

static int date_struct_getint(JanetStruct date, char *field) {
    Janet f = janet_struct_get(date, janet_ckeywordv(field));

    if (janet_checktype(f, JANET_NIL)) return 0;

    return (int)janet_unwrap_number(f);
}

void date_struct_to_tm(JanetStruct dt, struct tm *date) {
    date->tm_isdst = janet_unwrap_boolean(
        janet_struct_get(dt, janet_wrap_keyword("dst")));;

    date->tm_hour = date_struct_getint(dt, "hours");
    date->tm_min = date_struct_getint(dt, "minutes");
    date->tm_mon = date_struct_getint(dt, "month");
    date->tm_mday = date_struct_getint(dt, "month-day") + 1;
    date->tm_sec = date_struct_getint(dt, "seconds");
    date->tm_year = date_struct_getint(dt, "year") - 1900;
}

#else /* Windows */
/* *nix: unistd.h, *: ? */
DEF_NOT_IMPL(cfun_chown, "sys/windows/chown");
DEF_NOT_IMPL(cfun_chroot, "sys/windows/chroot");
DEF_NOT_IMPL(cfun_dup2, "sys/windows/dup2");
DEF_NOT_IMPL(cfun_fork, "sys/windows/fork");
DEF_NOT_IMPL(cfun_setegid, "sys/windows/setegid");
DEF_NOT_IMPL(cfun_seteuid, "sys/windows/seteuid");
DEF_NOT_IMPL(cfun_setgid, "sys/windows/setgid");
DEF_NOT_IMPL(cfun_setuid, "sys/windows/setuid");
DEF_NOT_IMPL(cfun_setsid, "sys/windows/setsid");
JANET_FN(cfun_getpid, SYS_FUSAGE0("getpid"),
         "-> _:number pid_\n\n"
         "Returns the PID of the current process.") {
    return janet_wrap_integer(GetCurrentProcessId());
}
DEF_NOT_IMPL(cfun_getppid, "sys/windows/getppid");

/* *nix: fcntl.h, *: ? */
DEF_NOT_IMPL(cfun_fcntl, "sys/windows/fcntl");

/* *nix: pwd.h, *: ? */
DEF_NOT_IMPL(cfun_getpwnam, "sys/windows/getpwnam");

/* *nix: grp.h, *: ? */
DEF_NOT_IMPL(cfun_getgrnam, "sys/windows/getgrnam");

/* *nix: stdio.h, *: ? */
DEF_NOT_IMPL(cfun_fileno, "sys/windows/fileno");

/* TODO: Definitely implement this! */
/* *nix: time.h, *: ? */
DEF_NOT_IMPL(cfun_strftime, "sys/windows/strftime");
#endif

/* *: time.h */
JANET_FN(cfun_strftime, SYS_FUSAGE("strftime", "date format"),
          "-> _:string|throws error_\n\n"
          "\t`date` **:struct**\n\n"
          "\t`format` **:string**\n\n"
          "With a struct compliant to Janet's `(os/date)` return a formatted "
          "time string.") {
    janet_fixarity(argc, 2);

    size_t      curr_max = 64, size = 0;
    JanetBuffer *datestr = janet_buffer(curr_max);
    JanetString format = janet_getstring(argv, 1);
    struct tm   date;

    /* extract the date */
    date_struct_to_tm(janet_getstruct(argv, 0), &date);

    do {
        janet_buffer_ensure(datestr, curr_max, 1);
        size = strftime((char *) datestr->data, curr_max - 1,
                        (const char *) format, &date);
        curr_max += 64;
        /* Stop trying at 4kb and error instead! */
    } while (0 == size && curr_max < 4097);

    if (curr_max > 4096 || size == 0)
        janet_panic("Failed to turn date into string.");

    return janet_wrap_string(janet_cstring((char *)datestr->data));
}

/*============================================================================
 * Export functions
 ===========================================================================*/
/* Don't bother exporting functions for one platform as unimplemented on
 * another platform */
JANET_MODULE_ENTRY(JanetTable *env) {
    janet_cfuns_ext(env, "sys", (JanetRegExt[]) {
        /* *nix: unistd.h, *: ? */
        JANET_REG(SYS_IMPL "/chown", cfun_chown),
        JANET_REG(SYS_IMPL "/chroot", cfun_chroot),
        JANET_REG(SYS_IMPL "/dup2", cfun_dup2),
        JANET_REG(SYS_IMPL "/fork", cfun_fork),
        JANET_REG(SYS_IMPL "/setegid", cfun_setegid),
        JANET_REG(SYS_IMPL "/seteuid", cfun_seteuid),
        JANET_REG(SYS_IMPL "/setgid", cfun_setgid),
        JANET_REG(SYS_IMPL "/setuid", cfun_setuid),
        JANET_REG(SYS_IMPL "/setsid", cfun_setsid),
        JANET_REG(SYS_IMPL "/getpid", cfun_getpid),
        JANET_REG(SYS_IMPL "/getppid", cfun_getppid),

        /* *nix: fcntl.h, *: ? */
        JANET_REG(SYS_IMPL "/fcntl", cfun_fcntl),

        /* *nix: pwd.h, *: ? */
        JANET_REG(SYS_IMPL "/getpwnam", cfun_getpwnam),

        /* *nix: grp.h, *: ? */
        JANET_REG(SYS_IMPL "/getgrnam", cfun_getgrnam),

        /* *nix: stdio.h, *: ? */
        JANET_REG(SYS_IMPL "/fileno", cfun_fileno),

        /* *nix: time.h, *: ? */
        JANET_REG(SYS_IMPL "/strftime", cfun_strftime),
        JANET_REG_END
    });
}
