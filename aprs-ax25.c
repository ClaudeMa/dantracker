/* Copyright 2012 Dan Smith <dsmith@danplanet.com> */

#ifdef HAVE_AX25_TRUE

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>

#if HAVE_NETAX25_AX25_H
#include <netax25/ax25.h>
#else
#include <netax25/kernel_ax25.h>
#endif
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <stdbool.h>

#include "aprs.h"
#include "aprs-is.h"

#if 0
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
#endif

int get_ax25packet(struct state *state, char *buffer, unsigned int *len)
{
	int i = 0;
	int result = 1;
	struct ifreq ifr;
	int rx_socket = state->tncfd;
	int ret = 0;

	buffer[i] = '\0';

	{
		struct sockaddr sa;
		int asize = sizeof(sa);
		int size = 0;
		char rxbuf[1500];


		if ((size = recvfrom(rx_socket, rxbuf, sizeof(rxbuf), 0, &sa, (socklen_t*)&asize)) == -1) {
			perror("AX25 Socket recvfrom error");
			printf("%s:%s(): socket %d\n", __FILE__, __FUNCTION__, rx_socket);

			result = -1;

		} else {

			if (state->ax25_dev != NULL && strcmp(state->ax25_dev, sa.sa_data) != 0) {
				result = 0;
			}


			if (state->ax25_recvproto == ETH_P_ALL) {       /* promiscuous mode? */
				strcpy(ifr.ifr_name, sa.sa_data);
				if (ioctl(rx_socket, SIOCGIFHWADDR, &ifr) < 0
				    || ifr.ifr_hwaddr.sa_family != AF_AX25)
					result = 0;             /* not AX25 so ignore this packet */
#if 0
				printf("IFHWADDR 0x%02x look for 0x%02x\n",
				       ifr.ifr_hwaddr.sa_family, AF_AX25);
#endif

			}
			if( (size > 512) && result != 0) {
				printf("%s:%s() Received oversize AX.25 frame %d bytes, sa_data: %s \n",
				       __FILE__, __FUNCTION__, size, sa.sa_data);
				result = 0;
			}


			if (result == 1) {

				ret = fap_ax25_to_tnc2(rxbuf+1, size-1, buffer, len);

				if (!ret) {
					unsigned char *pBuf = (unsigned char *)rxbuf;
					unsigned char *qBuf, *sBuf;
					int i,j;

					if(len) {
						printf("fap msg: %s\n", buffer);
					}

					printf("Failed to convert packet: %s, len=%d, sockaddr len = %d\n",
					       rxbuf, *len, sizeof(sa) );

					qBuf = pBuf;
					for(j=0; j < 4; j++) {
						for(i=0; i < 32; i++) {
							printf("%02x ", *pBuf++);
						}
						printf("\n");

						sBuf=qBuf;
						for(i=0; i < 32; i++) {
							printf("%2c ", *qBuf++);
						}
						printf("\n");
						while((*sBuf & 0x01) == 0) {
							printf("%2c ", *sBuf >> 1);
							sBuf++;
						}
						printf("\n");
					}
				} else {
					pr_debug("Good convert: recvfrom size: %d, fap size: %u -", size, *len);
				}
			}
		}
	}

	return ret;
}


