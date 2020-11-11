#include <time.h>

BLOCK(datetime)
{
	static char const DEFAULT_FORMAT[] = "%c";

	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);

	b->timeout = 61 - now.tv_sec % 60;

	strftime(b->buf, sizeof b->buf, b->format ? b->format : DEFAULT_FORMAT, localtime(&now.tv_sec));
}
