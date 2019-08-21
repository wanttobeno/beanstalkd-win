#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "dat_w32.h"
#include "dat.h"

#if defined WIN32_SELECT_MODE

#define IDLE_BUF_SIZE (1024)

#ifdef DEBUG

    #define WRITE_DEBUG_LOG(...)    \
    do {                            \
        if (verbose) {              \
            printf(__VA_ARGS__);    \
        }                           \
    }while(0);
#else
    #define WRITE_DEBUG_LOG(...)
#endif

#define WRITE_LOG(...)              \
    do {                            \
        if (verbose) {              \
            printf(__VA_ARGS__);    \
        }                           \
    }while(0);

typedef struct fd_list {
        u_int   fd_count;               /* how many are SET? */
        Socket* fd_array[FD_SETSIZE];   /* an array of SOCKETs */
        char    rw_array[FD_SETSIZE];   /* an array of rw */
} fd_list;

#define FDLIST_CLR(fd, set) do { \
    u_int __i; \
    for (__i = 0; __i < ((fd_list *)(set))->fd_count ; __i++) { \
        if (((fd_list *)(set))->fd_array[__i] == (fd)) { \
            while (__i < ((fd_list *)(set))->fd_count-1) { \
                ((fd_list *)(set))->fd_array[__i] = \
                    ((fd_list *)(set))->fd_array[__i+1]; \
                ((fd_list *)(set))->rw_array[__i] = \
                    ((fd_list *)(set))->rw_array[__i+1]; \
                __i++; \
            } \
            ((fd_list *)(set))->fd_count--; \
            break; \
        } \
    } \
} while(0)

#define FDLIST_SET(fd, rw, set) do { \
    u_int __i; \
    for (__i = 0; __i < ((fd_list *)(set))->fd_count; __i++) { \
        if (((fd_list *)(set))->fd_array[__i] == (fd)) { \
            ((fd_list *)(set))->rw_array[__i] = (rw); \
            break; \
        } \
    } \
    if (__i == ((fd_list *)(set))->fd_count) { \
        if (((fd_list *)(set))->fd_count < FD_SETSIZE) { \
            ((fd_list *)(set))->fd_array[__i] = (fd); \
            ((fd_list *)(set))->rw_array[__i] = (rw); \
            ((fd_list *)(set))->fd_count++; \
        } \
    } \
} while(0)

static fd_set         master_error_fd_set;
static fd_set         master_read_fd_set;
static fd_set         master_write_fd_set;
static fd_set         work_read_fd_set;
static fd_set         work_write_fd_set;

static fd_list        master_read_fd_list;
static fd_list        master_write_fd_list;
static fd_list        work_read_fd_list;
static fd_list        work_write_fd_list;
static fd_list        work_error_fd_list;

static int            max_fd;


static char buf0[512]; /* buffer of zeros */ 



static int
select_repair_fd_sets(Socket **ret)
{
    int    n, i;
    int    len;
    Socket* s;

    WRITE_LOG("[select_repair_fd_sets] begin\n");

    for (i = 0; i < master_read_fd_list.fd_count; i++) {
        WRITE_LOG("[select_repair_fd_sets] test read fd:%d\n", s->fd);

        s = master_read_fd_list.fd_array[i];

        len = sizeof(int);

        if (getsockopt(s->fd, SOL_SOCKET, SO_TYPE, &n, &len) == -1) {
            WRITE_LOG("[select_repair_fd_sets] invalid descriptor %d in read fd_set\n", s->fd);
            FD_CLR(s->fd, &master_read_fd_set);
            FDLIST_CLR(s, &master_read_fd_list);

            *ret = s;
            return 'h';
        }
    }


    for (i = 0; i < master_write_fd_list.fd_count; i++) {
        WRITE_LOG("[select_repair_fd_sets] test write fd:%d\n", s->fd);

        s = master_write_fd_list.fd_array[i];

        len = sizeof(int);

        if (getsockopt(s->fd, SOL_SOCKET, SO_TYPE, &n, &len) == -1) {
            WRITE_LOG("[select_repair_fd_sets] invalid descriptor %d in write fd_set\n", s->fd);
            FD_CLR(s->fd, &master_write_fd_set);
            FDLIST_CLR(s, &master_write_fd_list);

            *ret = s;
            return 'h';
        }
    }

    max_fd = -1;
    return 'h';
}




