/* CFLAGS+=-pthread */
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/statvfs.h>

#include "fourmat/fourmat.h"

static void *
block_fsstat_worker(void *arg)
{
	static char const DEFAULT_FORMAT[] = "%s%s: %s%s";

	Block *const b = arg;

	struct statvfs st;

	char *pathname = b->arg.str;
	while (-1 == statvfs(pathname, &st)) {
		if (EINTR == errno)
			continue;

		if (EACCES == errno || ENOENT == errno)
			*b->buf = '\0';
		else
			block_str_strerror("failed to stat filesystem");
		goto out;
	}

	char buf[5];
	fmt_space(buf, st.f_bsize * st.f_bavail);
	buf[4] = '\0';

	char *id_symbol;
	switch (st.f_fsid) {
	case 0:  id_symbol = " "; break;
	default: id_symbol = " "; break;
	}

	pthread_rwlock_rdlock(&buf_lock);
	sprintf(b->buf, b->format ? b->format : DEFAULT_FORMAT, id_symbol, strrchr(b->arg.str, '/') + 1, st.f_flag & ST_RDONLY ? "*" : "", buf);
	pthread_rwlock_unlock(&buf_lock);

out:
	close(b->state.num);

	return (void *)EXIT_SUCCESS;
}

BLOCK(fsstat)
{
	pthread_t thread;

	struct pollfd *pfd = BLOCK_POLLFD;
	if (pfd->fd < 0) {
		int pair[2];

		pipe(pair);

		pfd->fd = pair[0];
		pfd->events = POLLIN;

		b->state.num = pair[1];

		pthread_create(&thread, NULL, &block_fsstat_worker, b);
	} else {
		close(pfd->fd);
		pfd->fd = -1;
		b->timeout = 45;
	}
}
