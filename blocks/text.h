#include <string.h>

BLOCK(text)
{
	strcpy(b->buf, b->format);
}
