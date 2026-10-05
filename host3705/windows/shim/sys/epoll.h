/* sys/epoll.h stand-in for the MSYS2 build of the 3705 emulator: the little of epoll it uses
   (create, add, modify, wait; EPOLLIN and EPOLLONESHOT) done with poll(). */
#ifndef SHIM_EPOLL_H
#define SHIM_EPOLL_H
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#define EPOLLIN      0x001
#define EPOLLOUT     0x004
#define EPOLLERR     0x008
#define EPOLLHUP     0x010
#define EPOLLONESHOT (1u << 30)
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

typedef union epoll_data { void *ptr; int fd; uint32_t u32; uint64_t u64; } epoll_data_t;
struct epoll_event { uint32_t events; epoll_data_t data; };

#define SHIM_EPOLL_MAX 64
struct shim_epoll_set {
	int used;
	int n;
	int fd[SHIM_EPOLL_MAX];
	uint32_t ev[SHIM_EPOLL_MAX];
	epoll_data_t data[SHIM_EPOLL_MAX];
	int armed[SHIM_EPOLL_MAX];
};
#define SHIM_EPOLL_SETS 16
#define SHIM_EPOLL_BASE 100000
static struct shim_epoll_set shim_epoll_sets[SHIM_EPOLL_SETS];
static pthread_mutex_t shim_epoll_lock = PTHREAD_MUTEX_INITIALIZER;

static inline int epoll_create(int size)
{
	(void)size;
	pthread_mutex_lock(&shim_epoll_lock);
	for (int i = 0; i < SHIM_EPOLL_SETS; i++)
		if (!shim_epoll_sets[i].used) {
			shim_epoll_sets[i].used = 1;
			shim_epoll_sets[i].n = 0;
			pthread_mutex_unlock(&shim_epoll_lock);
			return SHIM_EPOLL_BASE + i;
		}
	pthread_mutex_unlock(&shim_epoll_lock);
	errno = EMFILE;
	return -1;
}

static inline struct shim_epoll_set *shim_epoll_get(int epfd)
{
	int i = epfd - SHIM_EPOLL_BASE;
	return (i >= 0 && i < SHIM_EPOLL_SETS && shim_epoll_sets[i].used) ? &shim_epoll_sets[i] : NULL;
}

static inline int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	struct shim_epoll_set *s = shim_epoll_get(epfd);
	int i, rc = 0;
	if (!s) { errno = EBADF; return -1; }
	pthread_mutex_lock(&shim_epoll_lock);
	for (i = 0; i < s->n && s->fd[i] != fd; i++)
		;
	if (op == EPOLL_CTL_ADD) {
		if (i < s->n) { errno = EEXIST; rc = -1; }
		else if (s->n == SHIM_EPOLL_MAX) { errno = ENOMEM; rc = -1; }
		else { s->fd[i] = fd; s->ev[i] = event->events; s->data[i] = event->data; s->armed[i] = 1; s->n++; }
	} else if (op == EPOLL_CTL_MOD) {
		if (i == s->n) { errno = ENOENT; rc = -1; }
		else { s->ev[i] = event->events; s->data[i] = event->data; s->armed[i] = 1; }
	} else if (op == EPOLL_CTL_DEL) {
		if (i == s->n) { errno = ENOENT; rc = -1; }
		else {
			s->n--;
			s->fd[i] = s->fd[s->n]; s->ev[i] = s->ev[s->n]; s->data[i] = s->data[s->n]; s->armed[i] = s->armed[s->n];
		}
	}
	pthread_mutex_unlock(&shim_epoll_lock);
	return rc;
}

static inline int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	struct shim_epoll_set *s = shim_epoll_get(epfd);
	struct pollfd p[SHIM_EPOLL_MAX];
	int idx[SHIM_EPOLL_MAX], n = 0, rc, out = 0;
	if (!s) { errno = EBADF; return -1; }
	pthread_mutex_lock(&shim_epoll_lock);
	for (int i = 0; i < s->n; i++)
		if (s->armed[i]) {
			p[n].fd = s->fd[i];
			p[n].events = ((s->ev[i] & EPOLLIN) ? POLLIN : 0) | ((s->ev[i] & EPOLLOUT) ? POLLOUT : 0);
			p[n].revents = 0;
			idx[n++] = i;
		}
	pthread_mutex_unlock(&shim_epoll_lock);
	rc = poll(p, n, timeout);
	if (rc <= 0)
		return rc;
	pthread_mutex_lock(&shim_epoll_lock);
	for (int j = 0; j < n && out < maxevents; j++)
		if (p[j].revents) {
			int i = idx[j];
			if (i >= s->n || s->fd[i] != p[j].fd)
				continue;
			events[out].events = ((p[j].revents & POLLIN) ? EPOLLIN : 0) | ((p[j].revents & POLLOUT) ? EPOLLOUT : 0)
					| ((p[j].revents & POLLERR) ? EPOLLERR : 0) | ((p[j].revents & POLLHUP) ? EPOLLHUP : 0);
			events[out].data = s->data[i];
			if (s->ev[i] & EPOLLONESHOT)
				s->armed[i] = 0;
			out++;
		}
	pthread_mutex_unlock(&shim_epoll_lock);
	return out;
}

/* closing an epoll descriptor releases the set; any other descriptor is closed for real */
#include <unistd.h>
static inline int shim_epoll_close(int fd)
{
	struct shim_epoll_set *s = shim_epoll_get(fd);
	if (s) { s->used = 0; return 0; }
	return close(fd);
}
#define close(fd) shim_epoll_close(fd)
#endif
