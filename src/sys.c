#include <janet.h>

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
#include <errno.h>     /* errno */
#include <unistd.h>    /* chdir(2) chown(2) chroot(2) dup2(2) fork(2)
                        * setegid(2) seteuid(2) setgid(2)
                        * setuid(2) setsid(2)  */
#include <fcntl.h>     /* F_GETLK F_SETLK - fcntl(2) open(2) */
#include <pwd.h>       /* struct passwd - getpwnam_r(3) */
#include <grp.h>       /* struct group - getgrnam(3) */
#endif

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
#define SYS_FUSAGE(name, ...) "(" SYS_UNAME(name) __VA_ARG__ ")"
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

/* TODO: Trim down, we only care about *BSD, MacOS, Linux (glibc, musl), and
 * Windows! */
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
JANET_CFUN(cfun_chdir);
JANET_CFUN(cfun_chown);
JANET_CFUN(cfun_chroot);
JANET_CFUN(cfun_dup2);
JANET_CFUN(cfun_fork);
JANET_CFUN(cfun_setegid);
JANET_CFUN(cfun_seteuid);
JANET_CFUN(cfun_setgid);
JANET_CFUN(cfun_setuid);
JANET_CFUN(cfun_setsid);

/* *nix: fcntl.h, *: ? */
JANET_CFUN(cfun_fcntl);

/* *nix: pwd.h, *: ? */
JANET_CFUN(cfun_getpwnam);

/* *nix: grp.h, *: ? */
JANET_CFUN(cfun_getgrnam);

/*============================================================================
 * Function definitions
 ===========================================================================*/
#ifndef JANET_WINDOWS /* *nix */
/* *nix: unistd.h, *: ? */
JANET_FN(cfun_chdir, SYS_FUSAGE0("chdir"),
         "DOCUMENT ME") {
    janet_fixarity(argc, 1);

    if(janet_checktype(argv[0], JANET_ABSTRACT)) {
        FILE *f = ((JanetFile *)
                   janet_getabstract(argv, 0, &janet_file_type))->file;
        if (0 != fchdir(fileno(f))) {
            sys_errno("Failed to chdir with file handle");
            return janet_wrap_boolean(0);
        }
    } else {
        const char *where = (char *)janet_getstring(argv, 0);
        if (0 != chdir(where)) {
            sys_errnof("Failed to chdir with path: %s", where);
            return janet_wrap_boolean(0);
        }
    }

    return janet_wrap_boolean(1);
}

JANET_FN(cfun_chown, SYS_FUSAGE0("chown"),
         "DOCUMENT ME") {
    janet_fixarity(argc, 3);

    uid_t uid = janet_getinteger(argv, 0);
    gid_t gid = janet_getinteger(argv, 1);

    if(janet_checktype(argv[0], JANET_ABSTRACT)) {
        FILE *f = ((JanetFile *)
                   janet_getabstract(argv, 0, &janet_file_type))->file;
        if (0 != fchown(fileno(f), uid, gid)) {
            sys_errno("Failed to chdir with file handle");
            return janet_wrap_boolean(0);
        }
    } else {
        const char *where = (char *)janet_getstring(argv, 0);
        if (0 != chown(where, uid, gid)) {
            sys_errnof("Failed to chdir with path: %s", where);
            return janet_wrap_boolean(0);
        }
    }

    return janet_wrap_boolean(1);
}

