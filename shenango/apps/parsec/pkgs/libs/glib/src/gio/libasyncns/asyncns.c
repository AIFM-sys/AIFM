/***
  This file is part of libasyncns.

  Copyright 2005-2008 Lennart Poettering

  libasyncns is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 2.1 of the
  License, or (at your option) any later version.

  libasyncns is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with libasyncns. If not, see
  <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include "g-asyncns.h"
#endif

/* #undef HAVE_PTHREAD */

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pwd.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <dirent.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <sys/resource.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#if HAVE_PTHREAD
#include <pthread.h>
#endif

#include "asyncns.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define MAX_WORKERS 16
#define MAX_QUERIES 256
#define BUFSIZE (10240)

typedef enum {
    REQUEST_ADDRINFO,
    RESPONSE_ADDRINFO,
    REQUEST_NAMEINFO,
    RESPONSE_NAMEINFO,
    REQUEST_RES_QUERY,
    REQUEST_RES_SEARCH,
    RESPONSE_RES,
    REQUEST_TERMINATE,
    RESPONSE_DIED
} query_type_t;

enum {
    REQUEST_RECV_FD = 0,
    REQUEST_SEND_FD = 1,
    RESPONSE_RECV_FD = 2,
    RESPONSE_SEND_FD = 3,
    MESSAGE_FD_MAX = 4
};

struct asyncns {
    int fds[4];

#ifndef HAVE_PTHREAD
    pid_t workers[MAX_WORKERS];
#else
    pthread_t workers[MAX_WORKERS];
#endif
    unsigned valid_workers;

    unsigned current_id, current_index;
    _g_asyncns_query_t* queries[MAX_QUERIES];

    _g_asyncns_query_t *done_head, *done_tail;

    int n_queries;
    int dead;
};

struct _g_asyncns_query {
    _g_asyncns_t *asyncns;
    int done;
    unsigned id;
    query_type_t type;
    _g_asyncns_query_t *done_next, *done_prev;
    int ret;
    int _errno;
    int _h_errno;
    struct addrinfo *addrinfo;
    char *serv, *host;
    void *userdata;
};

typedef struct rheader {
    query_type_t type;
    unsigned id;
    size_t length;
} rheader_t;

typedef struct addrinfo_request {
    struct rheader header;
    int hints_is_null;
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t node_len, service_len;
} addrinfo_request_t;

typedef struct addrinfo_response {
    struct rheader header;
    int ret;
    int _errno;
    int _h_errno;
    /* followed by addrinfo_serialization[] */
} addrinfo_response_t;

typedef struct addrinfo_serialization {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    size_t canonname_len;
    /* Followed by ai_addr amd ai_canonname with variable lengths */
} addrinfo_serialization_t;

typedef struct nameinfo_request {
    struct rheader header;
    int flags;
    socklen_t sockaddr_len;
    int gethost, getserv;
} nameinfo_request_t;

typedef struct nameinfo_response {
    struct rheader header;
    size_t hostlen, servlen;
    int ret;
    int _errno;
    int _h_errno;
} nameinfo_response_t;

typedef struct res_request {
    struct rheader header;
    int class;
    int type;
    size_t dname_len;
} res_request_t;

typedef struct res_response {
    struct rheader header;
    int ret;
    int _errno;
    int _h_errno;
} res_response_t;

typedef union packet {
    rheader_t rheader;
    addrinfo_request_t addrinfo_request;
    addrinfo_response_t addrinfo_response;
    nameinfo_request_t nameinfo_request;
    nameinfo_response_t nameinfo_response;
    res_request_t res_request;
    res_response_t res_response;
} packet_t;

#ifndef HAVE_STRNDUP

static char *strndup(const char *s, size_t l) {
    size_t a;
    char *n;

    a = strlen(s);
    if (a > l)
        a = l;

    if (!(n = malloc(a+1)))
        return NULL;

    memcpy(n, s, a);
    n[a] = 0;

    return n;
}

#endif

#ifndef HAVE_PTHREAD

static int close_allv(const int except_fds[]) {
    struct rlimit rl;
    int fd, maxfd;

#ifdef __linux__

    DIR *d;

    assert(except_fds);

    if ((d = opendir("/proc/self/fd"))) {

        struct dirent *de;

        while ((de = readdir(d))) {
            int found;
            long l;
            char *e = NULL;
            int i;

            if (de->d_name[0] == '.')
                continue;

            errno = 0;
            l = strtol(de->d_name, &e, 10);
            if (errno != 0 || !e || *e) {
                closedir(d);
                errno = EINVAL;
                return -1;
            }

            fd = (int) l;

            if ((long) fd != l) {
                closedir(d);
                errno = EINVAL;
                return -1;
            }

            if (fd < 3)
                continue;

            if (fd == dirfd(d))
                continue;

            found = 0;
            for (i = 0; except_fds[i] >= 0; i++)
                if (except_fds[i] == fd) {
                    found = 1;
                    break;
                }

            if (found)
                continue;

            if (close(fd) < 0) {
                int saved_errno;

                saved_errno = errno;
                closedir(d);
                errno = saved_errno;

                return -1;
            }
        }

        closedir(d);
        return 0;
    }

#endif

    if (getrlimit(RLIMIT_NOFILE, &rl) > 0)
        maxfd = (int) rl.rlim_max;
    else
        maxfd = sysconf(_SC_OPEN_MAX);

    for (fd = 3; fd < maxfd; fd++) {
        int i, found;

        found = 0;
        for (i = 0; except_fds[i] >= 0; i++)
            if (except_fds[i] == fd) {
                found = 1;
                continue;
            }

        if (found)
            continue;

        if (close(fd) < 0 && errno != EBADF)
            return -1;
    }

    return 0;
}

