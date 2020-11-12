#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

DEFINE_BLOCK(cpu)
{
	static char const DEFAULT_FORMAT[] = "CPU %2d%%";

	struct {
		int fd;
		unsigned long long total, idle;
	} *state;

	char buf[8192];

	BLOCK_INIT {
		state->total = 0;
		state->idle = 0;

		while ((state->fd = open("/proc/stat", O_RDONLY)) < 0) {
			if (EINTR == errno)
				continue;

			fprintf(stderr, "failed to open /proc/stat: %s",
					strerror(errno));
			return;
		}
	}

	while (pread(state->fd, buf, sizeof buf, 0) < 0) {
		if (EINTR == errno)
			continue;

		fprintf(stderr, "failed read /proc/stat: %s",
				strerror(errno));
		return;
	}

	char *p = buf;
	/* for (size_t skip_nl = b->arg.num + 1; skip_nl--;)
		p = strchr(p, '\n') + 1; */
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
		FORMAT_BEGIN {
		case 'p':
			p += sprintf(p, "%2d%%", 100 - ((100ULL * delta_idle) / delta_total));
			continue;
		} FORMAT_END;
	}

	state->idle = new_idle;
	state->total = new_total;

	b->timeout = 4;
}
