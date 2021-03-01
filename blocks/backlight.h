#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "utils.h"

DEFINE_BLOCK(backlight)
{
	enum { O_FLAGS = O_RDONLY | O_NONBLOCK | O_CLOEXEC };

	struct {
		int enabled_fd;
		int brightness_fd;
		int max_brightness_fd;
	} *state;

	BLOCK_SETUP {
		int const class_fd = open("/sys/class", O_FLAGS | O_DIRECTORY);
		int const dir_fd = openat(class_fd, b->arg, O_FLAGS | O_DIRECTORY);

		state->enabled_fd = openat(dir_fd, "device/enabled", O_FLAGS);
		if ((state->brightness_fd = openat(dir_fd, "actual_brightness", O_FLAGS)) < 0)
			state->brightness_fd = openat(dir_fd, "brightness", O_FLAGS);
		state->max_brightness_fd = openat(dir_fd, "max_brightness", O_FLAGS);

		close(dir_fd);
		close(class_fd);

		struct pollfd *const pfd = BLOCK_POLLFD;
		pfd->fd = acpi_connect();
		pfd->events = POLLIN;

		goto skip_filter;
	}

	if (!acpi_filter(BLOCK_POLLFD->fd,
	                 "video/brightness\0"
	                 /* Unknown/vendor (probably leds/...). */
	                 " \0"))
		return;
skip_filter:;

	/* usleep(150 #<{(| ms |)}># * 1000); */

	char buf[8];
	bool const enabled =
		state->enabled_fd < 0 || (
			8 == pread(state->enabled_fd, buf, 8, 0) &&
			!memcmp(buf, "enabled\n", 8)
		);

	unsigned long brightness;
	unsigned long max_brightness;
	unsigned percent;

	if (enabled) {
		brightness = readul(state->brightness_fd);
		max_brightness = readul(state->max_brightness_fd);
		percent = (unsigned)100UL * brightness / max_brightness;
	}

	FORMAT_BEGIN {
	case 'l': /* Any light? */
		if (enabled && 0 < brightness)
			continue;
		break;

	case 'i': /* Icon. */
	{
		if (!enabled)
			break;

		unsigned step = 7 * brightness / max_brightness;
		if (step == 7)
			step = 6;

		p[0] = 0xef;
		p[1] = 0x97;
		p[2] = 0x99 + step;
		p += 3;
	}
		continue;

	case 'o': /* on/off. */
	case 'O': /* ON/OFF */
	{
		static char LABELS[2][2][4] = {
			{ "off\0", "on\0" },
			{ "OFF\0", "ON\0" },
		};

		bool const on = enabled && 0 < brightness;
		memcpy(p, LABELS['O' == *format][on], 4);
		p += on ? 2 : 3;
	}
		continue;

	case 'b': /* Brightness. */
	case 'B': /* Maximum brightness. */
		if (!enabled)
			break;

		p += sprintf(p, "%lu", 'b' == *format ? brightness : max_brightness);
		continue;

	case 'p': /* Percent. */
		if (!enabled)
			break;

		p += sprintf(p, "%2u%%", percent);
		continue;
	} FORMAT_END
}
