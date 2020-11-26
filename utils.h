#ifndef SEABAR_UTILS_H
#define SEABAR_UTILS_H

#include <errno.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>

unsigned long
readul(int const fd)
{
	char buf[22];
	ssize_t len;

	if ((len = pread(fd, buf, sizeof buf - 1, 0)) < 0)
		len = 0;
	buf[len] = '\0';

	return strtoul(buf, NULL, 10);
}

static int
acpi_connect(void)
{
	int fd;

	if (0 <= (fd = socket(AF_UNIX, SOCK_NONBLOCK | SOCK_STREAM, 0))) {
		struct sockaddr_un sa;

		sa.sun_family = AF_UNIX;
		strncpy(sa.sun_path, "/var/run/acpid.socket", sizeof sa.sun_path - 1);

		if (connect(fd, (struct sockaddr *)&sa, sizeof sa) < 0)
			close(fd), fd = -1;
	}

	return fd;
}

static bool
acpi_filter(int const fd, char const *const events)
{
	char buf[BUFSIZ];
	ssize_t len;

	if ((len = read(fd, buf, sizeof buf - 1)) < 0)
		return true;
	buf[len] = '\0';

	for (char const *p = buf;;) {
		char const *pattern = events;
		for (size_t len; (len = strlen(pattern)); pattern += len)
			if (strncmp(buf, pattern, len))
				return true;

		if (!(p = strchr(p, '\n')))
			break;
		++p;
	}

	return false;
}

#endif
