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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netax25/kernel_ax25.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netax25/axlib.h>
#include <netax25/axconfig.h>

#include "aprs.h"
#include "aprs-is.h"
#include "aprs-ax25.h"


bool packet_qualify(struct state *state, struct sockaddr *sa, unsigned char *buf, int size)
{
        struct ifreq ifr;
        bool packet_status = true;

        if (state->ax25_recvproto == ETH_P_ALL) {       /* promiscuous mode? */

                strncpy(ifr.ifr_name, sa->sa_data, IF_NAMESIZE);

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
void dump_hex(unsigned char *buf, int len)
{
        int i;
        unsigned char *pBuf;
        pBuf = buf;

        for(i=0; i < len; i++) {
                printf("%02x ", *pBuf++);
        }
        printf("\n");
}

void fap_conversion_debug(char *rxbuf, char *buffer, int len)
{
        unsigned char *qBuf, *sBuf;
        int i;

        rxbuf[len] = '\0';
        printf("ax25frame: len=%d, rxbuf: %s\n",
               len, rxbuf);

        if(len && buffer != NULL) {
                printf("fap: %s\n", buffer);
        }

        /* dump hex */
        printf("hex:\n");
        dump_hex((unsigned char *)rxbuf, len);

        /* dump ascii */
        printf("ascii:\n");
        qBuf = (unsigned char *)rxbuf;

        for(i=0; i < len; i++) {
                printf("%2c ", *qBuf++);
        }
        printf("\n");

        /* Dump ascii with APRS converstion */
        printf("shifted:\n");
        sBuf = (unsigned char *)rxbuf;

        i = 0;
        while( ((*sBuf & 0x01) == 0) && (i++ < len) ) {
                printf("%2c ", *sBuf >> 1);
                sBuf++;
        }
        printf("\n");
}

int get_ax25packet(struct state *state, char *buffer, unsigned int *len)
{
        char rxbuf[1500];
        struct sockaddr_storage sa;
        socklen_t sa_len = sizeof(sa);
        int rx_socket = state->tncfd;
        int ret = 0;
        int size = 0;

        buffer[0] = '\0';

        if ((size = recvfrom(rx_socket, rxbuf, sizeof(rxbuf), 0, (struct sockaddr *)&sa, &sa_len)) == -1) {
                perror("AX25 Socket recvfrom error");
                printf("%s:%s(): socket %d\n", __FILE__, __FUNCTION__, rx_socket);
                fflush(stdout);
        } else {
                if (packet_qualify(state, (struct sockaddr *)&sa, (unsigned char *)rxbuf+1, size-1)) {

                        ret = fap_ax25_to_tnc2(rxbuf+1, size-1, buffer, len);

                        if (!ret) {
                                fap_conversion_debug(rxbuf+1, buffer, *len);
                        }
                        else {
#ifdef DEBUG_VERBOSE
                                pr_debug("fap ok: (%d/%u) - ", size, *len);
                                fflush(stdout);
#else
                                printf("."); fflush(stdout);

#endif /*  DEBUG_VERBOSE */

                        }
                }
        }
        return ret;
}

int aprsax25_connect(struct state *state)
{
        int rx_sock, tx_sock;
        unsigned int tx_src_addrlen, tx_dest_addrlen;   /* Length of addresses */
        char *ax25dev;
        struct full_sockaddr_ax25 sockaddr_ax25_dest;
        struct full_sockaddr_ax25 sockaddr_ax25_src;
        char *portcallsign, *src_addr;

        char *ax25port = state->conf.ax25_port;
        char *aprspath = state->conf.aprs_path;

        /* Protocol to use for receive, either ETH_P_ALL or ETH_P_AX25 */
        /* Choosing ETH_P_ALL would not only pick outbound packets, but all of
         *  the ethernet traffic..
         * ETH_P_AX25 picks only inbound-at-ax25-devices packets. */
        state->ax25_recvproto = ETH_P_AX25;

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

        /*
         * Open up a receive & transmit socket
         */
#if 0 /* why doesn' this work */
        if ( (rx_sock = socket(AF_AX25, SOCK_RAW, htons(state->ax25_recvproto))) == -1) {
                perror("receive AX.25 socket");
                return(-1);
        }
#endif

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

        /*
         * Choice of which source call sign to use
         * - config'ed in /etc/ax25/axports for the ax25 stack
         * - config'ed in aprs.ini as station:mycall
         */
        /* Get the port callsign from the ax25 stack */
        if ((portcallsign = ax25_config_get_addr(ax25port)) == NULL) {
                        perror("ax25_config_get_addr");
                        return -1;
        }

        /*
         * Check if the call sign in the ini file is the same as what's
         * config'ed for the AX.25 stack in /etc/ax25/axports.
         * If they are different set up source address for AX.25 stack.
         */
        if (state->mycall != NULL && strcmp(state->mycall, portcallsign) != 0) {
                if ((src_addr = (char *) malloc(strlen(state->mycall) + 1 + strlen(portcallsign) + 1)) == NULL)
                        return -1;
                sprintf(src_addr, "%s %s", state->mycall, portcallsign);
        } else {
                if ((src_addr = strdup(portcallsign)) == NULL)
                        return -1;
        }

        /* save it for convenience.
         * Displayed as source callsign for messaging on web interface
         */
        state->ax25_srcaddr = src_addr;
        pr_debug("%s(): Port %s is using device %s, callsign %s\n",
                 __FUNCTION__, ax25port, ax25dev, src_addr);

        /* Convert to AX.25 addresses */
        memset((char *)&sockaddr_ax25_src, 0, sizeof(sockaddr_ax25_src));
        memset((char *)&sockaddr_ax25_dest, 0, sizeof(sockaddr_ax25_dest));

        if ((tx_src_addrlen = ax25_aton(state->ax25_srcaddr, &sockaddr_ax25_src)) == -1) {
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

        pr_debug("%s(): Connected to AX.25 stack on rx socket %d, tx socket %d\n",
                 __FUNCTION__, rx_sock, tx_sock);
        fflush(stdout);

        return(rx_sock);
}

bool send_ax25_beacon(struct state *state, char *packet)
{
        char buf[512];
        char destcall[8];
        char destpath[32];
        char *pkt_start;
        int ret;
        unsigned int len = sizeof(buf);
        unsigned int tx_dest_addrlen;   /* Length of APRS path */
        char *aprspath = state->conf.aprs_path;
        struct full_sockaddr_ax25 sockaddr_ax25_dest;
        int sock = state->ax25_tx_sock;

        buf[0]='\0';
        strcpy(buf, packet);
        len=strlen(buf);

        if(strlen(aprspath) == 0 ) {
                fprintf(stderr, "%s:%s() no APRS path configured\n", __FILE__, __FUNCTION__);
                /* Fix this */
                return false;
        }
        /* set the aprs path */
        aprspath = state->conf.aprs_path;
        pkt_start = buf;

        /*
         * Mic-E is different as its data can be stored in the AX.25
         * destination address field
         *
         * The Mic-E data is passed to this routine as part of the
         * packet and here some of the Mic-E data is stripped out & put
         * in the destination address field.
         * Note: Currently only using 6 of the 7 available AX.25 destination
         * address field bytes.
         * */
        if(packet[0] == APRS_DATATYPE_CURRENT_MIC_E_DATA) {
                strncpy(destcall, &packet[1], MIC_E_DEST_ADDR_LEN - 1);
                destcall[MIC_E_DEST_ADDR_LEN - 1]='\0';
                sprintf(destpath, "%s via %s", destcall, state->conf.digi_path);
                aprspath = destpath;
                buf[MIC_E_DEST_ADDR_LEN-1] = APRS_DATATYPE_CURRENT_MIC_E_DATA;
                pkt_start = &buf[MIC_E_DEST_ADDR_LEN - 1];
                len -= (MIC_E_DEST_ADDR_LEN - 1);
        }
        memset((char *)&sockaddr_ax25_dest, 0, sizeof(sockaddr_ax25_dest));

        if ((tx_dest_addrlen = ax25_aton(aprspath, &sockaddr_ax25_dest)) == -1) {
                perror("ax25_config_get_addr dest");
                return false;
        }
#ifdef DEBUG_1
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

        if ((ret=sendto(sock, pkt_start, len, 0, (struct sockaddr *)&sockaddr_ax25_dest, tx_dest_addrlen)) == -1) {
                pr_debug("%s:%s(): after sendto sock msg len= %d, ret= 0x%02x\n",  __FILE__, __FUNCTION__, len, ret);
                perror("sendto");
                return false;
        }

        return (ret == len);
}

#ifdef MAIN

//#define PKT_DUMP_FAP
#define PKT_DUMP_SPY
/* Enable web display of SPY packets */
#define SPY_SOCK_ENABLE

#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <json/json.h>

#include "monixd.h"
#include "util.h"
#include "aprs.h"

char *ax25packet_dump(struct t_ax25packet *pax25packet, int dumpstyle);
struct t_ax25packet *ax25_decode( struct t_ax25packet *ax25packet , unsigned char *data, int length);

int send_ui_sock(int sock, char *msg)
{
        int ret;
        int len;

        len=strlen(msg);

        ret = send(sock, msg, len, MSG_NOSIGNAL);

        return ret;
}

int ui_sock_cfg(struct state *state)
{
        struct sockaddr *dest;
        int sock;

        struct sockaddr_un sun;

        sun.sun_family = AF_UNIX;
        strcpy(sun.sun_path, state->conf.ui_sock_path);

        dest=(struct sockaddr *)&sun;
        sock = socket(dest->sa_family, SOCK_STREAM, 0);
        if (sock < 0) {
                perror("socket");
                return -errno;
        }

        if (connect(sock, dest, sizeof(sun))) {
                fprintf(stderr, "%s: connect error on ui socket %d, path: %s, error: %s\n",
                          __FUNCTION__, sock,  state->conf.ui_sock_path, strerror(errno));

                return -errno;
        }

        return sock;
}

int ui_sock_wait(struct state *state)
{
        int ui_sock, retcode;
        struct stat sts;


        /* get rid of the Unix socket */
        unlink(state->conf.ui_sock_path);
        /* verify Unix socket removal was a success */
        retcode = stat(state->conf.ui_sock_path, &sts);
        if(retcode != -1 || errno != ENOENT) {
                printf("Problem deleting Unix socket %s, stat returned an errno of 0x%02x %s\n",
                       state->conf.ui_sock_path, errno, strerror(errno));
        }

        /* Wait for the socket to be created by the node script */
        printf("Waiting for node script to create a Unix socket\n");
        while (stat(state->conf.ui_sock_path, &sts) == -1 && errno == ENOENT) {
                sleep(1);
        }
        sleep(2);

        if( (ui_sock=ui_sock_cfg(state)) < 0) {
                printf("%s: Failed to connect to UI socket, error: 0x%02x\n",
                       __FUNCTION__, -ui_sock);
                return ui_sock;
        } else {
                printf("UI Socket %s, connected with sock %d\n",
                       state->conf.ui_sock_path, ui_sock);
        }
        return ui_sock;
}

void handle_ax25_pkt(struct state *state, struct sockaddr *sa, unsigned char *buf, int size, int ui_sock)
{
        struct t_ax25packet ax25packet;

        /* Fix me: Why is the first byte in the
         * receive buffer always a zero??
         * From AX25 first byte specifies:
         * 0 = packet
         * non 0 = parameters
         */
        memset( &ax25packet, 0, sizeof(ax25packet) );
        time(&ax25packet.time);

        if ((ax25packet.port = ax25_config_get_name(sa->sa_data)) == NULL)
                ax25packet.port = sa->sa_data;

#ifdef PKT_DUMP_FAP
        if (packet_qualify(state, &sa, buf, size)) {

                char packet[512];
                unsigned int len = sizeof(packet);
                int ret;

                printf("%s %s: ",
                       time2str(&ax25packet.time, 0),
                       ax25packet.port);
                fflush(stdout);

                memset(packet, 0, len);

                ret = fap_ax25_to_tnc2((char *)buf, size, packet, &len);

                if (!ret) {
                        printf("************************\n");
                        fap_conversion_debug((char *)buf, packet, len);
                        printf("========================\n");
                        ax25_decode( &ax25packet, buf, size );
                        ax25packet_dump( &ax25packet, READABLE );
                        printf("========================\n");

                } else {
                        printf("[%d] - %s\n",
                               size, packet);
                }
        }
#endif /* PKT_DUMP_FAP */
#ifdef PKT_DUMP_SPY
        if (packet_qualify(state, sa, buf, size)) {
                static unsigned long pktcount = 0;
                char *dumpbuf, *strpktcount = NULL;
                json_object *spy_object;

                ax25_decode( &ax25packet, buf, size );
                dumpbuf = ax25packet_dump( &ax25packet, READABLE );

                pktcount++;
                asprintf( &strpktcount, "%lu", pktcount);

#ifdef SPY_SOCK_ENABLE
                spy_object = json_object_new_object();
                json_object_object_add(spy_object, "spy", json_object_new_string(dumpbuf));
                json_object_object_add(spy_object, "count", json_object_new_string(strpktcount));

#ifdef TEST_TCPSTREAM
                /* concatenate received packets */
                {
                        char *lastpkt, *currpkt;
                        int index_pktarray=0;

                        if(index_pktarray == 0) {
                                lastpkt = (char *)json_object_to_json_string(spy_object);
                                index_pktarray++;
                        } else {
                                char * thispkt = (char *)json_object_to_json_string(spy_object);
                                index_pktarray = 0;
                                currpkt=malloc(strlen(lastpkt) + strlen(thispkt) + 1);
                                strcpy(currpkt, lastpkt);
                                strcat(currpkt, thispkt);
                                send_ui_sock(ui_sock, currpkt);

                                json_object_put(spy_object);
                        }
                }
#else
                send_ui_sock(ui_sock, (char *)json_object_to_json_string(spy_object));
                json_object_put(spy_object);

#endif  /*  TEST_TCPSTREAM */

                free(dumpbuf);
                free(strpktcount);
#endif /*  SPY_SOCK_ENABLE */

        }
#endif /* PKT_DUMP_SPY */

}

/*
 * For Demo purposes display packets previously saved to a file
 */
int canned_packets(struct state *state)
{
        unsigned char inbuf[1500];
        int pkt_size;
        int bytes_read = 1;
        int cnt_pkts = 0;
        int cnt_bytes = 0;
        recpkt_t recpkt;
        time_t last_time, wait_time;
        struct sockaddr sa;
        FILE *fsrecpkt;
#ifdef SPY_SOCK_ENABLE
        int ui_sock;
#endif /*  SPY_SOCK_ENABLE */

        /* open play back file */
        fsrecpkt = fopen(state->debug.playback_pkt_filename, "rb");
        if (fsrecpkt == NULL) {
                perror(state->debug.playback_pkt_filename);
                return errno;
        }
#ifdef SPY_SOCK_ENABLE
        if((ui_sock=ui_sock_wait(state)) < 0) {
                return 1;
        }
#endif /*  SPY_SOCK_ENABLE */

        printf("Playing back packets from file %s\n", state->debug.playback_pkt_filename);

        /* read the starting time of the captured packets */
        if(fread(&recpkt, sizeof(recpkt_t), 1, fsrecpkt) != 1) {
                fprintf(stderr, "Problem reading first bytes of playback file\n");
                return 1;
        }
        rewind(fsrecpkt);

        /* calc offset of current time & time of recorded
         * packets */

        last_time = recpkt.currtime;

        strcpy(sa.sa_data, "File");

        while(bytes_read > 0) {
                if ((bytes_read = fread(&recpkt, sizeof(recpkt_t), 1, fsrecpkt)) != 1) {
                        if(bytes_read == 0) {
                                printf("Finished reading all %d packets, %zu bytes\n",
                                       cnt_pkts, cnt_bytes + cnt_pkts*sizeof(recpkt_t));
                                break;
                        }
                        fprintf(stderr, "%s: Error reading file: 0x%02x %s %s\n",
                                  __FUNCTION__, errno, strerror(errno), state->debug.playback_pkt_filename);
                        return errno;
                }
                pkt_size = recpkt.size;

                bytes_read = fread(inbuf, 1, pkt_size, fsrecpkt);
                if(bytes_read != pkt_size) {
                        printf("Read error expected %d, read %d\n",
                               pkt_size, bytes_read);
                        break;
                }

                cnt_pkts++;
                cnt_bytes += bytes_read;

                printf("Pkt count %d, size %d, time %s ... ",
                       cnt_pkts, pkt_size, time2str(&recpkt.currtime, 0));

                /* pace the packets */
                wait_time = recpkt.currtime - last_time;

                printf("Waiting for %lu secs", wait_time);

                /* Pace packets quicker than reality */
                if(wait_time > 0)
                        wait_time = wait_time / state->debug.playback_time_scale;

                printf(", new wait %lu seconds", wait_time);
                fflush(stdout);

                if(wait_time < 0 || wait_time > 5*60) {
                        printf("Wait time seems unreasonable %lu\n", wait_time);
                        wait_time = 60;
                }
                last_time = recpkt.currtime;

                sleep(wait_time);

                printf("\n");

                handle_ax25_pkt(state, &sa, &inbuf[1], bytes_read-1, ui_sock);
        }
        return 0;
}

int live_packets(struct state *state)
{
        unsigned char inbuf[1500];
        struct sockaddr_storage sa;
        socklen_t sa_len = sizeof sa;
        recpkt_t recpkt;
        FILE *fsrecpkt;
        char *fname;
        int size = 0;
#ifdef SPY_SOCK_ENABLE
        int ui_sock;
#endif /*  SPY_SOCK_ENABLE */



        /* Live packet handling */
        state->tncfd = aprsax25_connect(state);
        printf("Debug: aprsax25_connect: 0x%02x\n", state->tncfd);
        if (state->tncfd < 0) {
                printf("Sock %i: %m\n", state->tncfd);
                return -1;
        }

#ifdef SPY_SOCK_ENABLE
        if((ui_sock = ui_sock_wait(state)) < 0) {
                return -1;
        }
#endif /*  SPY_SOCK_ENABLE */

        /*
         * Check if recording packets is enabled
         *  - grab a file descriptor
         */
        if(state->debug.record_pkt_filename != NULL) {
                time_t currtime;
                currtime = time(NULL);

                asprintf(&fname, "%s_%s.log",
                         state->debug.record_pkt_filename,
                         time2str(&currtime, 1));

                /* open a record file */
                fsrecpkt = fopen(fname, "wb");
                if (fsrecpkt == NULL) {
                        perror(fname);
                        return errno;
                }
                printf("Recording packets to file %s\n", fname);
        }

        while ((size = recvfrom(state->tncfd, inbuf, sizeof(inbuf), 0, (struct sockaddr *)&sa, &sa_len))) {

                /* is recording packets enabled ? */
                if(state->debug.record_pkt_filename != NULL) {
                        recpkt.currtime = time(NULL);
                        recpkt.size = size;
                        fwrite(&recpkt, sizeof(recpkt_t), 1, fsrecpkt);

                        if( fwrite(inbuf, size, 1, fsrecpkt) != 1) {
                                fflush(fsrecpkt);
                                fclose(fsrecpkt);
                                printf("Error %s writing to %s",
                                       strerror(errno), fname);
                                return errno;
                        }
                        fflush(fsrecpkt);
                }

                if(size > 0) {

                        size--;
                        handle_ax25_pkt(state, (struct sockaddr *)&sa, &inbuf[1], size, ui_sock);

                } else {
                        perror("AX25 Socket recvfrom error");
                        printf("%s:%s(): socket %d\n", __FILE__, __FUNCTION__, state->tncfd);
                        break;
                }
        }
        return 1;
}

int main(int argc, char **argv)
{
        char *program_name;
        struct state state;

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

        /*
         * Check if playback packets is enabled
         */
        if(state.debug.playback_pkt_filename != NULL) {
                canned_packets(&state);
        } else {
                live_packets(&state);
        }
        return 0;
}

#endif /* #ifdef MAIN */
#endif /* #ifdef HAVE_AX25_TRUE */
