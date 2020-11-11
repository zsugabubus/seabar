#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <net/if.h>
#include <linux/ethtool.h>
#include <linux/limits.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/sockios.h>
#include <linux/wireless.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include "fourmat/fourmat.h"

BLOCK(net)
{
	static char const DEFAULT_FORMAT[] = "%ls%s%s%s  %s @ %s  %s @ %s";

	struct {
		unsigned long long last_rx_bytes, last_tx_bytes;
		unsigned if_index;
		int if_state;

		char essid[IW_ESSID_MAX_SIZE + 1];
		char szip_addr[INET_ADDRSTRLEN + sizeof("/32")];
		char szip6_addr[INET6_ADDRSTRLEN + sizeof("/128")];
		char szmac_addr[sizeof("00:00:00:00:00:00")];
		char szlink_speed[4];
		wchar_t const *symbol;
	} *state;

	char *if_name = b->arg.str;

	static struct iovec iov;
	static struct sockaddr_nl nlsa = {
		.nl_family = AF_NETLINK,
		.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR,
		.nl_pad = 0,
	};
	static struct msghdr msg = {
		&nlsa, sizeof nlsa,
		&iov, 1, NULL, 0, 0
	};
	static int msg_seq = 0;

	static int sfd;

	if (!iov.iov_base) {
		iov.iov_len = sysconf(_SC_PAGESIZE);
		iov.iov_base = malloc(iov.iov_len);

		nlsa.nl_pid = getpid();

		if (-1 == sfd)
			sfd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_IP);
	}

	struct nlmsghdr *nlh = (struct nlmsghdr *)iov.iov_base;
	struct ifaddrmsg *ifa;
	struct ifinfomsg *ifi;
	struct rtattr *rta;

	int nlfd;

	if (!(state = b->state.ptr)) {
		if (!(state = b->state.ptr = malloc(sizeof *state)))
			abort();

		state->last_rx_bytes = 0;
		state->last_tx_bytes = 0;
		state->if_state = IF_OPER_UNKNOWN;
		*state->essid = '\0';
		*state->szip_addr = '\0';
		*state->szip6_addr = '\0';
		*state->szmac_addr = '\0';
		*state->szlink_speed = '\0';
		state->symbol = L"\0";

		if (!(state->if_index = if_nametoindex(if_name))) {
			block_str_strerror("failed to find interface");
			return;
		}

		struct pollfd *pfd = BLOCK_POLLFD;
		if (-1 == (nlfd = socket(AF_NETLINK, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, NETLINK_ROUTE)))
			return;

		setsockopt(nlfd, SOL_NETLINK, NETLINK_EXT_ACK, (int[]){ true }, sizeof(int));

		if (-1 == bind(nlfd, (struct sockaddr *)&nlsa, sizeof nlsa))
			return;

		pfd->fd = nlfd;
		pfd->events = POLLIN;

		nlh->nlmsg_type = RTM_GETADDR;
		nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
		nlh->nlmsg_pid = getpid();
		nlh->nlmsg_seq = ++msg_seq;
		nlh->nlmsg_len = NLMSG_LENGTH(sizeof *ifa);

		ifa = NLMSG_DATA(nlh);
		ifa->ifa_family = AF_UNSPEC;
		ifa->ifa_prefixlen = 0;
		ifa->ifa_flags = 0/* must be zero */;
		ifa->ifa_scope = RT_SCOPE_UNIVERSE;
		ifa->ifa_index = state->if_index;

		setsockopt(nlfd, SOL_NETLINK, NETLINK_GET_STRICT_CHK, (int[]){ true }, sizeof(int));
		if (-1 == sendto(nlfd, nlh, nlh->nlmsg_len, 0, NULL, 0))
			perror("sendto()");
		setsockopt(nlfd, SOL_NETLINK, NETLINK_GET_STRICT_CHK, (int[]){ false }, sizeof(int));
	} else {
		nlfd = BLOCK_POLLFD->fd;
	}

	nlh->nlmsg_type = RTM_GETLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST; /* Retrieve only the matching. */
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_seq = ++msg_seq;
	nlh->nlmsg_len = NLMSG_LENGTH(sizeof *ifi);

	ifi = NLMSG_DATA(nlh);
	ifi->ifi_family = AF_UNSPEC; /* AF_PACKET */
	ifi->ifi_type = ARPHRD_NETROM;
	ifi->ifi_index = state->if_index;
	ifi->ifi_flags = 0;
	ifi->ifi_change = 0xFFffFFff; /* 0 */

	if (-1 == sendto(nlfd, nlh, nlh->nlmsg_len, 0, NULL, 0))
		perror("sendto()");

	b->timeout = 4;

	uint64_t rx_bytes = state->last_rx_bytes, tx_bytes = state->last_tx_bytes;

	for (;;) {
		ssize_t len = recvmsg(nlfd, &msg, MSG_DONTWAIT);
		if (-1 == len)
			break;

		for (nlh = (struct nlmsghdr *)iov.iov_base;
		     NLMSG_OK(nlh, len);
		     nlh = NLMSG_NEXT(nlh, len))
		{
			size_t rtl;

			switch (nlh->nlmsg_type) {
			case NLMSG_DONE:
				continue;

			case RTM_NEWLINK:
			case RTM_GETLINK:
			{
				ifi = NLMSG_DATA(nlh);
				/* We receive events for other interfaces too. */
				if (state->if_index != (unsigned)ifi->ifi_index)
					continue;

				rta = IFLA_RTA(ifi);
				rtl = IFLA_PAYLOAD(nlh);
				ifa = NULL;

				switch (ifi->ifi_type) {
				case ARPHRD_ETHER: state->symbol = L" "; break;
				default:           state->symbol = L"ﯱ "; break;
				}

				*state->essid = '\0';
				*state->szlink_speed = '\0';

				struct iwreq iwr;
				strncpy(iwr.ifr_name, if_name, sizeof iwr.ifr_name);
				/* is wireless? */
				if (!ioctl(sfd, SIOCGIWNAME, &iwr)) {
					struct iw_statistics iwstat;

					state->symbol = L" ";

					iwr.u.data.pointer = &iwstat;
					iwr.u.data.length = sizeof(iwstat);

					/*
					if(0 == ioctl(sfd, SIOCGIWSTATS, &iwr)) {
						if (iwstat.qual.updated & IW_QUAL_DBM) {
							printf("SIGNAL: %ddBm\n", iwstat.qual.level - 256);
						}
					} */

					iwr.u.essid.pointer = state->essid;
					iwr.u.essid.length = sizeof(state->essid);
					iwr.u.essid.flags = 0;

					(void)ioctl(sfd, SIOCGIWESSID, &iwr);

					if(0 == ioctl(sfd, SIOCGIWRATE, &iwr))
						fmt_speed(state->szlink_speed, iwr.u.bitrate.value);

				}
			}
				break;

			case RTM_NEWADDR:
			case RTM_GETADDR:
				ifa = NLMSG_DATA(nlh);
				/* We receive events for other interfaces too. */
				if (state->if_index != ifa->ifa_index)
					continue;

				rta = IFA_RTA(ifa);
				rtl = IFA_PAYLOAD(nlh);
				ifi = NULL;
				break;

			default:
				continue;
			}

			for (; RTA_OK(rta, rtl); rta = RTA_NEXT(rta, rtl)) {
				switch (rta->rta_type) {
				case IFLA_ADDRESS:
					switch (nlh->nlmsg_type) {
					case RTM_GETADDR:
					case RTM_NEWADDR:
					{
						size_t str_size;
						char *str;

						if (AF_INET == ifa->ifa_family)
							str = state->szip_addr, str_size = sizeof(state->szip_addr);
						else
							str = state->szip6_addr, str_size = sizeof(state->szip6_addr);

						inet_ntop(ifa->ifa_family, RTA_DATA(rta), str, str_size);
						sprintf(str + strlen(str), "/%u", ifa->ifa_prefixlen);
					}
						break;

					case RTM_NEWLINK:
					case RTM_GETLINK:
						ether_ntoa_r(RTA_DATA(rta), state->szmac_addr);
						break;
					}
					break;

				case IFLA_OPERSTATE:
					state->if_state = *(int *)RTA_DATA(rta);
					break;

				case IFLA_STATS64: {
					struct rtnl_link_stats64 *ls64 = (struct rtnl_link_stats64 *)RTA_DATA(rta);
					rx_bytes = ls64->rx_bytes;
					tx_bytes = ls64->tx_bytes;
					break;
				}
				}

			}
		}
	}

	switch (state->if_state) {
	case IF_OPER_UNKNOWN: /* for loopback */
	case IF_OPER_UP:
	case IF_OPER_DORMANT:
	{
		uint64_t const elapsed_ns = elapsed.tv_sec * NSEC_PER_SEC + elapsed.tv_nsec;
		uint64_t rx_rate, tx_rate;
		if (0 < elapsed_ns) {
			rx_rate = (rx_bytes - state->last_rx_bytes) * NSEC_PER_SEC / elapsed_ns;
			tx_rate = (tx_bytes - state->last_tx_bytes) * NSEC_PER_SEC / elapsed_ns;
		} else {
			rx_rate = 0;
			tx_rate = 0;
		}

		char szrx_rate[5], sztx_rate[5];
		char szrx[5], sztx[5];

		szrx[fmt_speed(szrx, rx_bytes)] = '\0';
		sztx[fmt_speed(sztx, tx_bytes)] = '\0';
		szrx_rate[fmt_speed(szrx_rate, rx_rate)] = '\0';
		sztx_rate[fmt_speed(sztx_rate, tx_rate)] = '\0';

		sprintf(b->buf, b->format ? b->format : DEFAULT_FORMAT,
			state->symbol,
			state->essid,
			(*state->essid ? " " : ""),
			(*state->szip_addr ? state->szip_addr : *state->szip6_addr > 0 ? state->szip6_addr : state->szmac_addr),
			szrx, szrx_rate,
			sztx, sztx_rate);

		state->last_rx_bytes = rx_bytes;
		state->last_tx_bytes = tx_bytes;
	}
		break;
	}
}
