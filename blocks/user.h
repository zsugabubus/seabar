#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

DEFINE_BLOCK(user)
{
	struct passwd const *pw = getpwuid(geteuid());

	FORMAT_BEGIN {
	case 'u': /* UID. */
		p += sprintf(p, "%u", (unsigned)geteuid());
		continue;

	case 'g': /* GID. */
		p += sprintf(p, "%u", (unsigned)geteuid());
		continue;

	case 'n': /* Username. */
	{
		if (!pw)
			break;

		sprint(&p, pw->pw_name);
	}
		continue;

	case 'h': /* Home directory. */
	{
		if (!pw)
			break;

		sprint(&p, pw->pw_dir);
	}
		continue;
	} FORMAT_END;
}
