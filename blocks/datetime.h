#include <time.h>

DEFINE_BLOCK(datetime)
{
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);

	b->timeout = 61 - now.tv_sec % 60;

	strftime(b->buf, sizeof b->buf, b->format, localtime(&now.tv_sec));
}