/* Allocate disk space.
 * Expects fd's offset to be 0; may also reset fd's offset to 0.
 * Returns 0 on success, and a positive errno otherwise. */
int
rawfalloc(int fd, int len)
{
    int i, w;

    WRITE_LOG("rawfalloc len\n");

    for (i = 0; i < len; i += w) {
        w = write(fd, buf0, sizeof buf0);
        if (w == -1) return errno;
    }

    lseek(fd, 0, 0); /* do not care if this fails */

    return 0;
}


int
sockinit(void)
{
    WRITE_LOG("[sockinit] sockinit\n");

    FD_ZERO(&master_error_fd_set);
    FD_ZERO(&master_read_fd_set);
    FD_ZERO(&master_write_fd_set);
    FD_ZERO(&work_error_fd_list);

    max_fd = -1;
    WRITE_LOG("[sockinit] done\n");

    return 0;
}

int
sockwant(Socket *s, int rw)
{

    WRITE_LOG("sockwant, fd:%d, want:%c\n", s->fd, (char)rw);
    if (s->added) {
        switch (s->added) {
        case 'r':
            WRITE_LOG("[sockwant] close read, fd:%d\n", s->fd);
            FD_CLR(s->fd, &master_read_fd_set);
            FDLIST_CLR(s, &master_read_fd_list);
            break;
        case 'w':
            WRITE_LOG("[sockwant] close write, fd:%d\n", s->fd);
            FD_CLR(s->fd, &master_write_fd_set);
            FDLIST_CLR(s, &master_write_fd_list);
            break;
        default:
            break;
        }
    }

    if (rw) {

        if (FD_ISSET(s->fd, &master_error_fd_set)) {
            WRITE_LOG("[sockwant] socket is in error set: fd:%d\n", s->fd);
            return -1;
        }

        switch (rw) {
        case 'r':
        case 'h':
            WRITE_LOG("[sockwant] log it want read:%c, fd:%d\n", rw, s->fd);
            FD_SET(s->fd,     &master_read_fd_set);
            FDLIST_SET(s, rw, &master_read_fd_list);
            break;
        case 'w':
            WRITE_LOG("[sockwant] log it want write:%c, fd:%d\n", rw, s->fd);
            FD_SET(s->fd,     &master_write_fd_set);
            FDLIST_SET(s, rw, &master_write_fd_list);
            break;
        default:
            break;
        }

        s->added = rw;

        if (max_fd < s->fd) {
            max_fd = s->fd;
        }
    } else {
        WRITE_LOG("[sockwant] clear error socket:fd:%d\n", s->fd);
        FD_CLR(s->fd, &master_error_fd_set);

        FD_CLR(s->fd, &master_read_fd_set);
        FDLIST_CLR(s, &master_read_fd_list);

        FD_CLR(s->fd, &master_write_fd_set);
        FDLIST_CLR(s, &master_write_fd_list);
    }

    WRITE_LOG("[sockwant] done:fd:%d\n", s->fd);
    return 0;
}

int
idle_read(Socket *s)
{
    char bucket[IDLE_BUF_SIZE] = {0};
    int r = 0, c = 0;

    WRITE_LOG("[idle_read] begin:fd:%d\n", s->fd);

    do {
        r = net_read(s->fd, bucket, IDLE_BUF_SIZE);
        c++;
    } while (r > 0);

    WRITE_LOG("[idle_read] idle read socket %d in %d times\n", (s)->fd, c);

    return r;
}

