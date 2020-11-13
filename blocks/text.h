#include <string.h>

DEFINE_BLOCK(text)
{
	strcpy(b->buf, b->format);
}
