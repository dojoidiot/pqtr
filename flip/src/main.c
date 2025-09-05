// gcc -Wall -O2 -D_POSIX_C_SOURCE=200809L eq_single.c -o eq_single
// ./eq_single 8080
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

/* ---- Config ---- */
#define IDLE_MS 30000
#define TICK_MS 1000

/* ---- Events ---- */
enum EvType
{
    EV_OPEN,
    EV_READ,
    EV_SEND,
    EV_SHUT,
    EV_DEAD,
    EV_TICK,
    EV_TTY_IN,
    EV_TTY_OUT
};

typedef struct Event
{
    enum EvType type;
    int fd;
    void *payload;
    struct Event *next;
} Event;

typedef struct
{
    Event *head, *tail;
} EvQueue;

static void qpush(EvQueue *q, enum EvType t, int fd, void *payload)
{
    Event *e = (Event *)malloc(sizeof *e);
    e->type = t;
    e->fd = fd;
    e->payload = payload;
    e->next = NULL;
    if (!q->tail)
        q->head = q->tail = e;
    else
    {
        q->tail->next = e;
        q->tail = e;
    }
}
static Event *qpop(EvQueue *q)
{
    Event *e = q->head;
    if (!e)
        return NULL;
    q->head = e->next;
    if (!q->head)
        q->tail = NULL;
    return e;
}

/* ---- Time ---- */
static inline uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)(ts.tv_nsec / 1000000ull);
}

/* ---- Send payload ---- */
typedef struct
{
    char *buf;
    size_t len, off;
} SendBuf;
static SendBuf *mk_sendbuf(const char *s)
{
    size_t n = strlen(s);
    SendBuf *sb = malloc(sizeof *sb);
    sb->buf = malloc(n);
    memcpy(sb->buf, s, n);
    sb->len = n;
    sb->off = 0;
    return sb;
}
static SendBuf *mk_sendbuf_n(const char *s, size_t n)
{
    SendBuf *sb = malloc(sizeof *sb);
    sb->buf = malloc(n);
    memcpy(sb->buf, s, n);
    sb->len = n;
    sb->off = 0;
    return sb;
}
static void free_sendbuf(SendBuf *sb)
{
    if (!sb)
        return;
    free(sb->buf);
    free(sb);
}

/* ---- Connections ---- */
typedef struct Conn
{
    int fd;
    uint64_t last_active_ms;
    struct Conn *next_all;
} Conn;

#define MAXFDS 65536
static Conn *conns[MAXFDS];
static Conn *all_conns = NULL;

static Conn *mkconn(int fd)
{
    Conn *c = calloc(1, sizeof *c);
    c->fd = fd;
    c->last_active_ms = now_ms();
    c->next_all = all_conns;
    all_conns = c;
    return conns[fd] = c;
}
static void rmconn(int fd)
{
    Conn *c = conns[fd];
    if (!c)
        return;
    if (all_conns == c)
        all_conns = c->next_all;
    else
    {
        for (Conn *p = all_conns; p && p->next_all; p = p->next_all)
            if (p->next_all == c)
            {
                p->next_all = c->next_all;
                break;
            }
    }
    free(c);
    conns[fd] = NULL;
}

/* ---- FD helpers ---- */
static int set_nb(int fd)
{
    int f = fcntl(fd, F_GETFL, 0);
    if (f < 0)
        return -1;
    return fcntl(fd, F_SETFL, f | O_NONBLOCK);
}

/* ---- TTY helpers ---- */
static void tty_print(EvQueue *q, const char *s) { qpush(q, EV_TTY_OUT, 1, mk_sendbuf(s)); }
static void tty_printf(EvQueue *q, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if ((size_t)n > sizeof buf)
        n = (int)sizeof(buf);
    qpush(q, EV_TTY_OUT, 1, mk_sendbuf_n(buf, (size_t)n));
}

/* ---- Idle reap ---- */
static void app_reap_idle(uint64_t now, EvQueue *q)
{
    for (Conn *c = all_conns; c; c = c->next_all)
        if (now - c->last_active_ms > IDLE_MS)
            qpush(q, EV_DEAD, c->fd, NULL);
}