int aprsax25_connect(struct state *state)
{
	int rx_sock, tx_sock;
	unsigned int tx_src_addrlen, tx_dest_addrlen;	/* Length of addresses */
	char *ax25dev;
	struct full_sockaddr_ax25 sockaddr_ax25_dest;
	struct full_sockaddr_ax25 sockaddr_ax25_src;
	char *portcall;

	char *ax25port = state->conf.ax25_port;
	char *aprspath = state->conf.aprs_path;

	/* Protocol to use for receive, either ETH_P_ALL or ETH_P_AX25 */
	/* Choosing ETH_P_ALL would not only pick outbound packets, but all of
	 *  the ethernet traffic..
	 * ETH_P_AX25 picks only inbound-at-ax25-devices packets. */
	state->ax25_recvproto = ETH_P_ALL;

	if(strlen(aprspath) == 0 ) {
		fprintf(stderr, "%s:%s() no APRS path configured\n", __FILE__, __FUNCTION__);
		/* Fix this */
		return(-1);
	}

	if (ax25_config_load_ports() == 0) {
		fprintf(stderr, "%s:%s() no AX.25 port data configured\n", __FILE__, __FUNCTION__);
		/* Fix this */
		return(-1);
	}

	/* This function maps the port name onto the device
	 * name of the port. See /etc/ax25/ax25ports */
	if ((ax25dev = ax25_config_get_dev(ax25port)) == NULL) {
		fprintf(stderr, "%s:%s(): invalid port name - %s\n",
			  __FILE__, __FUNCTION__, ax25port);
		/* Fix this */
		return(-1);
	}

	/* Save the ax25 device name to be used by get_ax25packet */
	state->ax25_dev = ax25dev;

	printf("%s %s(): Port %s is using device %s\n",
	       __FILE__, __FUNCTION__, ax25port, ax25dev);

	/*
	 * Open up a receive & transmit socket
	 */
#if 0 /* why doesn' this work */
	if ( (rx_sock = socket(AF_AX25, SOCK_RAW, htons(state->ax25_recvproto))) == -1) {
		perror("receive AX.25 socket");
		return(-1);
	}
#endif
#if 1
	/* From monixd */
	if ( (rx_sock = socket(AF_INET, SOCK_PACKET, htons(state->ax25_recvproto))) == -1) {
		perror("receive AX.25 socket");
		return(-1);
	}

#else
	if ( (rx_sock = socket(PF_PACKET, SOCK_PACKET, htons(ETH_P_ALL))) == -1) {
		perror("receive AX.25 socket");
		return(-1);
	}
#endif
	/*
	 * Could use socket type SOCK_RAW or SOCK_DGRAM?
	 * htons(ETH_P_AX25)
	 */
	if ( (tx_sock = socket(AF_AX25, SOCK_DGRAM, 0)) == -1) {
		perror("transmit AX.25 socket");
		return(-1);
	}

#if 1
	/* Get the ax25 port callsign */
	if ((portcall = ax25_config_get_addr(ax25port)) == NULL) {
		perror("ax25_config_get_addr");
		return -1;
	}

	/* Convert to AX.25 addresses */
	if ((tx_src_addrlen = ax25_aton(portcall, &sockaddr_ax25_src)) == -1) {
		perror("ax25_config_get_addr src");
		return -1;
	}

	if ((tx_dest_addrlen = ax25_aton(aprspath, &sockaddr_ax25_dest)) == -1) {
		perror("ax25_config_get_addr dest");
		return -1;
	}

	{
		int i;
		unsigned char *pBuf =  (unsigned char *)&sockaddr_ax25_dest;

		printf("aprsax25_connect: tx addr len %d\n", tx_dest_addrlen);
		for (i = 0; i < sizeof(sockaddr_ax25_dest); i++) {
			printf("0x%02x ", *(pBuf+i));
		}
		printf("\n");
	}

	/* Bind the tx socket to the source address */
	if (bind(tx_sock, (struct sockaddr *)&sockaddr_ax25_src, tx_src_addrlen) == -1) {
		perror("bind");
		return 1;
	}

#else
	{
		char *mycall = state->mycall;

		/*  takes the ASCII string <mycall> that is in the format callsign [[V|VIA]
		 *  callsign ...] and stores it in <*sockaddr_ax25> in network format.
		 */
		ax25_aton(mycall, &sockaddr_ax25);
		if (sockaddr_ax25.fsa_ax25.sax25_ndigis == 0) {
			ax25_aton_entry(ax25_config_get_addr(ax25port),
					sockaddr_ax25.fsa_digipeater[0].
					ax25_call);
			sockaddr_ax25.fsa_ax25.sax25_ndigis = 1;
		}
		sockaddr_ax25.fsa_ax25.sax25_family = AF_AX25;
		tx_dst_addrlen = sizeof(struct full_sockaddr_ax25);

		/*
		 * Need to bind the TX socket to an end point
		 */
		if (bind(sock, (struct sockaddr *) &sockaddr_ax25, tx_dst_addrlen) == -1) {
			perror("bind");
			close(sock);
			/* Fix this */
			return(-1);
		}
		/*
		 * The APRS bind to AX.25 protocol is not connection oreinted
		 *  Need to figure this out
		 */

		if (ax25_aton(cfg.targetcall, &sockaddr_ax25) < 0) {
			close(s);
			perror("ax25_aton()");
			/* Fix this */
			return(-1);
		}

		settimeout(cfg.timeoutsecs);
		if (connect(sock, (struct sockaddr *) &sockaddr_ax25, addrlen) != 0) {
			close(sock);
			perror("ax25 connect()");
			/* Fix this */
			return(-1);
		}
	}
#endif
	state->ax25_tx_sock = tx_sock;

	printf("Connected to AX.25 stack on rx socket %d, tx socket %d\n", rx_sock, tx_sock);
	fflush(stdout);

	return(rx_sock);
}

