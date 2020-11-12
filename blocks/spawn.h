#include <signal.h>
#include <string.h>
#include <unistd.h>

DEFINE_BLOCK(spawn) {
	struct {
		char line[sizeof b->buf];
		uint32_t line_size;
	} *state;

	struct pollfd *pfd = BLOCK_POLLFD;

	BLOCK_INIT {
		pid_t pid;
		int pair[2];

		for (int sig = 1; sig < SIGRTMAX; ++sig)
			sigaction(sig, &(struct sigaction const){
					 .sa_handler = SIG_DFL
				}, NULL);

		sigset_t sigmask;
		sigemptyset(&sigmask);
		pthread_sigmask(SIG_SETMASK, &sigmask, NULL);

		if (pipe2(pair, O_NONBLOCK | O_CLOEXEC) < 0) {
			block_strerror("failed to create pipe");
			goto fail;
		}

		if (!(pid = fork())) {
			close(STDIN_FILENO);
			dup2(pair[1], STDOUT_FILENO);

			execlp("sh", "sh", "-c", b->arg, NULL);
			block_strerror("failed to exec");
			_exit(127);
		} else if (pid < 0) {
			block_strerror("failed to fork");
			goto fail;
		} else {
			state->line_size = 0;

			(pfd->fd = pair[0]), close(pair[1]);
			pfd->events = POLLIN;
		}
	}

	ssize_t len;
	while (0 < (len = read(pfd->fd, state->line + state->line_size, sizeof state->line - state->line_size))) {
		state->line_size += len;

		char *const p = memrchr(state->line, '\n', state->line_size - 1);
		if (p) {
			len = &state->line[state->line_size] - (p + 1);
			memmove(state->line, p + 1, len);
			state->line_size = len;
		}
	}

	FORMAT_BEGIN {
	case 's':
	case 'S':
		size = state->line_size - (state->line_size && '\n' == state->line[state->line_size - 1]);
		if (!size && 's' == *format)
			break;

		memcpy(p, state->line, size), p += size;
		continue;
	} FORMAT_END;

	if (0 < len) {
		return;
	} else if (!len) {
		goto fail;
	} else {
		if (EAGAIN == errno)
			return;

		block_strerror("failed to read");
		goto fail;
	}

fail:
	if (!(b->timeout = strtoul(b->arg + strlen(b->arg) + 1, NULL, 10)))
		b->timeout = 1;
	close(pfd->fd), pfd->fd = -1;
	BLOCK_UNINIT;
	return;
}
