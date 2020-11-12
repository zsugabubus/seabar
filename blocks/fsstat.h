/* CFLAGS+=-pthread */
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/statvfs.h>

#include "fourmat/fourmat.h"

struct block_fsstat_state {
	int fd;
};

static void *
block_fsstat_worker(void *arg)
{
	Block *const b = arg;
	DEFINE_BLOCK_STATE(struct block_fsstat_state);

	struct statvfs st;
	int const res = statvfs(b->arg, &st);

	pthread_rwlock_rdlock(&buf_lock);
	FORMAT_BEGIN {
	case 'i':
	{
		if (-1 == res)
			break;

		char const *icon;
		switch (st.f_fsid) {
		case 0:  icon = " "; break;
		default: icon = " "; break;
		}
		size_t const icon_size = strlen(icon);
		memcpy(p, icon, icon_size), p += icon_size;
	}
		continue;

	case 'n':
	{
		char const *name = strrchr(b->arg, '/');
		name = name ? name + 1 : b->arg;
		size_t const name_size = strlen(name);
		memcpy(p, name, name_size), p += name_size;
	}
		continue;

	case 'a':
		if (-1 == res)
			break;

		p += fmt_space(p, st.f_bsize * st.f_bavail);
		continue;

	case 'f':
		if (-1 == res)
			break;

		p += fmt_space(p, st.f_bsize * st.f_bfree);
		continue;

	case 't':
		if (-1 == res)
			break;

		p += fmt_space(p, st.f_frsize * st.f_blocks);
		continue;

	case 'p':
		if (-1 == res)
			break;

		p += fmt_percent(p, st.f_frsize * st.f_blocks - st.f_bsize * st.f_bavail, st.f_frsize * st.f_blocks);
		continue;

	case 'P':
		if (-1 == res)
			break;

		p += fmt_percent(p, st.f_bsize * st.f_bavail, st.f_frsize * st.f_blocks);
		continue;

	case 'F':
		if (-1 == res)
			break;

		*p++ = st.f_flag & ST_RDONLY ? '-' : 'w';
		*p++ = st.f_flag & ST_NOEXEC ? '-' : 'x';
		*p++ = st.f_flag & ST_NOSUID ? '-' : 's';
		continue;

	case 'c':
		if (-1 == res)
			break;

		p += sprintf(p, "%ld", st.f_files);
		continue;

	case 'r':
		if (-1 == res)
			break;

		if (st.f_flag & ST_RDONLY)
			*p++ = '*';
		continue;
	} FORMAT_END;
	pthread_rwlock_unlock(&buf_lock);

out:
	close(state->fd);
	BLOCK_UNINIT;

	return (void *)EXIT_SUCCESS;
}

DEFINE_BLOCK(fsstat)
{
	pthread_t thread;

	struct pollfd *pfd = BLOCK_POLLFD;
	if (pfd->fd < 0) {
		DEFINE_BLOCK_STATE(struct block_fsstat_state);
		int pair[2];

		pipe(pair);

		pfd->fd = pair[0];
		pfd->events = POLLIN;

		state->fd = pair[1];

		pthread_create(&thread, NULL, &block_fsstat_worker, b);
	} else {
		close(pfd->fd);
		pfd->fd = -1;
		b->timeout = 45;
	}
}
