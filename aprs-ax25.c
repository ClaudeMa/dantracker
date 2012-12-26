/* -*- Mode: C; tab-width: 8;  indent-tabs-mode: nil; c-basic-offset: 8; c-brace-offset: -8; c-argdecl-indent: 8 -*- */

/* dantracker aprs tracker
   *
   * Copyright 2012 Dan Smith <dsmith@danplanet.com>
   */

#ifdef HAVE_AX25_TRUE

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netax25/kernel_ax25.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <stdbool.h>

#include "aprs.h"
#include "aprs-is.h"
#include "aprs-ax25.h"


bool packet_qualify(struct state *state, struct sockaddr *sa, unsigned char *buf, int size)
{
        struct ifreq ifr;
        bool packet_status = true;

        if (state->ax25_recvproto == ETH_P_ALL) {       /* promiscuous mode? */
                strcpy(ifr.ifr_name, sa->sa_data);
                if (ioctl(state->tncfd, SIOCGIFHWADDR, &ifr) < 0
                    || ifr.ifr_hwaddr.sa_family != AF_AX25) {
                        pr_debug("%s:%s(): qualify fail due to protocol family: 0x%02x expected 0x%02x\n",
                                 __FILE__, __FUNCTION__, ifr.ifr_hwaddr.sa_family, AF_AX25);
                        return false;          /* not AX25 so ignore this packet */
                }
        }

        if ( (state->conf.ax25_pkt_device_filter == AX25_PKT_DEVICE_FILTER_ON) &&
             (state->ax25_dev != NULL) &&
             (strcmp(state->ax25_dev, sa->sa_data) != 0) ) {
                pr_debug("%s:%s(): qualify fail due to device %s\n", __FILE__, __FUNCTION__, sa->sa_data);
                packet_status = false;
        }

        if( (size > 512) && packet_status) {
                printf("%s:%s() Received oversize AX.25 frame %d bytes, sa_data: %s \n",
                       __FILE__, __FUNCTION__, size, sa->sa_data);
                packet_status = false;
        }
        if(!ISAX25ADDR(buf[0] >> 1)) {
                pr_debug("%s:%s(): qualify fail due first char not a valid AX.25 address char 0x%02x\n",
                         __FILE__, __FUNCTION__, buf[0]);
                packet_status = false;
        }

        return(packet_status);
}

void fap_conversion_debug(char *rxbuf, char *buffer, int len)
{
        unsigned char *pBuf = (unsigned char *)rxbuf;
        unsigned char *qBuf, *sBuf;
        int i,j;

        if(len) {
                printf("fap msg: %s\n", buffer);
        }

        printf("Failed to convert packet, len=%d : %s\n",
               len, rxbuf);

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
}

