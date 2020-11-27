#include <string.h>

DEFINE_BLOCK(text)
{
	FORMAT_BEGIN {
	case 's':
		if (!b->arg)
			break;

		size = strlen(b->arg);
		memcpy(p, b->arg, size), p += size;
		continue;
	} FORMAT_END;
}
