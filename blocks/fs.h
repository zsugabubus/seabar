/* CFLAGS+=-pthread */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "fourmat/fourmat.h"
#include "blocks/seabar.h"

typedef struct {
	int fd;
} BlockFSContext;

static void *
block_fs_worker(void *arg)
{
	Block *const b = arg;
	DEFINE_BLOCK_STATE(BlockFSContext);

	struct statvfs st;
	int const res = statvfs(b->arg, &st);

	uint64_t const total_size = (uint64_t)st.f_blocks * st.f_frsize;
	uint64_t const avail_size = (uint64_t)st.f_bavail * st.f_bsize;
	uint64_t const free_size  = (uint64_t)st.f_bfree * st.f_bsize;

	pthread_rwlock_rdlock(&buf_lock);
	FORMAT_BEGIN {
	case 'i': /* Basic fs icon. */
	{
		if (res < 0)
			break;

		char const *icon;
		switch (st.f_fsid) {
		case 0 /* tmpfs */: icon = "\xef\x82\xae \0" "MEM "; break;
		default:            icon = "\xef\x9f\x89 \0" "DISK "; break;
		}

		if (use_text_icon)
			icon += strlen(icon) + 1 /* NUL */;
		sprint(&p, icon);
	}
		continue;

	case 'n': /* Name. */
	{
		char const *name = strrchr(b->arg, '/');
		name = name ? name + 1 : b->arg;
		sprint(&p, name);
	}
		continue;

	case 'a': /* Available (free space for unprivileged users). */
		if (res < 0)
			break;

		p += fmt_space(p, avail_size);
		continue;

	case 'P': /* Available percent. */
		if (res < 0)
			break;

		p += fmt_percent(p, avail_size, total_size);
		continue;

	case 'f': /* Free. */
	{
		if (res < 0)
			break;

		p += fmt_space(p, free_size);
	}
		continue;

	case 'u': /* Used. */
		if (res < 0)
			break;

		p += fmt_space(p, total_size - free_size);
		continue;

	case 'p': /* Used percent. */
		if (res < 0)
			break;

		p += fmt_percent(p, total_size - avail_size, total_size);
		continue;

	case 't': /* Total. */
		if (res < 0)
			break;

		p += fmt_space(p, total_size);
		continue;

	case 'F': /* Flags. */
		if (res < 0)
			break;

		*p++ = st.f_flag & ST_RDONLY ? '-' : 'w';
		*p++ = st.f_flag & ST_NOEXEC ? '-' : 'x';
		*p++ = st.f_flag & ST_NOSUID ? '-' : 's';
		continue;

	case 'c': /* File count. */
		if (res < 0)
			break;

		p += sprintf(p, "%ld", st.f_files);
		continue;

	case 'r': /* '*' if read-only. */
		if (res < 0)
			break;

		if (st.f_flag & ST_RDONLY)
			*p++ = '*';
		continue;

	case 'R': /* '[RO]' if read-only. */
		if (res < 0)
			break;

		if (st.f_flag & ST_RDONLY)
			sprint(&p, "[RO]");
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
		DEFINE_BLOCK_STATE(BlockFSContext);
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