static int reset_sigsv(const int except[]) {
    int sig;
    assert(except);

    for (sig = 1; sig < NSIG; sig++) {
        int reset = 1;

        switch (sig) {
            case SIGKILL:
            case SIGSTOP:
                reset = 0;
                break;

            default: {
                int i;

                for (i = 0; except[i] > 0; i++) {
                    if (sig == except[i]) {
                        reset = 0;
                        break;
                    }
                }
            }
        }

        if (reset) {
            struct sigaction sa;

            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = SIG_DFL;

            /* On Linux the first two RT signals are reserved by
             * glibc, and sigaction() will return EINVAL for them. */
            if ((sigaction(sig, &sa, NULL) < 0))
                if (errno != EINVAL)
                    return -1;
        }
    }

    return 0;
}

static int ignore_sigsv(const int ignore[]) {
    int i;
    assert(ignore);

    for (i = 0; ignore[i] > 0; i++) {
        struct sigaction sa;

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = SIG_IGN;

        if ((sigaction(ignore[i], &sa, NULL) < 0))
            return -1;
    }

    return 0;
}

#endif

static int fd_nonblock(int fd) {
    int i;
    assert(fd >= 0);

    if ((i = fcntl(fd, F_GETFL, 0)) < 0)
        return -1;

    if (i & O_NONBLOCK)
        return 0;

    return fcntl(fd, F_SETFL, i | O_NONBLOCK);
}

static int fd_cloexec(int fd) {
    int v;
    assert(fd >= 0);

    if ((v = fcntl(fd, F_GETFD, 0)) < 0)
        return -1;

    if (v & FD_CLOEXEC)
        return 0;

    return fcntl(fd, F_SETFD, v | FD_CLOEXEC);
}

static int send_died(int out_fd) {
    rheader_t rh;
    assert(out_fd > 0);

    memset(&rh, 0, sizeof(rh));
    rh.type = RESPONSE_DIED;
    rh.id = 0;
    rh.length = sizeof(rh);

    return send(out_fd, &rh, rh.length, MSG_NOSIGNAL);
}

static void *serialize_addrinfo(void *p, const struct addrinfo *ai, size_t *length, size_t maxlength) {
    addrinfo_serialization_t s;
    size_t cnl, l;
    assert(p);
    assert(ai);
    assert(length);
    assert(*length <= maxlength);

    cnl = (ai->ai_canonname ? strlen(ai->ai_canonname)+1 : 0);
    l = sizeof(addrinfo_serialization_t) + ai->ai_addrlen + cnl;

    if (*length + l > maxlength)
        return NULL;

    s.ai_flags = ai->ai_flags;
    s.ai_family = ai->ai_family;
    s.ai_socktype = ai->ai_socktype;
    s.ai_protocol = ai->ai_protocol;
    s.ai_addrlen = ai->ai_addrlen;
    s.canonname_len = cnl;

    memcpy((uint8_t*) p, &s, sizeof(addrinfo_serialization_t));
    memcpy((uint8_t*) p + sizeof(addrinfo_serialization_t), ai->ai_addr, ai->ai_addrlen);

    if (ai->ai_canonname)
        strcpy((char*) p + sizeof(addrinfo_serialization_t) + ai->ai_addrlen, ai->ai_canonname);

    *length += l;
    return (uint8_t*) p + l;
}

static int send_addrinfo_reply(int out_fd, unsigned id, int ret, struct addrinfo *ai, int _errno, int _h_errno) {
    addrinfo_response_t data[BUFSIZE/sizeof(addrinfo_response_t) + 1];
    addrinfo_response_t *resp = data;
    assert(out_fd >= 0);

    memset(data, 0, sizeof(data));
    resp->header.type = RESPONSE_ADDRINFO;
    resp->header.id = id;
    resp->header.length = sizeof(addrinfo_response_t);
    resp->ret = ret;
    resp->_errno = _errno;
    resp->_h_errno = _h_errno;

    if (ret == 0 && ai) {
        void *p = data + 1;
        struct addrinfo *k;

        for (k = ai; k; k = k->ai_next) {

            if (!(p = serialize_addrinfo(p, k, &resp->header.length, (char*) data + BUFSIZE - (char*) p))) {
                resp->ret = EAI_MEMORY;
                break;
            }
        }
    }

    if (ai)
        freeaddrinfo(ai);

    return send(out_fd, resp, resp->header.length, MSG_NOSIGNAL);
}

