DEFINE_BLOCK(loadavg)
{
	FORMAT_BEGIN {
	case 's':
	{
		double loadavg[3];
		if (getloadavg(loadavg, ARRAY_SIZE(loadavg)) < 0)
			break;

		p += sprintf(p, "%.2f %.2f %.2f", loadavg[0], loadavg[1], loadavg[2]);
	}
		continue;
	} FORMAT_END;

	b->timeout = 5;
}