int
socknext(Socket **s, int64 timeout)
{
    int r, i;
    struct timeval tv, *tp;
    struct fd_list fd_notified = {0, {0}};



    // finish the list created before.
    if (work_error_fd_list.fd_count != 0) {
        *s = work_error_fd_list.fd_array[work_error_fd_list.fd_count-1];
        FDLIST_CLR(*s, &work_error_fd_list);
        WRITE_LOG("[socknext] error socket before  %d\n", (*s)->fd);
        return 'h';
    }

    if (work_read_fd_list.fd_count != 0) {
        *s = work_read_fd_list.fd_array[work_read_fd_list.fd_count-1];
        FDLIST_CLR(*s, &work_read_fd_list);
        WRITE_LOG("[socknext] read socket before  %d\n", (*s)->fd);
        return 'r';
    }

    if (work_write_fd_list.fd_count != 0){
        *s = work_write_fd_list.fd_array[work_write_fd_list.fd_count-1];
        FDLIST_CLR(*s, &work_write_fd_list);
        WRITE_LOG("[socknext] write socket before  %d\n", (*s)->fd);
        return 'w';
    }


    work_read_fd_set = master_read_fd_set;
    work_write_fd_set = master_write_fd_set;

    work_read_fd_list = master_read_fd_list;
    work_write_fd_list = master_write_fd_list;


    //long msec = (long)(timeout/1000000);
    //tv.tv_sec = (long) (msec / 1000);
    //tv.tv_usec = (long) (msec);

    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    tp = &tv;
    WRITE_DEBUG_LOG("[socknext] max_fd:%d, socknext () second:%ld. %ld\n", max_fd, tv.tv_sec, tv.tv_usec);

    r = select(max_fd + 1, &work_read_fd_set, &work_write_fd_set, NULL, tp);

    if (r == -1) {
        twarnx("select erro\n");
        WRITE_LOG("[socknext] select_repair_fd_sets\n");
        return select_repair_fd_sets(s);
    }



    for (i = 0; i < work_read_fd_list.fd_count; i++) {
        int fd = work_read_fd_list.fd_array[i]->fd;

        if (FD_ISSET(fd, &work_read_fd_set) == 0) {
            continue;
        }

        if (work_read_fd_list.rw_array[i] == 'h') { // if rw is hang up, idle read the data
            r = idle_read(work_read_fd_list.fd_array[i]);
            if (r <= 0) {
                WRITE_LOG("[socknext] save sock error event: %d\n", work_read_fd_list.fd_array[i]->fd);
                FDLIST_SET(work_read_fd_list.fd_array[i], work_read_fd_list.rw_array[i], &work_error_fd_list);

                FD_SET(work_read_fd_list.fd_array[i]->fd, &master_error_fd_set);

                FDLIST_CLR(work_read_fd_list.fd_array[i], &master_read_fd_list);
                FDLIST_CLR(work_read_fd_list.fd_array[i], &master_write_fd_list);
            }
            continue;
        }

        WRITE_LOG("[socknext] save sock read event: %d\n", work_read_fd_list.fd_array[i]->fd);
        FDLIST_SET(work_read_fd_list.fd_array[i], work_read_fd_list.rw_array[i], &fd_notified);
    }
    work_read_fd_list = fd_notified;


    fd_notified.fd_count = 0;
    for (i = 0; i < work_write_fd_list.fd_count; i++) {

        int fd = work_write_fd_list.fd_array[i]->fd;

        if (FD_ISSET(fd, &work_write_fd_set) == 0) {
            continue;
        }

        if (FD_ISSET(fd, &master_error_fd_set)) {
            WRITE_LOG("[socknext] can't save write event, error socket: %d\n", fd);
            continue;
        }

        WRITE_LOG("[socknext] save sock write event: %d\n", fd);
        FDLIST_SET(work_write_fd_list.fd_array[i], work_write_fd_list.rw_array[i], &fd_notified);
    }
    work_write_fd_list = fd_notified;

    if (work_error_fd_list.fd_count != 0) {
        *s = work_error_fd_list.fd_array[work_error_fd_list.fd_count-1];
        FDLIST_CLR(*s, &work_error_fd_list);
        WRITE_LOG("[socknext]  error  socket  %d\n", (*s)->fd);
        return 'h';
    }

    if (work_read_fd_list.fd_count != 0) {
        *s = work_read_fd_list.fd_array[work_read_fd_list.fd_count-1];
        FDLIST_CLR(*s, &work_read_fd_list);
        WRITE_LOG("[socknext]  read  socket  %d\n", (*s)->fd);
        return 'r';
    }

    if (work_write_fd_list.fd_count != 0){
        *s = work_write_fd_list.fd_array[work_write_fd_list.fd_count-1];
        FDLIST_CLR(*s, &work_write_fd_list);
        WRITE_LOG("[socknext]  write  socket  %d\n", (*s)->fd);
        return 'w';
    }

    return 0;
}

#endif