static char BLOCK_SEP[] = " ";
static char GROUP_SEP[] = " | ";

static Block BAR[] = {
	{ 1, NULL, block_alsa },
	{ 2, NULL, block_cpu },
	{ 2, NULL, block_memory },
	{ 3, NULL, block_fsstat, { .str = "/" } },
	{ 4, NULL, block_datetime }
};