/* ---- Shell helpers ---- */
static void trim(char *s)
{
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = 0;
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i]))
        i++;
    if (i)
        memmove(s, s + i, strlen(s + i) + 1);
}
static void cmd_list(EvQueue *q)
{
    tty_print(q, "FDs: ");
    for (Conn *c = all_conns; c; c = c->next_all)
        tty_printf(q, "%d ", c->fd);
    tty_print(q, "\n");
}
static void cmd_send(EvQueue *q, int fd, const char *text)
{
    if (fd < 0 || fd >= MAXFDS || !conns[fd])
    {
        tty_printf(q, "No such fd: %d\n", fd);
        return;
    }
    SendBuf *sb = mk_sendbuf(text);
    qpush(q, EV_SEND, fd, sb);
    tty_printf(q, "queued %zu bytes to fd %d\n", sb->len, fd);
}
static void cmd_kill(EvQueue *q, int fd)
{
    if (fd < 0 || fd >= MAXFDS || !conns[fd])
    {
        tty_printf(q, "No such fd: %d\n", fd);
        return;
    }
    qpush(q, EV_DEAD, fd, NULL);
    tty_printf(q, "kill queued for fd %d\n", fd);
}

/* ---- App processing (single-thread) ---- */
static int g_should_exit = 0;

static void app_process(Event *e, int epfd, EvQueue *q)
{
    Conn *c = (e->fd >= 0 && e->fd < MAXFDS) ? conns[e->fd] : NULL;

    switch (e->type)
    {
    case EV_TICK:
    {
        app_reap_idle(now_ms(), q);
        break;
    }

    case EV_OPEN:
        if (c)
            c->last_active_ms = now_ms();
        tty_printf(q, "[open] fd=%d\n", e->fd);
        break;

    case EV_READ:
    {
        if (!c)
            break;
        c->last_active_ms = now_ms();
        char buf[4096];
        for (;;)
        {
            ssize_t n = recv(e->fd, buf, sizeof buf, 0);
            if (n > 0)
            {
                c->last_active_ms = now_ms();
                /* Your protocol here; demo: echo line */
                qpush(q, EV_SEND, e->fd, mk_sendbuf("echo\n"));
            }
            else if (n == 0)
            {
                qpush(q, EV_DEAD, e->fd, NULL);
                break;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else if (errno == EINTR)
                continue;
            else
            {
                qpush(q, EV_DEAD, e->fd, NULL);
                break;
            }
        }
        break;
    }

    case EV_SEND:
    {
        SendBuf *sb = (SendBuf *)e->payload;
        if (!c)
        {
            free_sendbuf(sb);
            break;
        }
        c->last_active_ms = now_ms();
        while (sb->off < sb->len)
        {
            ssize_t n = send(e->fd, sb->buf + sb->off, sb->len - sb->off, MSG_NOSIGNAL);
            if (n > 0)
            {
                sb->off += (size_t)n;
                c->last_active_ms = now_ms();
                continue;
            }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                qpush(q, EV_SEND, e->fd, sb);
                return;
            }
            if (errno == EINTR)
                continue;
            free_sendbuf(sb);
            qpush(q, EV_DEAD, e->fd, NULL);
            return;
        }
        free_sendbuf(sb);
        break;
    }

    case EV_SHUT:
        if (c)
        {
            c->last_active_ms = now_ms();
            shutdown(e->fd, SHUT_WR);
        }
        break;

    case EV_DEAD:
        epoll_ctl(epfd, EPOLL_CTL_DEL, e->fd, NULL);
        close(e->fd);
        rmconn(e->fd);
        tty_printf(q, "[dead] fd=%d\n", e->fd);
        break;

    case EV_TTY_IN:
    {
        char buf[1024];
        for (;;)
        {
            ssize_t n = read(0, buf, sizeof buf);
            if (n > 0)
            {
                size_t off = 0;
                while (off < (size_t)n)
                {
                    char *nl = memchr(buf + off, '\n', n - off);
                    size_t len = nl ? (size_t)(nl - (buf + off)) : (size_t)(n - off);
                    char line[1024];
                    size_t cp = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
                    memcpy(line, buf + off, cp);
                    line[cp] = 0;
                    trim(line);
                    if (line[0])
                    {
                        if (!strcmp(line, "quit"))
                        {
                            tty_print(q, "bye\n");
                            g_should_exit = 1;
                        }
                        else if (!strcmp(line, "list"))
                            cmd_list(q);
                        else if (!strncmp(line, "kill ", 5))
                        {
                            int fd = atoi(line + 5);
                            cmd_kill(q, fd);
                        }
                        else if (!strncmp(line, "send ", 5))
                        {
                            char *p = (char *)line + 5;
                            while (*p && isspace((unsigned char)*p))
                                p++;
                            int fd = atoi(p);
                            while (*p && !isspace((unsigned char)*p))
                                p++;
                            while (*p && isspace((unsigned char)*p))
                                p++;
                            if (*p)
                                cmd_send(q, fd, p);
                            else
                                tty_print(q, "usage: send <fd> <text>\n");
                        }
                        else
                        {
                            tty_print(q, "commands: list | send <fd> <text> | kill <fd> | quit\n");
                        }
                    }
                    if (!nl)
                        break;
                    off += len + 1;
                }
            }
            else if (n == 0)
            { /* stdin closed */
                break;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else if (errno == EINTR)
                continue;
            else
                break;
        }
        break;
    }

    case EV_TTY_OUT:
    {
        SendBuf *sb = (SendBuf *)e->payload;
        while (sb->off < sb->len)
        {
            ssize_t n = write(1, sb->buf + sb->off, sb->len - sb->off);
            if (n > 0)
            {
                sb->off += (size_t)n;
                continue;
            }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                qpush(q, EV_TTY_OUT, 1, sb);
                return;
            }
            if (errno == EINTR)
                continue;
            break;
        }
        free_sendbuf(sb);
        break;
    }
    }
}

