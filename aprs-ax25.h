/* Copyright 2012 Dan Smith <dsmith@danplanet.com> */

#ifndef __APRS_AX25_H
#define __APRS_AX25_H

#define APRS_PORT_FILTERED_FEED 14580

int get_ax25packet(struct state *state, char *buffer, unsigned int *len);
int aprsax25_connect(struct state *state);
int send_ax25_beacon(struct state *state, char *packet);

#endif /* APRS_AX25_H */
