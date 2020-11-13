#include "fourmat/fourmat.h"

DEFINE_BLOCK(read)
{
	struct {
		int fd;
	} *state;

	char buf[sizeof b->buf];
	b->timeout = 5;
	*b->buf = '\0';

	BLOCK_INIT {
		if ((state->fd = open(b->arg, O_RDONLY | O_NONBLOCK | O_CLOEXEC)) < 0) {
			state->fd = 0;
			if (ENOENT != errno)
				block_strerror("failed to open for read");
			return;
		}
	}

	ssize_t len = pread(state->fd, buf, sizeof buf, 0);
	if (len < 0) {
		block_strerror("failed to read");
		close(state->fd), state->fd = 0;
		return;
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
}
