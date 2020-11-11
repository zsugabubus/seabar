#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

BLOCK(cpu)
{
	static char const DEFAULT_FORMAT[] = "CPU %2d%%";

	struct {
		int fd;
		unsigned long long total, idle;
	} *state;

	char buf[200];

	if (!(state = b->state.ptr)) {
		if (!(state = b->state.ptr = malloc(sizeof(*state))))
			return;

		state->total = 0;
		state->idle = 0;

		while (-1 == (state->fd = open("/proc/stat", O_RDONLY))) {
			if (EINTR == errno)
				continue;

			fprintf(stderr, "failed to open /proc/stat: %s",
					strerror(errno));
			return;
		}
	}

	char *p;
	unsigned i;
	unsigned long long new_total, delta_total;
	unsigned long long new_idle = new_idle, delta_idle;
	unsigned char percent;

	while (-1 == pread(state->fd, buf, sizeof buf, sizeof "cpu "/* skip it */)) {
		if (EINTR == errno)
			continue;

		fprintf(stderr, "failed read /proc/stat: %s",
				strerror(errno));
		return;
	}

	new_total = 0ULL, new_idle = 0ULL;
	for (p = buf, i = 0; '\n' != *p; ++i) {
		unsigned long long const val = strtoull(p, &p, 10);
		new_total += val;
		switch (i) {
		case 3/* idle */:
		case 4/* iowait */:
			new_idle += val;
		}
	}

	delta_idle = new_idle - state->idle;
	if (0 < (delta_total = new_total - state->total)) {
		percent = 100 - ((100ULL * delta_idle) / delta_total);
		sprintf(b->buf, b->format ? b->format : DEFAULT_FORMAT, percent);
	}

	state->idle = new_idle;
	state->total = new_total;

	b->timeout = 4;
}
