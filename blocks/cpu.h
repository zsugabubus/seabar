#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

DEFINE_BLOCK(cpu)
{
	static char const DEFAULT_FORMAT[] = "CPU %2d%%";

	struct {
		int fd;
		unsigned long long total, idle;
	} *state;

	char buf[8192];

	BLOCK_SETUP {
		state->total = 0;
		state->idle = 0;

		while ((state->fd = open("/proc/stat", O_RDONLY)) < 0) {
			if (EINTR == errno)
				continue;

			block_strerror("failed to open /proc/stat");
			return;
		}
	}

	while (pread(state->fd, buf, sizeof buf, 0) < 0) {
		if (EINTR == errno)
			continue;

		block_strerror("failed to read /proc/stat");
		return;
	}

	char *p = buf;
	/* for (size_t skip_nl = b->arg.num + 1; skip_nl--;)
		p = strchr(p, '\n') + 1; */
	p += sizeof("cpu");
	p = strchr(p, ' ') + 1;

	uint64_t new_total = 0, delta_total;
	uint64_t new_idle = 0, delta_idle;

	for (uint8_t i = 0; '\n' != *p; ++i) {
		uint64_t const val = strtoull(p, &p, 10);
		new_total += val;
		switch (i) {
		case 3 /* Idle. */:
		case 4 /* Iowait. */:
			new_idle += val;
		}
	}

	delta_idle = new_idle - state->idle;
	if (0 < (delta_total = new_total - state->total)) {
		FORMAT_BEGIN {
		case 'p': /* Busy percent. */
			p += sprintf(p, "%2u%%", 100U - (unsigned)((UINT64_C(100) * delta_idle) / delta_total));
			continue;

		case 'P': /* Idle percent. */
			p += sprintf(p, "%3u%%", (unsigned)((UINT64_C(100) * delta_idle) / delta_total));
			continue;
		} FORMAT_END;
	}

	state->idle = new_idle;
	state->total = new_total;

	b->timeout = 4;
}