int get_ax25packet(struct state *state, char *buffer, unsigned int *len)
{
        int rx_socket = state->tncfd;
        int ret = 0;
        struct sockaddr sa;
        int asize = sizeof(sa);
        int size = 0;
        char rxbuf[1500];

        buffer[0] = '\0';

        if ((size = recvfrom(rx_socket, rxbuf, sizeof(rxbuf), 0, &sa, (socklen_t*)&asize)) == -1) {
                perror("AX25 Socket recvfrom error");
                printf("%s:%s(): socket %d\n", __FILE__, __FUNCTION__, rx_socket);
        } else {
                if (packet_qualify(state, &sa, (unsigned char *)rxbuf+1, size-1)) {

                        ret = fap_ax25_to_tnc2(rxbuf+1, size-1, buffer, len);

                        if (!ret) {
                                fap_conversion_debug(rxbuf+1, buffer, *len);
                        } else {
                                pr_debug("Good convert: recvfrom size: %d, fap size: %u -", size, *len);
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

        /* Save the ax25 device name to be used by packet_qualify */
        state->ax25_dev = ax25dev;

        pr_debug("%s %s(): Port %s is using device %s\n",
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

        /* From monixd */
        if ( (rx_sock = socket(AF_INET, SOCK_PACKET, htons(state->ax25_recvproto))) == -1) {
                perror("receive AX.25 socket");
                return(-1);
        }

        /*
         * Could use socket type SOCK_RAW or SOCK_DGRAM?
         * htons(ETH_P_AX25)
         */
        if ( (tx_sock = socket(AF_AX25, SOCK_DGRAM, 0)) == -1) {
                perror("transmit AX.25 socket");
                return(-1);
        }

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

        /* Bind the tx socket to the source address */
        if (bind(tx_sock, (struct sockaddr *)&sockaddr_ax25_src, tx_src_addrlen) == -1) {
                perror("bind");
                return 1;
        }

        state->ax25_tx_sock = tx_sock;

        pr_debug("Connected to AX.25 stack on rx socket %d, tx socket %d\n", rx_sock, tx_sock);
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

        buf[0]='\0';
        strcpy(buf, packet);
        len=strlen(buf);

        pr_debug("Sending AX.25 beacon of %d bytes, with socket %d: %s\n",
                 len, sock, packet);

        if(strlen(aprspath) == 0 ) {
                fprintf(stderr, "%s:%s() no APRS path configured\n", __FILE__, __FUNCTION__);
                /* Fix this */
                return false;
        }

        if ((tx_dest_addrlen = ax25_aton(aprspath, &sockaddr_ax25_dest)) == -1) {
                perror("ax25_config_get_addr dest");
                return false;
        }
#ifdef DEBUG
        {
                int i;
                unsigned char *pBuf =(unsigned char *) &sockaddr_ax25_dest;

                printf("send_ax25_beacon: tx addr len %d\n", tx_dest_addrlen);
                for (i = 0; i < sizeof(sockaddr_ax25_dest); i++) {
                        printf("0x%02x ", *(pBuf+i));
                }
                printf("\n");
        }
#endif /* DEBUG */

        if ((ret=sendto(sock, buf, len, 0, (struct sockaddr *)&sockaddr_ax25_dest, tx_dest_addrlen)) == -1) {
                pr_debug("%s:%s(): after sendto sock msg len= %d, ret= 0x%02x\n",  __FILE__, __FUNCTION__, len, ret);
                perror("sendto");
                return false;
        }

        return (ret == len);
}

#ifdef MAIN

#include "util.h"
#include "aprs.h"

char* time2str(time_t *ptime);

int main(int argc, char **argv)
{
        int size = 0;
        char *program_name;
        unsigned char inbuf[1500];
        unsigned char *buf;
        struct state state;
        struct ifreq ifr;
        struct sockaddr sa;
        int asize = sizeof(sa);
        time_t current_time;
        char    *ax25_port;


        if ( (program_name=strrchr(argv[0], '/'))!=NULL) {  /* Get root program name */
                program_name++;
        } else {
                program_name = argv[0];
        }
        printf("%s v0.1.%04i (%s)\n", program_name, atoi(BUILD), REVISION);

        memset(&state, 0, sizeof(state));
        fap_init();

        if (parse_opts(argc, argv, &state)) {
                printf("Invalid option(s)\n");
                exit(1);
        }

        if (parse_ini(state.conf.config ? state.conf.config : "aprs.ini", &state)) {
                printf("Invalid config\n");
                exit(1);
        }

        state.tncfd = aprsax25_connect(&state);

        if (state.tncfd < 0) {
                printf("Sock %i: %m\n", state.tncfd);
                return 1;
        }

        while ((size = recvfrom(state.tncfd, inbuf, sizeof(inbuf), 0, &sa, (socklen_t*)&asize))) {

                if( size == -1) {
                        perror("AX25 Socket recvfrom error");
                        printf("%s:%s(): socket %d\n", __FILE__, __FUNCTION__, state.tncfd);
                        break;

                } else {
                        strcpy(ifr.ifr_name, sa.sa_data);
                        if (ioctl(state.tncfd, SIOCGIFHWADDR, &ifr) < 0
                            || ifr.ifr_hwaddr.sa_family != AF_AX25) {
                                continue;            /* not AX25 so ignore this packet */
                        }
                        /* Fix me: Why is the first byte in the
                         * receive buffer always a zero??
                         * From AX25 first byte specifies:
                         * 0 = packet
                         * non 0 = parameters
                         */
                        buf = &inbuf[1];
                        size--;

                        time(&current_time);

                        if ((ax25_port = ax25_config_get_name(sa.sa_data)) == NULL)
                                ax25_port = sa.sa_data;

                        if (packet_qualify(&state, &sa, buf, size)) {

                                char packet[512];
                                unsigned int len = sizeof(packet);
                                int ret;

                                printf("%s %s: ",
                                       time2str(&current_time),
                                       ax25_port);
                                fflush(stdout);

                                memset(packet, 0, len);

                                ret = fap_ax25_to_tnc2((char *)buf, size, packet, &len);

                                if (!ret) {
                                        fap_conversion_debug((char *)buf, packet, len);
                                } else {
                                        printf("[%d] - %s\n",
                                               size, packet);
                                }
                        }
                }
        }
        return 0;
}

#endif /* #ifdef MAIN */
#endif /* #ifdef HAVE_AX25_TRUE */