static int send_nameinfo_reply(int out_fd, unsigned id, int ret, const char *host, const char *serv, int _errno, int _h_errno) {
    nameinfo_response_t data[BUFSIZE/sizeof(nameinfo_response_t) + 1];
    size_t hl, sl;
    nameinfo_response_t *resp = data;

    assert(out_fd >= 0);

    sl = serv ? strlen(serv)+1 : 0;
    hl = host ? strlen(host)+1 : 0;

    memset(data, 0, sizeof(data));
    resp->header.type = RESPONSE_NAMEINFO;
    resp->header.id = id;
    resp->header.length = sizeof(nameinfo_response_t) + hl + sl;
    resp->ret = ret;
    resp->_errno = _errno;
    resp->_h_errno = _h_errno;
    resp->hostlen = hl;
    resp->servlen = sl;

    assert(sizeof(data) >= resp->header.length);

    if (host)
        memcpy((uint8_t *)data + sizeof(nameinfo_response_t), host, hl);

    if (serv)
        memcpy((uint8_t *)data + sizeof(nameinfo_response_t) + hl, serv, sl);

    return send(out_fd, resp, resp->header.length, MSG_NOSIGNAL);
}

static int send_res_reply(int out_fd, unsigned id, const unsigned char *answer, int ret, int _errno, int _h_errno) {
    res_response_t data[BUFSIZE/sizeof(res_response_t) + 1];
    res_response_t *resp = data;

    assert(out_fd >= 0);

    memset(data, 0, sizeof(data));
    resp->header.type = RESPONSE_RES;
    resp->header.id = id;
    resp->header.length = sizeof(res_response_t) + (ret < 0 ? 0 : ret);
    resp->ret = ret;
    resp->_errno = _errno;
    resp->_h_errno = _h_errno;

    assert(sizeof(data) >= resp->header.length);

    if (ret > 0)
        memcpy((uint8_t *)data + sizeof(res_response_t), answer, ret);

    return send(out_fd, resp, resp->header.length, MSG_NOSIGNAL);
}

static int handle_request(int out_fd, const packet_t *packet, size_t length) {
    const rheader_t *req;
    assert(out_fd >= 0);

    req = &packet->rheader;
    assert(req);
    assert(length >= sizeof(rheader_t));
    assert(length == req->length);

    switch (req->type) {

        case REQUEST_ADDRINFO: {
            struct addrinfo ai, *result = NULL;
            const addrinfo_request_t *ai_req = &packet->addrinfo_request;
            const char *node, *service;
            int ret;

            assert(length >= sizeof(addrinfo_request_t));
            assert(length == sizeof(addrinfo_request_t) + ai_req->node_len + ai_req->service_len);

            memset(&ai, 0, sizeof(ai));
            ai.ai_flags = ai_req->ai_flags;
            ai.ai_family = ai_req->ai_family;
            ai.ai_socktype = ai_req->ai_socktype;
            ai.ai_protocol = ai_req->ai_protocol;

            node = ai_req->node_len ? (const char*) ai_req + sizeof(addrinfo_request_t) : NULL;
            service = ai_req->service_len ? (const char*) ai_req + sizeof(addrinfo_request_t) + ai_req->node_len : NULL;

            ret = getaddrinfo(node, service,
                              ai_req->hints_is_null ? NULL : &ai,
                              &result);

            /* send_addrinfo_reply() frees result */
            return send_addrinfo_reply(out_fd, req->id, ret, result, errno, h_errno);
        }

        case REQUEST_NAMEINFO: {
            int ret;
            const nameinfo_request_t *ni_req = &packet->nameinfo_request;
            char hostbuf[NI_MAXHOST], servbuf[NI_MAXSERV];
            struct sockaddr_storage sa;

            assert(length >= sizeof(nameinfo_request_t));
            assert(length == sizeof(nameinfo_request_t) + ni_req->sockaddr_len);

            memcpy(&sa, (const uint8_t *) ni_req + sizeof(nameinfo_request_t), ni_req->sockaddr_len);

            ret = getnameinfo((struct sockaddr *)&sa, ni_req->sockaddr_len,
                              ni_req->gethost ? hostbuf : NULL, ni_req->gethost ? sizeof(hostbuf) : 0,
                              ni_req->getserv ? servbuf : NULL, ni_req->getserv ? sizeof(servbuf) : 0,
                              ni_req->flags);

            return send_nameinfo_reply(out_fd, req->id, ret,
                                       ret == 0 && ni_req->gethost ? hostbuf : NULL,
                                       ret == 0 && ni_req->getserv ? servbuf : NULL,
                                       errno, h_errno);
        }

        case REQUEST_RES_QUERY:
        case REQUEST_RES_SEARCH: {
            int ret;
            HEADER answer[BUFSIZE/sizeof(HEADER) + 1];
            const res_request_t *res_req = &packet->res_request;
            const char *dname;

            assert(length >= sizeof(res_request_t));
            assert(length == sizeof(res_request_t) + res_req->dname_len);

            dname = (const char *) req + sizeof(res_request_t);

            if (req->type == REQUEST_RES_QUERY)
                ret = res_query(dname, res_req->class, res_req->type, (unsigned char *) answer, BUFSIZE);
            else
                ret = res_search(dname, res_req->class, res_req->type, (unsigned char *) answer, BUFSIZE);

            return send_res_reply(out_fd, req->id, (unsigned char *) answer, ret, errno, h_errno);
        }

        case REQUEST_TERMINATE:
            /* Quit */
            return -1;

        default:
            ;
    }

    return 0;
}