int send_ax25_beacon(struct state *state, char *packet)
{
	char buf[512];
	int ret;
	unsigned int len = sizeof(buf);
	unsigned int tx_dest_addrlen;	/* Length of APRS path */
	char *aprspath = state->conf.aprs_path;
	struct full_sockaddr_ax25 sockaddr_ax25_dest;
	int sock = state->ax25_tx_sock;

#if 0
	ret = fap_tnc2_to_ax25(packet, strlen(packet), buf, &len);
	if (!ret) {
		printf("Failed to make beacon AX.25 packet\n");
		return false;
	}
#else
	buf[0]='\0';
	strcpy(buf, packet);
	len=strlen(buf);
#endif
	printf("Sending AX.25 beacon of %d bytes, with socket %d: %s\n",
	       len, sock, packet);

#if 0

	{
		int i;
		char buffer[512];
		unsigned int faplen = sizeof(buffer);
		char *bufx;
		unsigned int lenx;

		i=0;
		while((unsigned char)buf[i] != 0xf0 && i < 512) {
			i++;
		}
		printf("Found 0xf0 at byte %d, 0x%02x\n", i, (unsigned char)buf[i] );
		bufx = &buf[i];
		lenx = len - i;

		buffer[0] = '\0';

		printf("raw dump after fap conversion: size: %d\n", len);
		for(i=0; i < len; i++) {
			printf("%02x ", (unsigned char) buf[i]);
		}
		printf("\n");
		for(i=0; i < len; i++) {
			printf("%2c ", (unsigned char) buf[i]);
		}
		printf("\n");

		printf("Sending AX.25 beacon fap test\n");
		ret = fap_ax25_to_tnc2(buf, len, buffer, &faplen);
		printf("fap ret: 0x%x, size: %d\n", ret, faplen);
		for(i=0; i < faplen; i++) {
			printf("%02x ", buffer[i]);
		}
		printf("\n");
		for(i=0; i < faplen; i++) {
			printf("%2c ", buffer[i]);
		}
		printf("\n");
	}
#endif

	if(strlen(aprspath) == 0 ) {
		fprintf(stderr, "%s:%s() no APRS path configured\n", __FILE__, __FUNCTION__);
		/* Fix this */
		return false;
	}

	if ((tx_dest_addrlen = ax25_aton(aprspath, &sockaddr_ax25_dest)) == -1) {
		perror("ax25_config_get_addr dest");
		return false;
	}
	{
		int i;
		unsigned char *pBuf =(unsigned char *) &sockaddr_ax25_dest;

		printf("send_ax25_beacon: tx addr len %d\n", tx_dest_addrlen);
		for (i = 0; i < sizeof(sockaddr_ax25_dest); i++) {
			printf("0x%02x ", *(pBuf+i));
		}
		printf("\n");
	}


	printf("Sock: family 0x%02x, buf1 0x%02x, protocol 0x%02x, dest addr len=%d\n",
	       sockaddr_ax25_dest.fsa_ax25.sax25_family,
	       (unsigned char)buf[0],
	       htons(ETH_P_AX25),
	       tx_dest_addrlen

	      );
#if 1

	if ((ret=sendto(sock, buf, len, 0, (struct sockaddr *)&sockaddr_ax25_dest, tx_dest_addrlen)) == -1) {
		pr_debug("%s:%s(): after sendto sock msg len= %d, ret= 0x%02x\n",  __FILE__, __FUNCTION__, len, ret);
		perror("sendto");
		return false;
	}
#else
	if (connect(sock, (struct sockaddr *) &sockaddr_ax25_dest, tx_dest_addrlen) != 0) {
		close(sock);
		perror("ax25 connect()");
			/* Fix this */
		return(-1);
	}

	if((ret=write(sock, buf, len)) == -1) {
		pr_debug("%s:%s(): after writing to sock ret= 0x%02x\n",  __FILE__, __FUNCTION__, ret);
		perror("write");
	}


#endif

	return (ret == len);
}

#ifdef MAIN

#include "util.h"
#include "aprs.h"

int main()
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

#endif /* #ifdef MAIN */
#endif /* #ifdef HAVE_AX25_TRUE */