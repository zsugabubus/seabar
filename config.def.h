static char const BLOCK_SEP[] = " ";
static char const GROUP_SEP[] = " | ";

static Block blocks[] = {
	{ 8, block_read, "/proc/sys/kernel/random/entropy_avail", "E=%d" },
	{ 1, block_alsa, "Master", "VOL %d" },
	{ 2, block_cpu, "", "CPU %p" },
	{ 2, block_memory, NULL, "MEM %u/%t (%p)" },
	{ 3, block_fsstat, "/home", "%n: %a (%F)" },
	{ 4, block_net, "lo", "%n: DN %R @ %r UP %T @ %t" },
	{ 5, block_datetime, NULL, "%c" }
};