#ifndef HAVE_PTHREAD

static int process_worker(int in_fd, int out_fd) {
    int have_death_sig = 0;
    int good_fds[3];
    int ret = 1;

    const int ignore_sigs[] = {
        SIGINT,
        SIGHUP,
        SIGPIPE,
        SIGUSR1,
        SIGUSR2,
        -1
    };

    assert(in_fd > 2);
    assert(out_fd > 2);

    close(0);
    close(1);
    close(2);

    if (open("/dev/null", O_RDONLY) != 0)
        goto fail;

    if (open("/dev/null", O_WRONLY) != 1)
        goto fail;

    if (open("/dev/null", O_WRONLY) != 2)
        goto fail;

    if (chdir("/") < 0)
        goto fail;

    if (geteuid() == 0) {
        struct passwd *pw;
        int r;

        if ((pw = getpwnam("nobody"))) {
#ifdef HAVE_SETRESUID
            r = setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid);
#elif HAVE_SETREUID
            r = setreuid(pw->pw_uid, pw->pw_uid);
#else
            if ((r = setuid(pw->pw_uid)) >= 0)
                r = seteuid(pw->pw_uid);
#endif
            if (r < 0)
                goto fail;
        }
    }

    if (reset_sigsv(ignore_sigs) < 0)
        goto fail;

    if (ignore_sigsv(ignore_sigs) < 0)
        goto fail;

    good_fds[0] = in_fd; good_fds[1] = out_fd; good_fds[2] = -1;
    if (close_allv(good_fds) < 0)
        goto fail;

#ifdef PR_SET_PDEATHSIG
    if (prctl(PR_SET_PDEATHSIG, SIGTERM) >= 0)
        have_death_sig = 1;
#endif

    if (!have_death_sig)
        fd_nonblock(in_fd);

    while (getppid() > 1) { /* if the parent PID is 1 our parent process died. */
        packet_t buf[BUFSIZE/sizeof(packet_t) + 1];
        ssize_t length;

        if (!have_death_sig) {
            fd_set fds;
            struct timeval tv = { 0, 500000 };

            FD_ZERO(&fds);
            FD_SET(in_fd, &fds);

            if (select(in_fd+1, &fds, NULL, NULL, &tv) < 0)
                break;

            if (getppid() == 1)
                break;
        }

        if ((length = recv(in_fd, buf, sizeof(buf), 0)) <= 0) {

            if (length < 0 &&
                (errno == EAGAIN || errno == EINTR))
                continue;

            break;
        }

        if (handle_request(out_fd, buf, (size_t) length) < 0)
            break;
    }

    ret = 0;

fail:
    send_died(out_fd);

    return ret;
}

#else

static void* thread_worker(void *p) {
    _g_asyncns_t *asyncns = p;
    sigset_t fullset;

    /* No signals in this thread please */
    sigfillset(&fullset);
    pthread_sigmask(SIG_BLOCK, &fullset, NULL);

    while (!asyncns->dead) {
        packet_t buf[BUFSIZE/sizeof(packet_t) + 1];
        ssize_t length;

        if ((length = recv(asyncns->fds[REQUEST_RECV_FD], buf, sizeof(buf), 0)) <= 0) {

            if (length < 0 &&
                (errno == EAGAIN || errno == EINTR))
                continue;

            break;
        }

        if (asyncns->dead)
            break;

        if (handle_request(asyncns->fds[RESPONSE_SEND_FD], buf, (size_t) length) < 0)
            break;
    }

    send_died(asyncns->fds[RESPONSE_SEND_FD]);

    return NULL;
}

#endif

_g_asyncns_t* _g_asyncns_new(unsigned n_proc) {
    _g_asyncns_t *asyncns = NULL;
    int i;
    assert(n_proc >= 1);

    if (n_proc > MAX_WORKERS)
        n_proc = MAX_WORKERS;

    if (!(asyncns = malloc(sizeof(_g_asyncns_t)))) {
        errno = ENOMEM;
        goto fail;
    }

    asyncns->dead = 0;
    asyncns->valid_workers = 0;

    for (i = 0; i < MESSAGE_FD_MAX; i++)
        asyncns->fds[i] = -1;

    memset(asyncns->queries, 0, sizeof(asyncns->queries));

    if (socketpair(PF_UNIX, SOCK_DGRAM, 0, asyncns->fds) < 0 ||
        socketpair(PF_UNIX, SOCK_DGRAM, 0, asyncns->fds+2) < 0)
        goto fail;

    for (i = 0; i < MESSAGE_FD_MAX; i++)
        fd_cloexec(asyncns->fds[i]);

    for (asyncns->valid_workers = 0; asyncns->valid_workers < n_proc; asyncns->valid_workers++) {

#ifndef HAVE_PTHREAD
        if ((asyncns->workers[asyncns->valid_workers] = fork()) < 0)
            goto fail;
        else if (asyncns->workers[asyncns->valid_workers] == 0) {
            int ret;

            close(asyncns->fds[REQUEST_SEND_FD]);
            close(asyncns->fds[RESPONSE_RECV_FD]);
            ret = process_worker(asyncns->fds[REQUEST_RECV_FD], asyncns->fds[RESPONSE_SEND_FD]);
            close(asyncns->fds[REQUEST_RECV_FD]);
            close(asyncns->fds[RESPONSE_SEND_FD]);
            _exit(ret);
        }
#else
        int r;

        if ((r = pthread_create(&asyncns->workers[asyncns->valid_workers], NULL, thread_worker, asyncns)) != 0) {
            errno = r;
            goto fail;
        }
#endif
    }

#ifndef HAVE_PTHREAD
    close(asyncns->fds[REQUEST_RECV_FD]);
    close(asyncns->fds[RESPONSE_SEND_FD]);
    asyncns->fds[REQUEST_RECV_FD] = asyncns->fds[RESPONSE_SEND_FD] = -1;
#endif

    asyncns->current_index = asyncns->current_id = 0;
    asyncns->done_head = asyncns->done_tail = NULL;
    asyncns->n_queries = 0;

    fd_nonblock(asyncns->fds[RESPONSE_RECV_FD]);

    return asyncns;

fail:
    if (asyncns)
        _g_asyncns_free(asyncns);

    return NULL;
}

