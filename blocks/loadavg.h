DEFINE_BLOCK(loadavg)
{
	FORMAT_BEGIN {
	case 's':
	{
		double loadavg[3];
		if (-1 == getloadavg(loadavg, ARRAY_SIZE(loadavg)))
			break;

		p += sprintf(p, "%.2f %.2f %.2f", loadavg[0], loadavg[1], loadavg[2]);
	}
		continue;
	} FORMAT_END;

	b->timeout = 5;
}
