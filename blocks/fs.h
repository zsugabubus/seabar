/* CFLAGS+=-pthread */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "fourmat/fourmat.h"

struct block_fs_state {
	int fd;
};

static void *
block_fs_worker(void *arg)
{
	Block *const b = arg;
	DEFINE_BLOCK_STATE(struct block_fs_state);

	struct statvfs st;
	int const res = statvfs(b->arg, &st);

	uint64_t const total_size = (uint64_t)st.f_blocks * st.f_frsize;
	uint64_t const avail_size = (uint64_t)st.f_bavail * st.f_bsize;
	uint64_t const free_size  = (uint64_t)st.f_bfree * st.f_bsize;

	pthread_rwlock_rdlock(&buf_lock);
	FORMAT_BEGIN {
	case 'i': /* basic fs icon */
	{
		if (res < 0)
			break;

		char const *icon;
		switch (st.f_fsid) {
		case 0/* tmpfs */: icon = "\xef\x82\xae "; break;
		default:           icon = "\xef\x9f\x89 "; break;
		}
		size_t const icon_size = strlen(icon);
		memcpy(p, icon, icon_size), p += icon_size;
	}
		continue;

	case 'n': /* name */
	{
		char const *name = strrchr(b->arg, '/');
		name = name ? name + 1 : b->arg;
		size_t const name_size = strlen(name);
		memcpy(p, name, name_size), p += name_size;
	}
		continue;

	case 'a': /* available (free space for unprivileged users) */
		if (res < 0)
			break;

		p += fmt_space(p, avail_size);
		continue;

	case 'P': /* available percent */
		if (res < 0)
			break;

		p += fmt_percent(p, avail_size, total_size);
		continue;

	case 'f': /* free */
	{
		if (res < 0)
			break;

		p += fmt_space(p, free_size);
	}
		continue;

	case 'u': /* used */
		if (res < 0)
			break;

		p += fmt_space(p, total_size - free_size);
		continue;

	case 'p': /* used percent */
		if (res < 0)
			break;

		p += fmt_percent(p, total_size - avail_size, total_size);
		continue;

	case 't': /* total */
		if (res < 0)
			break;

		p += fmt_space(p, total_size);
		continue;

	case 'F': /* flags */
		if (res < 0)
			break;

		*p++ = st.f_flag & ST_RDONLY ? '-' : 'w';
		*p++ = st.f_flag & ST_NOEXEC ? '-' : 'x';
		*p++ = st.f_flag & ST_NOSUID ? '-' : 's';
		continue;

	case 'c': /* file count */
		if (res < 0)
			break;

		p += sprintf(p, "%ld", st.f_files);
		continue;

	case 'r': /* '*' if read-only */
		if (res < 0)
			break;

		if (st.f_flag & ST_RDONLY)
			*p++ = '*';
		continue;

	case 'R': /* '[RO]' if read-only */
		if (res < 0)
			break;

		if (st.f_flag & ST_RDONLY)
			memcpy(p, "[RO]", 4), p += 4;
		continue;
	} FORMAT_END;
	pthread_rwlock_unlock(&buf_lock);

out:
	close(state->fd);
	BLOCK_TEARDOWN;

	return (void *)EXIT_SUCCESS;
}

DEFINE_BLOCK(fs)
{
	pthread_t thread;

	struct pollfd *pfd = BLOCK_POLLFD;
	if (pfd->fd < 0) {
		DEFINE_BLOCK_STATE(struct block_fs_state);
		int pair[2];

		pipe(pair);

		pfd->fd = pair[0];
		pfd->events = POLLIN;

		state->fd = pair[1];

		pthread_create(&thread, NULL, &block_fs_worker, b);
	} else {
		close(pfd->fd);
		pfd->fd = -1;
		b->timeout = 45;
	}
}
