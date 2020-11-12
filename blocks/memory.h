#include <unistd.h>
#include <errno.h>

#include "fourmat/fourmat.h"

DEFINE_BLOCK(memory)
{
	struct {
		int fd;
	} *state;

#define ENTRY(name) name ":", strlen(name ":")

	unsigned long total_kib = 0, avail_kib = 0;
	struct {
		char const *const name;
		size_t const len;
		unsigned long *const val;
	} const entries[] = {
		{ ENTRY("MemTotal"),     &total_kib },
		{ ENTRY("MemAvailable"), &avail_kib },
		{ ENTRY("SwapTotal"),    &total_kib },
		{ ENTRY("SwapFree"),     &avail_kib },
	};

	*b->buf = '\0';

	BLOCK_INIT {
		if ((state->fd = open("/proc/meminfo", O_RDONLY)) < 0) {
			state->fd = 0;
			block_strerror("failed to open /proc/meminfo");
			return;
		}
	}

	char buf[1024];

	if (pread(state->fd, buf, sizeof buf, 0) < 0)
		return;

	size_t i;
	char *p;
	for (i = 0, p = buf;; p = strchr(p, '\n') + 1) {
		if (!memcmp(p, entries[i].name, entries[i].len)) {
			*entries[i].val += strtoul(p + entries[i].len, &p, 10);

			if (ARRAY_SIZE(entries) <= ++i)
				break;
		}
	}

	FORMAT_BEGIN {
	case 'u':
		p += fmt_space(p, (total_kib - avail_kib) * 1024UL);
		continue;

	case 'a':
		p += fmt_space(p, avail_kib * 1024UL);
		continue;

	case 't':
		p += fmt_space(p, total_kib * 1024UL);
		continue;

	case 'p':
		p += fmt_percent(p, avail_kib, total_kib);
		continue;
	} FORMAT_END;

	unsigned const percent = 100 - (unsigned)((100UL * avail_kib) / total_kib);
	b->timeout = (110 - percent) / 10;

#undef ENTRY
}
