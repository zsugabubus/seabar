#ifndef SEABAR_BLOCK_SEABAR_H
#define SEABAR_BLOCK_SEABAR_H

int use_text_icon = 0;

DEFINE_BLOCK(seabar)
{
	FORMAT_BEGIN {
	case 'i':
	case 't':
		use_text_icon = *format == 't';
		continue;
	} FORMAT_END;
}

#endif /* SEABAR_BLOCK_SEABAR_H */
