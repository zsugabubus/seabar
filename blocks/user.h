#include <inttypes.h>
#include <pwd.h>
#include <string.h>
#include <sys/types.h>

DEFINE_BLOCK(user)
{
	FORMAT_BEGIN {
	case 'u': /* uid */
		p += sprintf(p, "%u", (unsigned)geteuid());
		continue;

	case 'g': /* gid */
		p += sprintf(p, "%u", (unsigned)geteuid());
		continue;

	case 'n': /* username */
	{
		struct passwd *pw;

		if (!(pw = getpwuid(geteuid())))
			break;

		size = strlen(pw->pw_name);
		memcpy(p, pw->pw_name, size), p += size;
	}
		continue;
	} FORMAT_END;
}
