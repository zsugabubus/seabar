#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fourmat/fourmat.h"

DEFINE_BLOCK(read)
{
	struct {
		int fd;
	} *state;

	char buf[sizeof b->buf];
	if (!(b->timeout = strtoul(b->arg + strlen(b->arg) + 1, NULL, 10)))
		b->timeout = 5;
	*b->buf = '\0';

	BLOCK_INIT {
		if ((state->fd = open(b->arg, O_RDONLY | O_CLOEXEC)) < 0) {
			if (ENOENT != errno)
				block_strerror("failed to open for read");
			goto fail;
		}
	}

	ssize_t len = pread(state->fd, buf, sizeof buf, 0);
	if (len < 0) {
		block_strerror("failed to read");
		goto fail;
	}

	{
		char *p = memchr(buf, '\n', len);
		if (p)
			len = p - buf;
	}

	FORMAT_BEGIN {
	case 'n': /* number */
		p += fmt_number(p, strtoull(buf, NULL, 10));
		continue;

	case 'i': /* IEC number */
		p += fmt_space(p, strtoull(buf, NULL, 10));
		continue;

	case 's': /* SI number */
		p += fmt_speed(p, strtoull(buf, NULL, 10));
		continue;

	case 's': /* string */
		memcpy(p, buf, len), p += len;
		continue;
	} FORMAT_END;

	return;

fail:
	close(state->fd);
	BLOCK_UNINIT;
}
