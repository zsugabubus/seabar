#define _GNU_SOURCE
#include <limits.h>
#include <locale.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#include <wordexp.h>

#define DEFINE_BLOCK(name) \
	static void block_##name(Block *const b)

/* initialize internal state of block */
#define BLOCK_SETUP /* { ... } */ \
	if (sizeof *state <= sizeof b->state ? (state = (void *)&b->state, !b->state) : !(state = b->state)) \
		if (sizeof b->state < sizeof *state && !(state = b->state = malloc(sizeof *state))) \
			return; \
		else for (bool once_ = true; once_; once_ = false)

#define BLOCK_TEARDOWN do { \
	if (sizeof b->state < sizeof *state) \
		free(b->state); \
	b->state = NULL; \
} while (0)

#define DEFINE_BLOCK_STATE(type) \
	type *state = (sizeof(type) <= sizeof(b->state) ? &b->state : b->state)

#define FORMAT_BEGIN \
{ \
	char *p = b->buf; \
	for (char *format = b->format; *format && '\t' != *format; ++format) { \
		if ('%' == *format) { \
			switch (*++format)
			/* { ... } */
#define FORMAT_END \
			p = b->buf; \
			if (!(format = strchr(format, '\t'))) \
				break; \
		} else { \
			*p++ = *format; \
		} \
	} \
	*p = '\0'; \
}

#define BLOCK_POLLFD (&fds[b - blocks])

#define ARRAY_SIZE(x) (sizeof x / sizeof *x)

#define NSEC_PER_SEC UINT64_C(1000000000)
#define TS_SEC(sec) (struct timespec){ .tv_sec = (sec), .tv_nsec = 0 }
#define TS_ZERO TS_SEC(0)

#define block_errorf(msg, ...) fprintf(stderr, "[%s \"%s\"] " msg "\n", __FUNCTION__, b->arg, __VA_ARGS__)
#define block_strerror(msg) block_errorf(msg ": %s", strerror(errno))

typedef struct Block Block;
struct Block {
	unsigned const group;
	void(*const poll)(Block *);
	char *arg;
	char *const format;
	void *state;
	unsigned timeout;
	char buf[128];
};

static pthread_rwlock_t buf_lock = PTHREAD_RWLOCK_INITIALIZER;

static Block *blocks;
static size_t num_blocks;
static struct pollfd *fds;

#define ANSI_BOLD(text) "\e[1m" text "\e[21m"
#define ANSI_RGB(r, g, b, text) "\e[38;2;" #r ";" #g ";" #b "m" text "\e[m"

static void init(void);

static int
ts_cmp(struct timespec const *const __restrict__ lhs, struct timespec const *const __restrict__ rhs)
{
	if (lhs->tv_sec != rhs->tv_sec)
		return lhs->tv_sec < rhs->tv_sec ? -1 : 1;
	else if (lhs->tv_nsec != rhs->tv_nsec)
		return lhs->tv_nsec < rhs->tv_nsec ? -1 : 1;
	else
		return 0;
}

static struct timespec
ts_sub(struct timespec const *const __restrict__ lhs, struct timespec const *const __restrict__ rhs)
{
	return (struct timespec){
		.tv_sec = lhs->tv_sec - rhs->tv_sec - (lhs->tv_nsec < rhs->tv_nsec),
		.tv_nsec = (lhs->tv_nsec < rhs->tv_nsec ? NSEC_PER_SEC : 0) + lhs->tv_nsec - rhs->tv_nsec,
	};
}

static int
sprint(char **p, char const *str)
{
	size_t str_size = str ? strlen(str) : 0;
	if (!str_size)
		return 0;

	memcpy(*p, str, str_size);
	*p += str_size;

	return 1;
}

#include "config.blocks.h"
#include "config.h"

static void
sig_handler(int signum)
{
	(void)signum;
}

