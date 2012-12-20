/* Copyright 2012 Dan Smith <dsmith@danplanet.com> */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "aprs-is.h"

static int aprsis_login(int fd, const char *call,
			double lat, double lon, double range)
{
	char *buf;
	int ret, len;

	len = asprintf(&buf, "user %s pass -1 vers Unknown 0.00 filter r/%.0f/%.0f/%.0f\r\n",
		       call, lat, lon, range);
	if (len < 0)
		return -ENOMEM;

	ret = write(fd, buf, len);
	free(buf);

	if (ret != len)
		return -EIO;
	else
		return 0;
}

int aprsis_connect(const char *hostname, int port, const char *mycall,
		   double lat, double lon, double range)
{
	int sock;
	struct sockaddr_in sa;
	struct hostent *he;
	int ret;

	he = gethostbyname(hostname);
	if (!he || (he->h_length < 1))
		return -ENETUNREACH;

	sa.sin_family = AF_INET;
	memcpy(&sa.sin_addr, he->h_addr_list[0], sizeof(sa.sin_addr));
	sa.sin_port = htons(port);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		return sock;

	ret = connect(sock, (struct sockaddr *)&sa, sizeof(sa));
	if (ret < 0)
		goto out;

	ret = aprsis_login(sock, mycall, lat, lon, range);
	if (ret < 0)
		goto out;

	printf("Connected\n");

 out:
	if (ret) {
		close(sock);
		return ret;
	}

	return sock;
}

int get_packet_text(int fd, char *buffer, unsigned int *len)
{
	int i = 0;
	char last = '\0';

	buffer[i] = '\0';

	while ((i < *len) && (last != '\n')) {
		int ret;
		ret = read(fd, &buffer[i], 1);
		last = buffer[i];
		if (ret == 1)
			i++;
		else if (ret == 0)
			break; /* Socket disconnected */
	}

	*len = i;
	return (i > 0) && (last == '\n');
}

#ifdef MAIN

#include "util.h"
#include "aprs.h"

int main(int argc, char **argv)
{
	int sock;
	int ret;
	char buf[256];
	struct state state;

	memset(&state, 0, sizeof(state));

	if (parse_ini(state.conf.config ? state.conf.config : "aprs.ini", &state)) {
		printf("Invalid config\n");
		exit(1);
	}

	sock = aprsis_connect(state.conf.net_server_host_addr,
			      APRS_PORT_FILTERED_FEED,
			      state.mycall,
			      MYPOS(&state)->lat,
			      MYPOS(&state)->lon,
			      state.conf.aprsis_range);

	if (sock < 0) {
		printf("Sock %i: %m\n", sock);
		return 1;
	}

	while ((ret = read(sock, buf, sizeof(buf)))) {
		int i;
		for (i = 0; i < ret; i++) {
			if (buf[i] != '*')
				write(1, &buf[i], 1);
		}
		write(1, "\r", 1);

		//buf[ret] = 0;
		//printf("Got: %s\n", buf);
	}
	return 0;
}

#endif
