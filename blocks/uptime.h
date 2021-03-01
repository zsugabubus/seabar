#include <string.h>
#include <time.h>

#include "fourmat/fourmat.h"

DEFINE_BLOCK(uptime)
{
	struct timespec uptime;
	char szuptime[5];

	clock_gettime(
#if defined(CLOCK_BOOTTIME)
			CLOCK_BOOTTIME
#elif defined(CLOCK_UPTIME)
			CLOCK_UPTIME
#else
			CLOCK_MONOTONIC
#endif
			, &uptime);

	fmt_time(szuptime, uptime.tv_sec);

	FORMAT_BEGIN {
	case 't': /* Uptime. */
		memcpy(p, szuptime, sizeof szuptime), p += sizeof szuptime;
		continue;
	} FORMAT_END;

	switch (szuptime[4]) {
	case 's':
		b->timeout = 1;
		break;

	case 'm':
		b->timeout = 60;
		break;

	default:
		b->timeout = 60 * 60;
		break;
	}
}
