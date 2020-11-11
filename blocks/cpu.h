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

	char buf[8192];

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

	while (-1 == pread(state->fd, buf, sizeof buf, 0)) {
		if (EINTR == errno)
			continue;

		fprintf(stderr, "failed read /proc/stat: %s",
				strerror(errno));
		return;
	}

	char *p = buf;
	for (size_t skip_nl = b->arg.num + 1; skip_nl--;)
		p = strchr(p, '\n') + 1;
	p += sizeof("cpu");
	p = strchr(p, ' ') + 1;

	unsigned long long new_total = 0, delta_total;
	unsigned long long new_idle = 0, delta_idle;

	for (uint8_t i = 0; '\n' != *p; ++i) {
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
		uint8_t const percent = 100 - ((100ULL * delta_idle) / delta_total);
		sprintf(b->buf, b->format ? b->format : DEFAULT_FORMAT, percent);
	}

	state->idle = new_idle;
	state->total = new_total;

	b->timeout = 4;
}