void _g_asyncns_free(_g_asyncns_t *asyncns) {
    int i;
    int saved_errno = errno;
    unsigned p;

    assert(asyncns);

    asyncns->dead = 1;

    if (asyncns->fds[REQUEST_SEND_FD] >= 0) {
        rheader_t req;

        memset(&req, 0, sizeof(req));
        req.type = REQUEST_TERMINATE;
        req.length = sizeof(req);
        req.id = 0;

        /* Send one termination packet for each worker */
        for (p = 0; p < asyncns->valid_workers; p++)
            send(asyncns->fds[REQUEST_SEND_FD], &req, req.length, MSG_NOSIGNAL);
    }

    /* Now terminate them and wait until they are gone. */
    for (p = 0; p < asyncns->valid_workers; p++) {
#ifndef HAVE_PTHREAD
        kill(asyncns->workers[p], SIGTERM);
        for (;;) {
            if (waitpid(asyncns->workers[p], NULL, 0) >= 0 || errno != EINTR)
                break;
        }
#else
        for (;;) {
            if (pthread_join(asyncns->workers[p], NULL) != EINTR)
                break;
        }
#endif
    }

    /* Close all communication channels */
    for (i = 0; i < MESSAGE_FD_MAX; i++)
        if (asyncns->fds[i] >= 0)
            close(asyncns->fds[i]);

    for (p = 0; p < MAX_QUERIES; p++)
        if (asyncns->queries[p])
            _g_asyncns_cancel(asyncns, asyncns->queries[p]);

    free(asyncns);

    errno = saved_errno;
}

int _g_asyncns_fd(_g_asyncns_t *asyncns) {
    assert(asyncns);

    return asyncns->fds[RESPONSE_RECV_FD];
}

static _g_asyncns_query_t *lookup_query(_g_asyncns_t *asyncns, unsigned id) {
    _g_asyncns_query_t *q;
    assert(asyncns);

    if ((q = asyncns->queries[id % MAX_QUERIES]))
        if (q->id == id)
            return q;

    return NULL;
}

static void complete_query(_g_asyncns_t *asyncns, _g_asyncns_query_t *q) {
    assert(asyncns);
    assert(q);
    assert(!q->done);

    q->done = 1;

    if ((q->done_prev = asyncns->done_tail))
        asyncns->done_tail->done_next = q;
    else
        asyncns->done_head = q;

    asyncns->done_tail = q;
    q->done_next = NULL;
}

static const void *unserialize_addrinfo(const void *p, struct addrinfo **ret_ai, size_t *length) {
    addrinfo_serialization_t s;
    size_t l;
    struct addrinfo *ai;
    assert(p);
    assert(ret_ai);
    assert(length);

    if (*length < sizeof(addrinfo_serialization_t))
        return NULL;

    memcpy(&s, p, sizeof(s));

    l = sizeof(addrinfo_serialization_t) + s.ai_addrlen + s.canonname_len;
    if (*length < l)
        return NULL;

    if (!(ai = malloc(sizeof(struct addrinfo))))
        goto fail;

    ai->ai_addr = NULL;
    ai->ai_canonname = NULL;
    ai->ai_next = NULL;

    if (s.ai_addrlen && !(ai->ai_addr = malloc(s.ai_addrlen)))
        goto fail;

    if (s.canonname_len && !(ai->ai_canonname = malloc(s.canonname_len)))
        goto fail;

    ai->ai_flags = s.ai_flags;
    ai->ai_family = s.ai_family;
    ai->ai_socktype = s.ai_socktype;
    ai->ai_protocol = s.ai_protocol;
    ai->ai_addrlen = s.ai_addrlen;

    if (ai->ai_addr)
        memcpy(ai->ai_addr, (const uint8_t*) p + sizeof(addrinfo_serialization_t), s.ai_addrlen);

    if (ai->ai_canonname)
        memcpy(ai->ai_canonname, (const uint8_t*) p + sizeof(addrinfo_serialization_t) + s.ai_addrlen, s.canonname_len);

    *length -= l;
    *ret_ai = ai;

    return (const uint8_t*) p + l;


fail:
    if (ai)
        _g_asyncns_freeaddrinfo(ai);

    return NULL;
}

