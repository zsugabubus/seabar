#include <unistd.h>
#include <string.h>

DEFINE_BLOCK(hostname)
{
	char host[HOST_NAME_MAX];

	if (gethostname(host, sizeof host) < 0) {
		*host = '\0';
		block_strerror("failed to get name of host");
	}

	FORMAT_BEGIN {
	case 'n': /* hostname */
		if (!(size = strlen(host)))
			break;

		memcpy(p, host, size), p += size;
		continue;
	} FORMAT_END
}