/* ---- Main (single thread) ---- */
int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return 2;
    }
    int port = atoi(argv[1]);

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0)
    {
        perror("socket");
        return 1;
    }
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    set_nb(lfd);

    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(port);
    if (bind(lfd, (struct sockaddr *)&a, sizeof a) || listen(lfd, 128))
    {
        perror("bind/listen");
        return 1;
    }

    int epfd = epoll_create1(0);
    struct epoll_event ev = {.events = EPOLLIN, .data.u32 = (uint32_t)lfd};
    epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);

    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    struct itimerspec its = {{0}};
    its.it_value.tv_sec = TICK_MS / 1000;
    its.it_value.tv_nsec = (TICK_MS % 1000) * 1000000L;
    its.it_interval = its.it_value;
    timerfd_settime(tfd, 0, &its, NULL);
    struct epoll_event tev = {.events = EPOLLIN, .data.u32 = (uint32_t)tfd};
    epoll_ctl(epfd, EPOLL_CTL_ADD, tfd, &tev);

    set_nb(0);
    set_nb(1);
    struct epoll_event sev_in = {.events = EPOLLIN, .data.u32 = (uint32_t)0};
    epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &sev_in);

    EvQueue q = {0};
    struct epoll_event evs[256];

    tty_print(&q, "shell ready. commands: list | send <fd> <text> | kill <fd> | quit\n");

    for (;;)
    {
        if (g_should_exit)
            break;
        int n = epoll_wait(epfd, evs, 256, -1);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            perror("epoll_wait");
            break;
        }
        for (int i = 0; i < n; i++)
        {
            int fd = (int)evs[i].data.u32;
            uint32_t re = evs[i].events;

            if (fd == lfd)
            {
                for (;;)
                {
                    int cfd = accept(lfd, NULL, NULL);
                    if (cfd < 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        if (errno == EINTR)
                            continue;
                        perror("accept");
                        break;
                    }
                    set_nb(cfd);
                    mkconn(cfd);
                    struct epoll_event cev = {.events = EPOLLIN | EPOLLET | EPOLLOUT, .data.u32 = (uint32_t)cfd};
                    epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &cev);
                    qpush(&q, EV_OPEN, cfd, NULL);
                }
                continue;
            }

            if (fd == tfd)
            {
                uint64_t expirations;
                ssize_t r;
                do
                {
                    r = read(tfd, &expirations, sizeof expirations);
                } while (r < 0 && errno == EINTR);
                if (r == (ssize_t)sizeof expirations)
                    qpush(&q, EV_TICK, -1, NULL);
                continue;
            }

            if (fd == 0)
            {
                if (re & EPOLLIN)
                    qpush(&q, EV_TTY_IN, 0, NULL);
                continue;
            }

            if (re & (EPOLLERR | EPOLLHUP))
                qpush(&q, EV_DEAD, fd, NULL);
            if (re & EPOLLIN)
                qpush(&q, EV_READ, fd, NULL);
            // Optionally: if you track "pending write", push EV_SEND on EPOLLOUT
        }

        /* Strict single-thread, single-event processing */
        Event *e = qpop(&q);
        if (e)
        {
            app_process(e, epfd, &q);
            free(e);
        }
    }

    close(tfd);
    close(epfd);
    close(lfd);
    return 0;
}
