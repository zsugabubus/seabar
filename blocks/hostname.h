#include <unistd.h>

DEFINE_BLOCK(hostname)
{
	char host[HOST_NAME_MAX];

	if (-1 == gethostname(host, sizeof host)) {
		*b->buf = '\0';
		block_strerror("failed to get name of host");
	}

	size_t const host_size = strlen(host);

	FORMAT_BEGIN {
	case 'n':
		memcpy(p, host, host_size), p += host_size;
		continue;
	} FORMAT_END
}
