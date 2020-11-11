#define _GNU_SOURCE
#include <limits.h>
#include <locale.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BLOCK(name) \
	static void block_##name(Block *const b)

#define ARRAY_SIZE(x) (sizeof x / sizeof *x)

#define NSEC_PER_SEC UINT64_C(1000000000)
#define TS_SEC(sec) (struct timespec){ .tv_sec = (sec), .tv_nsec = 0 }
#define TS_ZERO TS_SEC(0)

static pthread_rwlock_t buf_lock = PTHREAD_RWLOCK_INITIALIZER;

typedef struct block Block;
struct block {
	unsigned const group;
	char *const format;
	void(*const poll)(Block *);
	union {
		void *ptr;
		char *str;
		int num;
	} arg, state;
	unsigned timeout;
	char buf[128];
};

static Block BAR[];
static struct pollfd fds[];

static struct timespec elapsed;

#include "config.blocks.h"
#include "config.h"

static struct pollfd fds[ARRAY_SIZE(BAR)];

int
ts_cmp(struct timespec const *const __restrict__ lhs, struct timespec const *const __restrict__ rhs)
{
	if (lhs->tv_sec != rhs->tv_sec)
		return lhs->tv_sec < rhs->tv_sec ? -1 : 1;
	else if (lhs->tv_nsec != rhs->tv_nsec)
		return lhs->tv_nsec < rhs->tv_nsec ? -1 : 1;
	else
		return 0;
}

void
ts_sub(struct timespec *const __restrict__ lhs, struct timespec const *const __restrict__ rhs)
{
	lhs->tv_sec = lhs->tv_sec - rhs->tv_sec - (lhs->tv_nsec < rhs->tv_nsec);
	lhs->tv_nsec = (lhs->tv_nsec < rhs->tv_nsec ? NSEC_PER_SEC : 0) + lhs->tv_nsec - rhs->tv_nsec;
}

int
main(int argc, char *argv[])
{
	sigset_t mask;

	struct sigaction sa;

	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP | SA_NOCLDWAIT;

	sa.sa_handler = SIG_DFL;
	sigaction(SIGCHLD, &sa, NULL);

	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);

	sigfillset(&mask);
	sigdelset(&mask, SIGPIPE);
	sigdelset(&mask, SIGINT);
	sigdelset(&mask, SIGHUP);
	pthread_sigmask(SIG_SETMASK, &mask, NULL);

	setlocale(LC_ALL, "");

	struct timespec timeout = TS_ZERO;

	for (size_t i = 0; i < ARRAY_SIZE(BAR); ++i)
		fds[i].fd = -1;

	sigemptyset(&mask);

	for (;;) {
		struct timespec start;

		fputs("\r", stdout);

		pthread_rwlock_wrlock(&buf_lock);
		unsigned group = 0;
		for (size_t i = 0; i < ARRAY_SIZE(BAR); ++i) {
			Block *const b = &BAR[i];

			if (b->buf[0]) {
				if (group)
					fputs(group != b->group ? GROUP_SEP : BLOCK_SEP, stdout);
				fputs(b->buf, stdout);
				/* printf("[#%uT%u]", i, b->timeout); */

				group = b->group;
			}
		}
		pthread_rwlock_unlock(&buf_lock);

		fputs("\e[K", stdout);
		fflush(stdout);

		clock_gettime(
#ifdef CLOCK_MONOTONIC_COARSE
			CLOCK_MONOTONIC_COARSE
#else
			CLOCK_MONOTONIC
#endif
			, &start);

		/* fprintf(stderr, "\n\rtimeout %ld s %09ld ns", timeout.tv_sec, timeout.tv_nsec); */
		int res = ppoll(fds, ARRAY_SIZE(fds), &timeout, &mask);

		if (0 == res) {
			elapsed = timeout;
			timeout = TS_ZERO;
		} else {
			clock_gettime(
#ifdef CLOCK_MONOTONIC_COARSE
				CLOCK_MONOTONIC_COARSE
#else
				CLOCK_MONOTONIC
#endif
				, &elapsed);
			ts_sub(&elapsed, &start);

			if (ts_cmp(&timeout, &elapsed) <= 0)
				ts_sub(&timeout, &elapsed);
			else
				timeout = TS_ZERO;
		}
		/* fprintf(stderr, "\r\nelapsed %ld s %09lld ns\n", elapsed.tv_sec, elapsed.tv_nsec); */

		for (size_t i = 0; i < ARRAY_SIZE(BAR); ++i) {
			Block *const b = &BAR[i];

			if (fds[i].revents || b->timeout <= elapsed.tv_sec) {
				/* fprintf(stderr, "updating block #%d\n", i); */
				b->timeout = UINT_MAX;
				b->poll(b);

			} else if (b->timeout > elapsed.tv_sec)
				b->timeout -= elapsed.tv_sec;

			if (0 == timeout.tv_sec || b->timeout < timeout.tv_sec)
				timeout = TS_SEC(b->timeout);
		}

		if (0 == timeout.tv_sec)
			timeout.tv_sec = -1;
	}
}
