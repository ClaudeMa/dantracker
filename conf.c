/* -*- Mode: C; tab-width: 8;  indent-tabs-mode: nil; c-basic-offset: 8; c-brace-offset: -8; c-argdecl-indent: 8 -*- */

/* dantracker aprs tracker
 *
 * Copyright 2012 Dan Smith <dsmith@danplanet.com>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <sys/un.h>
#include <netdb.h>
#include <iniparser.h>

#include "ui.h"
#include "util.h"
#include "aprs.h"

#define TIER2_HOST_NAME ".aprs2.net"

void usage(char *argv0)
{
        printf("Usage:\n"
               "%s [OPTS]\n"
               "Options:\n"
               "  --help, -h       This help message\n"
               "  --tnc, -t        Serial port for TNC\n"
               "  --gps, -g        Serial port for GPS\n"
               "  --telemetry, -T  Serial port for telemetry\n"
               "  --testing        Testing mode (faked speed, course, digi)\n"
               "  --verbose, -v    Log to stdout\n"
               "  --conf, -c       Configuration file to use\n"
               "  --display, -d    Host to use for display over INET socket\n"
               "  --netrange, -r   Range (miles) to use for APRS-IS filter\n"
               "\n",
               argv0);
}

int lookup_host(struct state *state, const char *hostname)
{
        struct hostent *host;
        struct sockaddr_in *sa = (struct sockaddr_in *)&state->conf.display_to;

        host = gethostbyname(hostname);
        if (!host) {
                perror(hostname);
                return -errno;
        }

        if (host->h_length < 1) {
                fprintf(stderr, "No address for %s\n", hostname);
                return -EINVAL;
        }

        sa->sin_family = AF_INET;
        sa->sin_port = htons(SOCKPORT);
        memcpy(&sa->sin_addr, host->h_addr_list[0], sizeof(sa->sin_addr));

        return 0;
}

int parse_opts(int argc, char **argv, struct state *state)
{
        static struct option lopts[] = {
                {"help",      0, 0, 'h'},
                {"tnc",       1, 0, 't'},
                {"gps",       1, 0, 'g'},
                {"telemetry", 1, 0, 'T'},
                {"testing",   0, 0,  1 },
                {"verbose",   0, 0, 'v'},
                {"conf",      1, 0, 'c'},
                {"display",   1, 0, 'd'},
                {"netrange",  1, 0, 'r'},
                {NULL,        0, 0,  0 },
        };

        state->conf.display_to.sa_family = AF_UNIX;
        strcpy(((struct sockaddr_un *)&state->conf.display_to)->sun_path,
               SOCKPATH);

        state->conf.aprsis_range = 100;

        while (1) {
                int c;
                int optidx;

                c = getopt_long(argc, argv, "ht:g:T:c:svd:r:",
                                lopts, &optidx);
                if (c == -1)
                        break;

                switch(c) {
                        case 'h':
                                usage(argv[0]);
                                exit(1);
                        case 't':
                                state->conf.tnc = optarg;
                                break;
                        case 'g':
                                state->conf.gps = optarg;
                                break;
                        case 'T':
                                state->conf.tel = optarg;
                                break;
                        case 1:
                                state->conf.testing = 1;
                                break;
                        case 'v':
                                state->conf.verbose = 1;
                                break;
                        case 'c':
                                state->conf.config = optarg;
                                break;
                        case 'd':
                                lookup_host(state, optarg);
                                break;
                        case 'r':
                                state->conf.aprsis_range = \
                                        (unsigned int)strtoul(optarg, NULL, 10);
                                break;
                        case '?':
                                printf("Unknown option\n");
                                return -1;
                };
        }

        return 0;
}

char *process_tnc_cmd(char *cmd)
{
        char *ret;
        char *a, *b;

        ret = malloc(strlen(cmd) * 2);
        if (ret < 0)
                return NULL;

        for (a = cmd, b = ret; *a; a++, b++) {
                if (*a == ',')
                        *b = '\r';
                else
                        *b = *a;
        }

        *b = '\0';

        //printf("TNC command: `%s'\n", ret);

        return ret;
}

char **parse_list(char *string, int *count)
{
        char **list;
        char *ptr;
        int i = 0;

        for (ptr = string; *ptr; ptr++)
                if (*ptr == ',')
                        i++;
        *count = i+1;

        list = calloc(*count, sizeof(char **));
        if (!list)
                return NULL;

        for (i = 0; string; i++) {
                ptr = strchr(string, ',');
                if (ptr) {
                        *ptr = 0;
                        ptr++;
                }
                list[i] = strdup(string);
                string = ptr;
        }

        return list;
}

int parse_ini(char *filename, struct state *state)
{
        dictionary *ini;
        char *tmp, *tmp1;

        ini = iniparser_load(filename);
        if (ini == NULL)
                return -EINVAL;

        if (!state->conf.tnc)
                state->conf.tnc = iniparser_getstring(ini, "tnc:port", NULL);
        state->conf.tnc_rate = iniparser_getint(ini, "tnc:rate", 9600);
        state->conf.tnc_type = iniparser_getstring(ini, "tnc:type", "KISS");

        tmp = iniparser_getstring(ini, "tnc:init_kiss_cmd", "");
        state->conf.init_kiss_cmd = process_tnc_cmd(tmp);

        if (!state->conf.gps)
                state->conf.gps = iniparser_getstring(ini, "gps:port", NULL);
        state->conf.gps_type = iniparser_getstring(ini, "gps:type", "static");
        state->conf.gps_rate = iniparser_getint(ini, "gps:rate", 4800);

        /* Build the TIER 2 host name */
        tmp = iniparser_getstring(ini, "net:server_host_address", "oregon");

        state->conf.net_server_host_addr = calloc(sizeof(tmp)+sizeof(TIER2_HOST_NAME)+1, sizeof(char));
        sprintf(state->conf.net_server_host_addr,"%s%s", tmp, TIER2_HOST_NAME);

        /* Get the AX25 port name */
        state->conf.ax25_port = iniparser_getstring(ini, "ax25:port", "undefined");

        /* Get the AX25 device filter setting */
        tmp = iniparser_getstring(ini, "ax25:device_filter", "OFF");

        strupper(tmp);
        if(strcmp(tmp, "OFF") == 0) {
                state->conf.ax25_pkt_device_filter = AX25_PKT_DEVICE_FILTER_OFF;
        } else if(strcmp(tmp, "ON") == 0) {
                state->conf.ax25_pkt_device_filter = AX25_PKT_DEVICE_FILTER_ON;
        } else {
                state->conf.ax25_pkt_device_filter = AX25_PKT_DEVICE_FILTER_UNDEFINED;
        }

        /* Get the APRS transmit path */
        state->conf.aprs_path = iniparser_getstring(ini, "ax25:aprspath", "");

        if (!state->conf.tel)
                state->conf.tel = iniparser_getstring(ini, "telemetry:port",
                        NULL);
        state->conf.tel_rate = iniparser_getint(ini, "telemetry:rate", 9600);

        state->mycall = iniparser_getstring(ini, "station:mycall", "N0CALL-7");

        /* Verify call sign */
        tmp=strdup(state->mycall);
        tmp1=tmp;
        while ((*tmp1 != '-') && (*tmp1 != 0)) {
                tmp1++;
        }
        *tmp1 = '\0';

        strupper(tmp);
        if(strcmp(tmp, "NOCALL") == 0 ) {
                printf("Configure file %s with your callsign\n",
                       state->conf.config ? state->conf.config : "aprs.ini");
                free(tmp);
                return(-1);
        }
        free(tmp);

        pr_debug("calling iniparser for station:mycall %s\n", state->mycall);

        state->conf.icon = iniparser_getstring(ini, "station:icon", "/>");

        if (strlen(state->conf.icon) != 2) {
                printf("ERROR: Icon must be two characters, not `%s'\n",
                       state->conf.icon);
                return -1;
        }

        state->conf.digi_path = iniparser_getstring(ini, "station:digi_path",
                "WIDE1-1,WIDE2-1");

        state->conf.power = iniparser_getint(ini, "station:power", 0);
        state->conf.height = iniparser_getint(ini, "station:height", 0);
        state->conf.gain = iniparser_getint(ini, "station:gain", 0);
        state->conf.directivity = iniparser_getint(ini, "station:directivity",
                0);

        state->conf.atrest_rate = iniparser_getint(ini,
                "beaconing:atrest_rate",
                600);
        state->conf.sb_low.speed = iniparser_getint(ini,
                "beaconing:min_speed",
                10);
        state->conf.sb_low.int_sec = iniparser_getint(ini,
                "beaconing:min_rate",
                600);
        state->conf.sb_high.speed = iniparser_getint(ini,
                "beaconing:max_speed",
                60);
        state->conf.sb_high.int_sec = iniparser_getint(ini,
                "beaconing:max_rate",
                60);
        state->conf.course_change_min = iniparser_getint(ini,
                "beaconing:course_change_min",
                30);
        state->conf.course_change_slope = iniparser_getint(ini,
                "beaconing:course_change_slope",
                255);
        state->conf.after_stop = iniparser_getint(ini,
                "beaconing:after_stop",
                180);

        state->conf.static_lat = iniparser_getdouble(ini,
                "static:lat",
                0.0);
        state->conf.static_lon = iniparser_getdouble(ini,
                "static:lon",
                0.0);
        state->conf.static_alt = iniparser_getdouble(ini,
                "static:alt",
                0.0);
        state->conf.static_spd = iniparser_getdouble(ini,
                "static:speed",
                0.0);
        state->conf.static_crs = iniparser_getdouble(ini,
                "static:course",
                0.0);

        state->conf.digi_alias = iniparser_getstring(ini, "digi:alias",
                "TEMP1-1");
        state->conf.digi_enabled = iniparser_getint(ini, "digi:enabled", 0);
        state->conf.digi_append = iniparser_getint(ini, "digi:append_path",
                0);
        state->conf.digi_delay = iniparser_getint(ini, "digi:txdelay", 500);

        tmp = iniparser_getstring(ini, "station:beacon_types", "posit");
        if (strlen(tmp) != 0) {
                char **types;
                int count;
                int i;

                types = parse_list(tmp, &count);
                if (!types) {
                        printf("Failed to parse beacon types\n");
                        return -EINVAL;
                }

                for (i = 0; i < count; i++) {
                        if (STREQ(types[i], "weather"))
                                state->conf.do_types |= DO_TYPE_WX;
                        else if (STREQ(types[i], "phg"))
                                state->conf.do_types |= DO_TYPE_PHG;
                        else
                                printf("WARNING: Unknown beacon type %s\n",
                                       types[i]);
                        free(types[i]);
                }
                free(types);
        }

        tmp = iniparser_getstring(ini, "comments:enabled", "");
        if (strlen(tmp) != 0) {
                int i;

                state->conf.comments = parse_list(tmp,
                        &state->conf.comments_count);
                if (!state->conf.comments)
                        return -EINVAL;

                for (i = 0; i < state->conf.comments_count; i++) {
                        char section[32];

                        snprintf(section, sizeof(section),
                                 "comments:%s", state->conf.comments[i]);
                        free(state->conf.comments[i]);
                        state->conf.comments[i] = iniparser_getstring(ini,
                                section,
                                "INVAL");
                }
        }
        /*
         * Check that mycall has been set in ini file
         */
        if (STREQ(state->mycall, "NOCALL")) {
                printf("ERROR: Please config ini file with your call sign\n");
                return -1;
        }

        return 0;
}