int
main(int argc, char *argv[])
{
	setpriority(PRIO_PROCESS, 0, PRIO_MAX);

	struct sigaction sa;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP | SA_NOCLDWAIT;

	sa.sa_handler = SIG_DFL;
	sigaction(SIGCHLD, &sa, NULL);

	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);

	sa.sa_handler = sig_handler;
	sigaction(SIGWINCH, &sa, NULL);

	sigset_t mask;
	sigfillset(&mask);
	sigdelset(&mask, SIGCHLD);
	sigdelset(&mask, SIGHUP);
	sigdelset(&mask, SIGINT);
	sigdelset(&mask, SIGKILL);
	pthread_sigmask(SIG_SETMASK, &mask, NULL);

	setlocale(LC_ALL, "");

	char stdout_buf[BUFSIZ];
	setvbuf(stdout, stdout_buf, _IOFBF, sizeof stdout_buf);

	sigemptyset(&mask);
	sigaddset(&mask, SIGPIPE);

	init();

	fds = alloca(sizeof *fds * num_blocks);
	for (size_t i = 0; i < num_blocks; ++i)
		fds[i].fd = -1;

	wordexp_t we = { 0 };
	for (size_t i = 0; i < num_blocks; ++i) {
		Block *const b = &blocks[i];
		int err = 0;

		if (b->arg && !(err = wordexp(b->arg, &we, WRDE_SHOWERR | WRDE_REUSE)) && 0 < we.we_wordc) {
			size_t size = 0;
			for (size_t i = 0; i < we.we_wordc; ++i)
				size += strlen(we.we_wordv[i]) + 1;

			char *p = b->arg = malloc(size);

			for (size_t i = 0; i < we.we_wordc; ++i) {
				char const *const word = we.we_wordv[i];
				memcpy(p, word, (size = strlen(word))), p += size;
				*p++ = ' ';
			}
			p[-1] = '\0';
		} else if (WRDE_SYNTAX == err)
			block_errorf("invalid syntax", NULL);
	}
	if (0 < num_blocks)
		wordfree(&we);

	bool const is_tty = isatty(STDOUT_FILENO);

	if (is_tty)
		fputs("\e[s", stdout);

	struct timespec timeout = TS_ZERO;

	for (;;) {
		struct timespec start;

		if (is_tty)
			fputs("\e[u", stdout);

		pthread_rwlock_wrlock(&buf_lock);
		unsigned group = 0;
		for (size_t i = 0; i < num_blocks; ++i) {
			Block *const b = &blocks[i];

			if (b->buf[0]) {
				if (group)
					fputs(group != b->group ? GROUP_SEP : BLOCK_SEP, stdout);
				fputs(b->buf, stdout);
				/* printf("[#%uT%u]", i, b->timeout); */

				group = b->group;
			}
		}
		pthread_rwlock_unlock(&buf_lock);

		fputs(is_tty ? "\e[K" : "\n", stdout);
		if (EOF == fflush(stdout))
			break;

		clock_gettime(
#ifdef CLOCK_MONOTONIC_COARSE
				CLOCK_MONOTONIC_COARSE
#else
				CLOCK_MONOTONIC
#endif
				, &start);

		struct timespec elapsed;
		int res = ppoll(fds, num_blocks, &timeout, &mask);
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
			elapsed = ts_sub(&elapsed, &start);

			if (ts_cmp(&timeout, &elapsed) >= 0)
				timeout = ts_sub(&timeout, &elapsed);
			else
				timeout = TS_ZERO;
		}

		for (size_t i = 0; i < num_blocks; ++i) {
			Block *const b = &blocks[i];

			if (fds[i].revents || b->timeout <= elapsed.tv_sec) {
				b->timeout = UINT_MAX;
				b->poll(b);
			} else if (b->timeout > elapsed.tv_sec) {
				b->timeout -= elapsed.tv_sec;
			}

			if (0 == timeout.tv_sec || b->timeout < timeout.tv_sec)
				timeout = TS_SEC(b->timeout);
		}

		if (0 == timeout.tv_sec)
			timeout.tv_sec = -1;
	}
}