JANET_FN(cfun_chroot, SYS_FUSAGE0("chroot"),
         "DOCUMENT ME") {
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
JANET_FN(cfun_dup2, SYS_FUSAGE0("dup2"),
         "DOCUMENT ME") {
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
         "DOCUMENT ME") {
    janet_fixarity(argc, 0);

    pid_t pid;

    if (-1 == (pid = fork())) {
        sys_errno("Failed to fork");
    }

    /* If we actually have an error, this shouldn't be reached... */
    return janet_wrap_integer(pid);
}

JANET_FN(cfun_setegid, SYS_FUSAGE0("setegid"),
         "DOCUMENT ME") {
    janet_fixarity(argc, 1);

    gid_t gid = janet_getinteger(argv, 0);

    if (0 != setegid(gid)) {
        sys_errnof("Failed to set effective group id: %d", gid);
        return janet_wrap_boolean(0);
    }

    return janet_wrap_boolean(1);
}

JANET_FN(cfun_seteuid, SYS_FUSAGE0("seteuid"),
         "DOCUMENT ME") {
    janet_fixarity(argc, 1);

    uid_t uid = janet_getinteger(argv, 0);

    if (0 != seteuid(uid)) {
        sys_errnof("Failed to set effective user id: %d", uid);
        return janet_wrap_boolean(0);
    }

    return janet_wrap_boolean(1);
}

JANET_FN(cfun_setgid, SYS_FUSAGE0("setgid"),
         "DOCUMENT ME") {
    janet_fixarity(argc, 1);

    gid_t gid = janet_getinteger(argv, 0);

    if (0 != setegid(gid)) {
        sys_errnof("Failed to set group id: %d", gid);
        return janet_wrap_boolean(0);
    }

    return janet_wrap_boolean(1);
}

JANET_FN(cfun_setuid, SYS_FUSAGE0("setuid"),
         "DOCUMENT ME") {
    janet_fixarity(argc, 1);

    uid_t uid = janet_getinteger(argv, 0);

    if (0 != seteuid(uid)) {
        sys_errnof("Failed to set user id: %d", uid);
        return janet_wrap_boolean(0);
    }

    return janet_wrap_boolean(1);
}

JANET_FN(cfun_setsid, SYS_FUSAGE0("setsid"),
         "DOCUMENT ME") {
    janet_fixarity(argc, 0);

    pid_t pid;

    if (-1 == (pid = setsid())) {
        sys_errno("Failed to become session leader");
        return janet_wrap_boolean(0);
    }

    return janet_wrap_integer(pid);
}

/* *nix: fcntl.h, *: ? */
/* Only supporting F_GETLK, F_SETLK, and F_SETLK at this moment
 * Operates on open file handles only (os/open) or (file/open)
 * Using fcntl for lock files exclusively (no other methods), it supports
 * getting the pid of the process holding the lock, which the other
 * interfaces may not. */
JANET_FN(cfun_fcntl, SYS_FUSAGE0("fcntl"),
         "DOCUMENT ME") {
    janet_fixarity(argc, 2);

    if(janet_checktype(argv[0], JANET_ABSTRACT)) {
        int fd = fileno(((JanetFile *)
                         janet_getabstract(argv, 0, &janet_file_type))->file);
        int op = 0;
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

int sys_name_opt(int32_t argc, Janet *argv, int *id, char *name) {
    int ret = 0;

    if(janet_checktype(argv[0], JANET_STRING)) {
        /* converting name to integer if necessary is left to wrapper! */
        name = (char *)janet_getcstring(argv, 0);
    } else if (janet_checktype(argv[0], JANET_NUMBER)) {
        int ret = 1;
        *id = janet_getinteger(argv, 0);
    } else {
        janet_panic("Slot #1 must be a string (username) or integer (uid)");
    }

    return ret;
}

JANET_FN(cfun_getpwnam, SYS_FUSAGE0("getpwnam"),
         "DOCUMENT ME") {
    janet_fixarity(argc, 1);

    struct passwd *ent;
    char  *buf = (char *)janet_smalloc(sizeof(char) * 128);
    int id; char *name;

    /* did we get an ID? or a Name? */
    switch(sys_name_opt(argc, argv, &id, name)) {
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
                     janet_wrap_keyword(":user-name"),
                     janet_wrap_string(janet_cstring(ent->pw_name)));
    janet_struct_put(ret,
                     janet_wrap_keyword(":password"),
                     janet_wrap_string(janet_cstring(ent->pw_passwd)));
    janet_struct_put(ret,
                     janet_wrap_keyword(":user-id"),
                     janet_wrap_integer(ent->pw_uid));
    janet_struct_put(ret,
                     janet_wrap_keyword(":group-id"),
                     janet_wrap_integer(ent->pw_gid));
    janet_struct_put(ret,
                     janet_wrap_keyword(":home-directory"),
                     janet_wrap_string(janet_cstring(ent->pw_dir)));
    janet_struct_put(ret,
                     janet_wrap_keyword(":shell"),
                     janet_wrap_string(janet_cstring(ent->pw_shell)));
    janet_struct_put(ret,
                     janet_wrap_keyword(":gecos"),
                     janet_wrap_string(janet_cstring(ent->pw_gecos)));
    const JanetStruct r = janet_struct_end(ret);

    /* copy out above! */
    janet_sfree(buf);

    return janet_wrap_struct(r);
}

/* *nix: grp.h, *: ? */
JANET_FN(cfun_getgrnam, SYS_FUSAGE0("getgrnam"),
         "DOCUMENT ME") {
    janet_fixarity(argc, 1);

    struct group *ent;
    char  *buf = (char *)janet_smalloc(sizeof(char) * 128);
    int id; char *name;

    /* did we get an ID? or a Name? */
    switch(sys_name_opt(argc, argv, &id, name)) {
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
                     janet_wrap_keyword(":group-name"),
                     janet_wrap_string(janet_cstring(ent->gr_name)));
    janet_struct_put(ret,
                     janet_wrap_keyword(":password"),
                     janet_wrap_string(janet_cstring(ent->gr_passwd)));
    janet_struct_put(ret,
                     janet_wrap_keyword(":group-id"),
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
                    janet_wrap_keyword(":group-members"),
                    janet_wrap_tuple(janet_tuple_n(mem, len)));

    const JanetStruct r = janet_struct_end(ret);

    /* copy out above! */
    janet_sfree(buf);

    return janet_wrap_struct(r);
}
#else /* Windows */
/* *nix: unistd.h, *: ? */
DEF_NOT_IMPL(cfun_chdir, "sys/windows/chdir");
DEF_NOT_IMPL(cfun_chown, "sys/windows/chown");
DEF_NOT_IMPL(cfun_chroot, "sys/windows/chroot");
DEF_NOT_IMPL(cfun_dup2, "sys/windows/dup2");
DEF_NOT_IMPL(cfun_fork, "sys/windows/fork");
DEF_NOT_IMPL(cfun_setegid, "sys/windows/setegid");
DEF_NOT_IMPL(cfun_seteuid, "sys/windows/seteuid");
DEF_NOT_IMPL(cfun_setgid, "sys/windows/setgid");
DEF_NOT_IMPL(cfun_setuid, "sys/windows/setuid");
DEF_NOT_IMPL(cfun_setsid, "sys/windows/setsid");

/* *nix: fcntl.h, *: ? */
DEF_NOT_IMPL(cfun_fcntl, "sys/windows/fcntl");

/* *nix: pwd.h, *: ? */
DEF_NOT_IMPL(cfun_getpwnam, "sys/windows/getpwnam");

/* *nix: grp.h, *: ? */
DEF_NOT_IMPL(cfun_getgrnam, "sys/windows/getgrnam");
#endif

/*============================================================================
 * Export functions
 ===========================================================================*/
/* Don't bother exporting functions for one platform as unimplemented on
 * another platform */
JANET_MODULE_ENTRY(JanetTable *env) {
    janet_cfuns_ext(env, "sys", (JanetRegExt[]) {
        /* *nix: unistd.h, *: ? */
        JANET_REG(SYS_IMPL "/chdir", cfun_chdir),
                  JANET_REG(SYS_IMPL "/chown", cfun_chown),
                  JANET_REG(SYS_IMPL "/chroot", cfun_chroot),
                  JANET_REG(SYS_IMPL "/dup2", cfun_dup2),
                  JANET_REG(SYS_IMPL "/fork", cfun_fork),
                  JANET_REG(SYS_IMPL "/setegid", cfun_setegid),
                  JANET_REG(SYS_IMPL "/seteuid", cfun_seteuid),
                  JANET_REG(SYS_IMPL "/setgid", cfun_setgid),
                  JANET_REG(SYS_IMPL "/setuid", cfun_setuid),
                  JANET_REG(SYS_IMPL "/setsid", cfun_setsid),

                  /* *nix: fcntl.h, *: ? */
                  JANET_REG(SYS_IMPL "/fcntl", cfun_fcntl),

                  /* *nix: pwd.h, *: ? */
                  JANET_REG(SYS_IMPL "/getpwnam", cfun_getpwnam),

                  /* *nix: grp.h, *: ? */
                  JANET_REG(SYS_IMPL "/getgrnam", cfun_getgrnam),
                  JANET_REG_END
    });
}
