static char const BLOCK_SEP[] = " ";
static char const GROUP_SEP[] = " | ";

static Block BLOCKS[] =
{
	{ 1, block_alsa, "Master", "VOL %d" },
	{ 2, block_cpu, "", "CPU %p" },
	{ 2, block_memory, NULL, "MEM %u/%t (%p)" },
	{ 3, block_fs, "~", "%n: %a (%F)" },
	{ 4, block_net, "lo", "%n: RX %R @ %r TX %T @ %t" },
	{ 5, block_datetime, NULL, "%c" }
	{ 6, block_text, "$(echo world)", "hello %s" },
	{ 7, block_spawn,
		"while true; do for c in a b c; do echo -n $c; sleep 1; done; echo; echo; sleep 1; done\0",
		">%s<" "\t" ":)"
	},
	{ 8, block_battery, "BAT0", "%F%n %s, %p[%P] (%l)" },
};

static Block *blocks = BLOCKS;
static size_t num_blocks = ARRAY_SIZE(BLOCKS);

static void
init(void)
{ }
