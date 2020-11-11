#include <string.h>

BLOCK(text)
{
	strcpy(b->buf, b->arg.str);
}
