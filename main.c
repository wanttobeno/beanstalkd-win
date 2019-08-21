#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "dat.h"




#ifndef WIN32
#include <pwd.h>

static void
su(const char *user) {
    int r;
    struct passwd *pwent;

    errno = 0;
    pwent = getpwnam(user);
    if (errno) twarn("getpwnam(\"%s\")", user), exit(32);
    if (!pwent) twarnx("getpwnam(\"%s\"): no such user", user), exit(33);

    r = setgid(pwent->pw_gid);
    if (r == -1) twarn("setgid(%d \"%s\")", pwent->pw_gid, user), exit(34);

    r = setuid(pwent->pw_uid);
    if (r == -1) twarn("setuid(%d \"%s\")", pwent->pw_uid, user), exit(34);
}

static void
set_sig_handlers()
{
    int r;
    struct sigaction sa;

    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    r = sigemptyset(&sa.sa_mask);
    if (r == -1) twarn("sigemptyset()"), exit(111);

    r = sigaction(SIGPIPE, &sa, 0);
    if (r == -1) twarn("sigaction(SIGPIPE)"), exit(111);

    sa.sa_handler = enter_drain_mode;
    r = sigaction(SIGUSR1, &sa, 0);
    if (r == -1) twarn("sigaction(SIGUSR1)"), exit(111);
}

static int 
setlinebuf(FILE* stream) {
    return setvbuf(stream, (char*)NULL, _IOLBF, 0);
}
#else

int 
init_winsock() {
    WSADATA wsa;
    WORD vers;
    int r;

    vers = MAKEWORD(2, 2);
    r = WSAStartup(vers, &wsa);

    if(r != NO_ERROR || LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wVersion) != 2 ) {
        twarnx("init_winsock()");
        exit(1);
    } 

    return 0;
}


#endif


int
main(int argc, char **argv)
{
    int r;
    struct job list = {0};

#if !defined WIN32
    setlinebuf(stdout);
#else
    init_winsock(); 
#endif

    progname = argv[0];
    optparse(&srv, argv+1);


    if (verbose) {
        printf("pid %d\n", getpid());
    }

    r = make_server_socket(srv.addr, srv.port);
    if (r == -1) twarnx("make_server_socket()"), exit(111);
    srv.sock.fd = r;

    prot_init();

#ifndef WIN32
    if (srv.user) su(srv.user);
    set_sig_handlers();
#endif

    if (srv.wal.use) {
        // We want to make sure that only one beanstalkd tries
        // to use the wal directory at a time. So acquire a lock
        // now and never release it.
        if (!waldirlock(&srv.wal)) {
            twarnx("failed to lock wal dir %s", srv.wal.dir);
            exit(10);
        }

        list.prev = list.next = &list;
        walinit(&srv.wal, &list);
        r = prot_replay(&srv, &list);
        if (!r) {
            twarnx("failed to replay log");
            return 1;
        }
    }

    srvserve(&srv);
    return 0;
}
