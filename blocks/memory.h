#include <unistd.h>
#include <errno.h>

#include "fourmat/fourmat.h"

BLOCK(memory)
{
	static char const DEFAULT_FORMAT[] = "MEM %si/%si (%s)";

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

	if (!b->state.num) {
		if (-1 == (b->state.num = open("/proc/meminfo", O_RDONLY))) {
			block_strerrorf("failed to open %s", "/proc/meminfo");
			return;
		}
	}

	char buf[1024];

	if (-1 == pread(b->state.num, buf, sizeof buf - 1, 0))
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

	char szused[5], sztotal[5], szpercent[6];
	fmt_space(sztotal, total_kib * 1024UL), sztotal[4] = '\0';
	fmt_space(szused, (total_kib - avail_kib) * 1024UL), szused[4] = '\0';
	fmt_percent(szpercent, avail_kib, total_kib), szpercent[5] = '\0';

	sprintf(b->buf, b->format ? b->format : DEFAULT_FORMAT,
			szused, sztotal, szpercent);

	unsigned const percent = 100 - (unsigned)((100UL * avail_kib) / total_kib);
	b->timeout = (110 - percent) / 10;

#undef ENTRY
}