static int handle_response(_g_asyncns_t *asyncns, const packet_t *packet, size_t length) {
    const rheader_t *resp;
    _g_asyncns_query_t *q;

    assert(asyncns);

    resp = &packet->rheader;
    assert(resp);
    assert(length >= sizeof(rheader_t));
    assert(length == resp->length);

    if (resp->type == RESPONSE_DIED) {
        asyncns->dead = 1;
        return 0;
    }

    if (!(q = lookup_query(asyncns, resp->id)))
        return 0;

    switch (resp->type) {
        case RESPONSE_ADDRINFO: {
            const addrinfo_response_t *ai_resp = &packet->addrinfo_response;
            const void *p;
            size_t l;
            struct addrinfo *prev = NULL;

            assert(length >= sizeof(addrinfo_response_t));
            assert(q->type == REQUEST_ADDRINFO);

            q->ret = ai_resp->ret;
            q->_errno = ai_resp->_errno;
            q->_h_errno = ai_resp->_h_errno;
            l = length - sizeof(addrinfo_response_t);
            p = (const uint8_t*) resp + sizeof(addrinfo_response_t);

            while (l > 0 && p) {
                struct addrinfo *ai = NULL;
                p = unserialize_addrinfo(p, &ai, &l);

                if (!p || !ai) {
                    q->ret = EAI_MEMORY;
                    break;
                }

                if (prev)
                    prev->ai_next = ai;
                else
                    q->addrinfo = ai;

                prev = ai;
            }

            complete_query(asyncns, q);
            break;
        }

        case RESPONSE_NAMEINFO: {
            const nameinfo_response_t *ni_resp = &packet->nameinfo_response;

            assert(length >= sizeof(nameinfo_response_t));
            assert(q->type == REQUEST_NAMEINFO);

            q->ret = ni_resp->ret;
            q->_errno = ni_resp->_errno;
            q->_h_errno = ni_resp->_h_errno;

            if (ni_resp->hostlen)
                if (!(q->host = strndup((const char*) ni_resp + sizeof(nameinfo_response_t), ni_resp->hostlen-1)))
                    q->ret = EAI_MEMORY;

            if (ni_resp->servlen)
                if (!(q->serv = strndup((const char*) ni_resp + sizeof(nameinfo_response_t) + ni_resp->hostlen, ni_resp->servlen-1)))
                    q->ret = EAI_MEMORY;

            complete_query(asyncns, q);
            break;
        }

        case RESPONSE_RES: {
            const res_response_t *res_resp = &packet->res_response;

            assert(length >= sizeof(res_response_t));
            assert(q->type == REQUEST_RES_QUERY || q->type == REQUEST_RES_SEARCH);

            q->ret = res_resp->ret;
            q->_errno = res_resp->_errno;
            q->_h_errno = res_resp->_h_errno;

            if (res_resp->ret >= 0)  {
                if (!(q->serv = malloc(res_resp->ret))) {
                    q->ret = -1;
                    q->_errno = ENOMEM;
                } else
                    memcpy(q->serv, (const char *)resp + sizeof(res_response_t), res_resp->ret);
            }

            complete_query(asyncns, q);
            break;
        }

        default:
            ;
    }

    return 0;
}

int _g_asyncns_wait(_g_asyncns_t *asyncns, int block) {
    int handled = 0;
    assert(asyncns);

    for (;;) {
        packet_t buf[BUFSIZE/sizeof(packet_t) + 1];
        ssize_t l;

        if (asyncns->dead) {
            errno = ECHILD;
            return -1;
        }

        if (((l = recv(asyncns->fds[RESPONSE_RECV_FD], buf, sizeof(buf), 0)) < 0)) {
            fd_set fds;

            if (errno != EAGAIN)
                return -1;

            if (!block || handled)
                return 0;

            FD_ZERO(&fds);
            FD_SET(asyncns->fds[RESPONSE_RECV_FD], &fds);

            if (select(asyncns->fds[RESPONSE_RECV_FD]+1, &fds, NULL, NULL, NULL) < 0)
                return -1;

            continue;
        }

        if (handle_response(asyncns, buf, (size_t) l) < 0)
            return -1;

        handled = 1;
    }
}

static _g_asyncns_query_t *alloc_query(_g_asyncns_t *asyncns) {
    _g_asyncns_query_t *q;
    assert(asyncns);

    if (asyncns->n_queries >= MAX_QUERIES) {
        errno = ENOMEM;
        return NULL;
    }

    while (asyncns->queries[asyncns->current_index]) {

        asyncns->current_index++;
        asyncns->current_id++;

        while (asyncns->current_index >= MAX_QUERIES)
            asyncns->current_index -= MAX_QUERIES;
    }

    if (!(q = asyncns->queries[asyncns->current_index] = malloc(sizeof(_g_asyncns_query_t)))) {
        errno = ENOMEM;
        return NULL;
    }

    asyncns->n_queries++;

    q->asyncns = asyncns;
    q->done = 0;
    q->id = asyncns->current_id;
    q->done_next = q->done_prev = NULL;
    q->ret = 0;
    q->_errno = 0;
    q->_h_errno = 0;
    q->addrinfo = NULL;
    q->userdata = NULL;
    q->host = q->serv = NULL;

    return q;
}

