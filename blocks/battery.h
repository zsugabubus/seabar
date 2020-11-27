#include <unistd.h>
#include <string.h>

#include "utils.h"

DEFINE_BLOCK(battery)
{
	enum { O_FLAGS = O_RDONLY | O_NONBLOCK | O_CLOEXEC };

	struct {
		int online_fd;
		int status_fd;
		int charge_now_fd;
		int charge_full_fd;
		int charge_full_design_fd;
		int capacity_level_fd;
	} *state;

	BLOCK_SETUP {
		int const power_supply_fd = open("/sys/class/power_supply", O_FLAGS | O_DIRECTORY);
		int dir_fd = openat(power_supply_fd, b->arg, O_FLAGS | O_DIRECTORY);

		state->online_fd = openat(dir_fd, "online", O_FLAGS);
		state->status_fd = openat(dir_fd, "status", O_FLAGS);
		state->charge_now_fd = openat(dir_fd, "charge_now", O_FLAGS);
		state->charge_full_fd = openat(dir_fd, "charge_full", O_FLAGS);
		state->charge_full_design_fd = openat(dir_fd, "charge_full_design", O_FLAGS);
		state->capacity_level_fd = openat(dir_fd, "capacity_level", O_FLAGS);

		close(dir_fd);
		close(power_supply_fd);

		struct pollfd *const pfd = BLOCK_POLLFD;
		pfd->fd = acpi_connect();
		pfd->events = POLLIN;

		goto skip_filter;
	}

	if (!acpi_filter(BLOCK_POLLFD->fd, "ac_adapter\0" "battery\0"))
		return;
skip_filter:;

	char status[50];
	size_t status_size = 0;
	char capacity_level[50];
	size_t capacity_level_size = 0;
	unsigned long charge_now = 0;
	unsigned long charge_full = 0;
	unsigned long charge_full_design = 0;

	bool poll = false;

	FORMAT_BEGIN {
	case 'n': /* name */
		size = strlen(b->arg);
		if (!size)
			break;

		memcpy(p, b->arg, size), p += size;
		continue;

	case 'l': /* capacity level */
		if (!capacity_level_size) {
			ssize_t len;
			if ((len = pread(state->capacity_level_fd, capacity_level, sizeof capacity_level - 1, 0)) < 0)
				break;

			capacity_level_size = len - (0 < len && '\n' == capacity_level[len - 1]);
		}
		memcpy(p, capacity_level, capacity_level_size), p += capacity_level_size;
		continue;

	case 's': /* status */
	case 'F': /* not full? */
		if (!status_size) {
			ssize_t len;
			if ((len = pread(state->status_fd, status, sizeof status - 1, 0)) < 0)
				break;

			status_size = len - (0 < len && '\n' == status[len - 1]);
		}

		if ('s' == *format) {
			memcpy(p, status, status_size), p += status_size;
		} else if ('F' == *format) {
			if (4 == status_size && !memcmp(status, "Full", 4))
				break;
		}
		continue;

	case 'p': /* charge percent */
		if (!charge_now)
			charge_now = readul(state->charge_now_fd);

		if (!charge_full)
			charge_full = readul(state->charge_full_fd);

		if (!charge_full)
			break;

		p += sprintf(p, "%2u%%", (unsigned)(100UL * charge_now / charge_full));
		poll = true;
		continue;

	case 'P': /* maximum percent (health) */
		if (!charge_full)
			charge_full = readul(state->charge_full_fd);

		if (!charge_full_design)
			charge_full_design = readul(state->charge_full_design_fd);

		if (!charge_full_design)
			break;

		p += sprintf(p, "%2u%%", (unsigned)(100UL * charge_full / charge_full_design));
		continue;
	} FORMAT_END

	if (poll)
		b->timeout = 120;
}
