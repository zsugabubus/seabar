static char const BLOCK_SEP[] = " ";
static char const GROUP_SEP[] = " | ";

static Block MY_BAR[] = {
	{ 1, NULL, block_alsa },
	{ 2, NULL, block_cpu },
	{ 2, NULL, block_memory },
	{ 3, NULL, block_fsstat, { .str = "/" } },
	{ 4, NULL, block_net, { .str = "lo" } },
	{ 5, NULL, block_datetime }
};
static Block *blocks = MY_BAR;
static size_t num_blocks = ARRAY_SIZE(MY_BAR);

static void
init(void)
{ }