_g_asyncns_query_t* _g_asyncns_getaddrinfo(_g_asyncns_t *asyncns, const char *node, const char *service, const struct addrinfo *hints) {
    addrinfo_request_t data[BUFSIZE/sizeof(addrinfo_request_t) + 1];
    addrinfo_request_t *req = data;
    _g_asyncns_query_t *q;
    assert(asyncns);
    assert(node || service);

    if (asyncns->dead) {
        errno = ECHILD;
        return NULL;
    }

    if (!(q = alloc_query(asyncns)))
        return NULL;

    memset(req, 0, sizeof(addrinfo_request_t));

    req->node_len = node ? strlen(node)+1 : 0;
    req->service_len = service ? strlen(service)+1 : 0;

    req->header.id = q->id;
    req->header.type = q->type = REQUEST_ADDRINFO;
    req->header.length = sizeof(addrinfo_request_t) + req->node_len + req->service_len;

    if (req->header.length > BUFSIZE) {
        errno = ENOMEM;
        goto fail;
    }

    if (!(req->hints_is_null = !hints)) {
        req->ai_flags = hints->ai_flags;
        req->ai_family = hints->ai_family;
        req->ai_socktype = hints->ai_socktype;
        req->ai_protocol = hints->ai_protocol;
    }

    if (node)
        strcpy((char*) req + sizeof(addrinfo_request_t), node);

    if (service)
        strcpy((char*) req + sizeof(addrinfo_request_t) + req->node_len, service);

    if (send(asyncns->fds[REQUEST_SEND_FD], req, req->header.length, MSG_NOSIGNAL) < 0)
        goto fail;

    return q;

fail:
    if (q)
        _g_asyncns_cancel(asyncns, q);

    return NULL;
}

int _g_asyncns_getaddrinfo_done(_g_asyncns_t *asyncns, _g_asyncns_query_t* q, struct addrinfo **ret_res) {
    int ret;
    assert(asyncns);
    assert(q);
    assert(q->asyncns == asyncns);
    assert(q->type == REQUEST_ADDRINFO);

    if (asyncns->dead) {
        errno = ECHILD;
        return EAI_SYSTEM;
    }

    if (!q->done)
        return EAI_AGAIN;

    *ret_res = q->addrinfo;
    q->addrinfo = NULL;

    ret = q->ret;

    if (ret == EAI_SYSTEM)
        errno = q->_errno;

    if (ret != 0)
        h_errno = q->_h_errno;

    _g_asyncns_cancel(asyncns, q);

    return ret;
}

_g_asyncns_query_t* _g_asyncns_getnameinfo(_g_asyncns_t *asyncns, const struct sockaddr *sa, socklen_t salen, int flags, int gethost, int getserv) {
    nameinfo_request_t data[BUFSIZE/sizeof(nameinfo_request_t) + 1];
    nameinfo_request_t *req = data;
    _g_asyncns_query_t *q;

    assert(asyncns);
    assert(sa);
    assert(salen > 0);

    if (asyncns->dead) {
        errno = ECHILD;
        return NULL;
    }

    if (!(q = alloc_query(asyncns)))
        return NULL;

    memset(req, 0, sizeof(nameinfo_request_t));

    req->header.id = q->id;
    req->header.type = q->type = REQUEST_NAMEINFO;
    req->header.length = sizeof(nameinfo_request_t) + salen;

    if (req->header.length > BUFSIZE) {
        errno = ENOMEM;
        goto fail;
    }

    req->flags = flags;
    req->sockaddr_len = salen;
    req->gethost = gethost;
    req->getserv = getserv;

    memcpy((uint8_t*) req + sizeof(nameinfo_request_t), sa, salen);

    if (send(asyncns->fds[REQUEST_SEND_FD], req, req->header.length, MSG_NOSIGNAL) < 0)
        goto fail;

    return q;

fail:
    if (q)
        _g_asyncns_cancel(asyncns, q);

    return NULL;
}

int _g_asyncns_getnameinfo_done(_g_asyncns_t *asyncns, _g_asyncns_query_t* q, char *ret_host, size_t hostlen, char *ret_serv, size_t servlen) {
    int ret;
    assert(asyncns);
    assert(q);
    assert(q->asyncns == asyncns);
    assert(q->type == REQUEST_NAMEINFO);
    assert(!ret_host || hostlen);
    assert(!ret_serv || servlen);

    if (asyncns->dead) {
        errno = ECHILD;
        return EAI_SYSTEM;
    }

    if (!q->done)
        return EAI_AGAIN;

    if (ret_host && q->host) {
        strncpy(ret_host, q->host, hostlen);
        ret_host[hostlen-1] = 0;
    }

    if (ret_serv && q->serv) {
        strncpy(ret_serv, q->serv, servlen);
        ret_serv[servlen-1] = 0;
    }

    ret = q->ret;

    if (ret == EAI_SYSTEM)
        errno = q->_errno;

    if (ret != 0)
        h_errno = q->_h_errno;

    _g_asyncns_cancel(asyncns, q);

    return ret;
}

