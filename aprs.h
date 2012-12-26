/* -*- Mode: C; tab-width: 8;  indent-tabs-mode: nil; c-basic-offset: 8; c-brace-offset: -8; c-argdecl-indent: 8 -*- */

/* dantracker aprs tracker
 *
 * Copyright 2012 Dan Smith <dsmith@danplanet.com>
 */

#ifndef __APRS__H
#define __APRS__H

#include <sys/socket.h>
#include <fap.h>
#include "nmea.h"

#define KEEP_PACKETS 8
#define KEEP_POSITS  4

#define DO_TYPE_NONE 0
#define DO_TYPE_WX   1
#define DO_TYPE_PHG  2

#define TZ_OFFSET (-8)

#ifdef DEBUG
#define pr_debug(format, ...) fprintf (stderr, "DEBUG: "format, ## __VA_ARGS__)
#else
#define pr_debug(format, ...)
#endif

/* AX.25 Packet Device Filter */
typedef enum {
        AX25_PKT_DEVICE_FILTER_UNDEFINED,
        AX25_PKT_DEVICE_FILTER_OFF,
        AX25_PKT_DEVICE_FILTER_ON
} ax25_pkt_filter_t;

struct smart_beacon_point {
        float int_sec;
        float speed;
};

struct state {
        struct {
                char *tnc;
                int tnc_rate;
                char *gps;
                int gps_rate;
                char *tel;
                int tel_rate;
                char *net;

                char *tnc_type;
                char *gps_type;
                char *net_server_host_addr;
                int ax25_pkt_device_filter;
                char *ax25_port;
                char *aprs_path;
                int testing;
                int verbose;
                char *icon;

                char *digi_path;

                int power;
                int height;
                int gain;
                int directivity;

                int atrest_rate;
                struct smart_beacon_point sb_low;
                struct smart_beacon_point sb_high;
                int course_change_min;
                int course_change_slope;
                int after_stop;

                unsigned int do_types;

                char **comments;
                int comments_count;

                char *config;

                double static_lat, static_lon, static_alt;
                double static_spd, static_crs;

                char *init_kiss_cmd;

                char *digi_alias;
                int digi_enabled;
                int digi_append;
                int digi_delay;

                struct sockaddr display_to;

                unsigned int aprsis_range;
        } conf;

        struct posit mypos[KEEP_POSITS];
        int mypos_idx;

        struct posit last_beacon_pos;

        struct {
                double temp1;
                double voltage;

                time_t last_tel_beacon;
                time_t last_tel;
        } tel;

        char *mycall;
        char *myssid;

        char *ax25_dev;
        int ax25_recvproto;         /* Protocol to use for receive ETH_P_ALL or ETH_P_AX25 */
        int ax25_tx_sock;

        int tncfd;
        int gpsfd;
        int telfd;
        int dspfd;

        fap_packet_t *last_packet; /* In case we don't store it below */
        fap_packet_t *recent[KEEP_PACKETS];
        int recent_idx;
        int disp_idx;

        char gps_buffer[128];
        int gps_idx;
        time_t last_gps_update;
        time_t last_gps_data;
        time_t last_beacon;
        time_t last_time_set;
        time_t last_moving;
        time_t last_status;

        fap_packet_t *last_wx;

        int comment_idx;
        int other_beacon_idx;

        uint8_t digi_quality;
};

int parse_ini(char *filename, struct state *state);
int parse_opts(int argc, char **argv, struct state *state);

#endif /* APRS_H */
