/* -*- Mode: C; tab-width: 8;  indent-tabs-mode: nil; c-basic-offset: 8; c-brace-offset: -8; c-argdecl-indent: 8 -*- */

/* dantracker aprs tracker
 *
 * Copyright 2012 Dan Smith <dsmith@danplanet.com>
 */

#ifndef __APRS__H
#define __APRS__H

#include <sys/socket.h>
#include <fap.h>
#include <iniparser.h>
#include <stdbool.h>

#include "nmea.h"

#define MAX_AX25_DIGIS 8        /* Maximim number of digipeaters: */
#define MAX_APRS_MSG_LEN (67)
#define MAX_CALLSIGN  9
#define MAX_MSGID     5
#define KEEP_MESSAGES 8

#define KEEP_PACKETS 8
#define KEEP_POSITS  4

#define DO_TYPE_NONE 0
#define DO_TYPE_WX   1
#define DO_TYPE_PHG  2

#define TZ_OFFSET (-8)

#define APRS_DATATYPE_POS_NO_TIME_NO_MSG '!'
#define APRS_DATATYPE_POS_NO_TIME_WITH_MSG '='
#define APRS_DATATYPE_POS_WITH_TIME_WITH_MSG '@'
#define APRS_DATATYPE_POS_WITH_TIME_NO_MSG '/'
#define APRS_DATATYPE_CURRENT_MIC_E_DATA '`'

#ifdef DEBUG
#define pr_debug(format, ...) fprintf (stderr, "DEBUG: "format, ## __VA_ARGS__)
#else
#define pr_debug(format, ...)
#endif

/* Define config bits for console_display_filter */
#define CONSOLE_DISPLAY_ALL    (0xff)
#define CONSOLE_DISPLAY_WX     (0x01)
#define CONSOLE_DISPLAY_MSG    (0x02)
#define CONSOLE_DISPLAY_FAPERR (0x04)
#define CONSOLE_DISPLAY_DEBUG  (0x80)



/* APRS message */
typedef struct aprsmsg {
        bool acked;
        char *message_id;
        time_t timestamp;
        char srccall[MAX_CALLSIGN+1];
        char dstcall[MAX_CALLSIGN+1];
        char *message;
} aprsmsg_t;

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
                char *ui_sock_path;
                char *ui_host_name;
                unsigned int ui_inet_port;

                unsigned int aprsis_range;
                int metric_units;
                bool aprs_message_ack;
                dictionary *ini_dict;

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
        int myssid;
        char *basecall;

        char *ax25_dev;                 /* device name */
        char *ax25_portcallsign;        /* callsign assigned to port name */
        int ax25_recvproto;             /* Protocol to use for receive ETH_P_ALL or ETH_P_AX25 */
        int ax25_tx_sock;

        int tncfd;
        int gpsfd;
        int telfd;
        int dspfd;

        aprsmsg_t *msghist[KEEP_MESSAGES];
        int msghist_idx;

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
        struct {
                unsigned long inPktCount;
                unsigned long inMsgCount;
                unsigned long inWxCount;
                unsigned long encapCount;
                unsigned long fapErrCount;
        } stats;

        struct {
                char *record_pkt_filename;
                char *playback_pkt_filename;
                int playback_time_scale;
                int console_display_filter;
        } debug;
};

/*
 * defines the header written before each canned packet
 */
typedef struct recpkt {
        time_t currtime;
        int size;
} recpkt_t;


int parse_ini(char *filename, struct state *state);
int parse_opts(int argc, char **argv, struct state *state);
void ini_cleanup(struct state *state);
int lookup_host(struct state *state);

#endif /* APRS_H */
