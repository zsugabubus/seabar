#include <string.h>

DEFINE_BLOCK(text)
{
	FORMAT_BEGIN {
	case 's':
		if (!sprint(&p, b->arg))
			break;
		continue;
	} FORMAT_END;
}