static _g_asyncns_query_t * _g_asyncns_res(_g_asyncns_t *asyncns, query_type_t qtype, const char *dname, int class, int type) {
    res_request_t data[BUFSIZE/sizeof(res_request_t) + 1];
    res_request_t *req = data;
    _g_asyncns_query_t *q;

    assert(asyncns);
    assert(dname);

    if (asyncns->dead) {
        errno = ECHILD;
        return NULL;
    }

    if (!(q = alloc_query(asyncns)))
        return NULL;

    memset(req, 0, sizeof(res_request_t));

    req->dname_len = strlen(dname) + 1;

    req->header.id = q->id;
    req->header.type = q->type = qtype;
    req->header.length = sizeof(res_request_t) + req->dname_len;

    if (req->header.length > BUFSIZE) {
        errno = ENOMEM;
        goto fail;
    }

    req->class = class;
    req->type = type;

    strcpy((char*) req + sizeof(res_request_t), dname);

    if (send(asyncns->fds[REQUEST_SEND_FD], req, req->header.length, MSG_NOSIGNAL) < 0)
        goto fail;

    return q;

fail:
    if (q)
        _g_asyncns_cancel(asyncns, q);

    return NULL;
}

_g_asyncns_query_t* _g_asyncns_res_query(_g_asyncns_t *asyncns, const char *dname, int class, int type) {
    return _g_asyncns_res(asyncns, REQUEST_RES_QUERY, dname, class, type);
}

_g_asyncns_query_t* _g_asyncns_res_search(_g_asyncns_t *asyncns, const char *dname, int class, int type) {
    return _g_asyncns_res(asyncns, REQUEST_RES_SEARCH, dname, class, type);
}

int _g_asyncns_res_done(_g_asyncns_t *asyncns, _g_asyncns_query_t* q, unsigned char **answer) {
    int ret;
    assert(asyncns);
    assert(q);
    assert(q->asyncns == asyncns);
    assert(q->type == REQUEST_RES_QUERY || q->type == REQUEST_RES_SEARCH);
    assert(answer);

    if (asyncns->dead) {
        errno = ECHILD;
        return -ECHILD;
    }

    if (!q->done) {
        errno = EAGAIN;
        return -EAGAIN;
    }

    *answer = (unsigned char *)q->serv;
    q->serv = NULL;

    ret = q->ret;

    if (ret < 0) {
        errno = q->_errno;
        h_errno = q->_h_errno;
    }

    _g_asyncns_cancel(asyncns, q);

    return ret < 0 ? -errno : ret;
}

_g_asyncns_query_t* _g_asyncns_getnext(_g_asyncns_t *asyncns) {
    assert(asyncns);
    return asyncns->done_head;
}

int _g_asyncns_getnqueries(_g_asyncns_t *asyncns) {
    assert(asyncns);
    return asyncns->n_queries;
}

void _g_asyncns_cancel(_g_asyncns_t *asyncns, _g_asyncns_query_t* q) {
    int i;
    int saved_errno = errno;

    assert(asyncns);
    assert(q);
    assert(q->asyncns == asyncns);
    assert(asyncns->n_queries > 0);

    if (q->done) {

        if (q->done_prev)
            q->done_prev->done_next = q->done_next;
        else
            asyncns->done_head = q->done_next;

        if (q->done_next)
            q->done_next->done_prev = q->done_prev;
        else
            asyncns->done_tail = q->done_prev;
    }

    i = q->id % MAX_QUERIES;
    assert(asyncns->queries[i] == q);
    asyncns->queries[i] = NULL;

    _g_asyncns_freeaddrinfo(q->addrinfo);
    free(q->host);
    free(q->serv);

    asyncns->n_queries--;
    free(q);

    errno = saved_errno;
}

void _g_asyncns_freeaddrinfo(struct addrinfo *ai) {
    int saved_errno = errno;

    while (ai) {
        struct addrinfo *next = ai->ai_next;

        free(ai->ai_addr);
        free(ai->ai_canonname);
        free(ai);

        ai = next;
    }

    errno = saved_errno;
}

void _g_asyncns_freeanswer(unsigned char *answer) {
    int saved_errno = errno;

    if (!answer)
        return;

    /* Please note that this function is new in libasyncns 0.4. In
     * older versions you were supposed to free the answer directly
     * with free(). Hence, if this function is changed to do more than
     * just a simple free() this must be considered ABI/API breakage! */

    free(answer);

    errno = saved_errno;
}

int _g_asyncns_isdone(_g_asyncns_t *asyncns, _g_asyncns_query_t*q) {
    assert(asyncns);
    assert(q);
    assert(q->asyncns == asyncns);

    return q->done;
}

void _g_asyncns_setuserdata(_g_asyncns_t *asyncns, _g_asyncns_query_t *q, void *userdata) {
    assert(q);
    assert(asyncns);
    assert(q->asyncns = asyncns);

    q->userdata = userdata;
}

void* _g_asyncns_getuserdata(_g_asyncns_t *asyncns, _g_asyncns_query_t *q) {
    assert(q);
    assert(asyncns);
    assert(q->asyncns = asyncns);

    return q->userdata;
}
