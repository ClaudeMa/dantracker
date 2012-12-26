/* -*- Mode: C; tab-width: 8;  indent-tabs-mode: nil; c-basic-offset: 8; c-brace-offset: -8; c-argdecl-indent: 8 -*- */
/* Copyright 2012 Dan Smith <dsmith@danplanet.com> */

#ifndef __APRS_AX25_H
#define __APRS_AX25_H

#define ISAX25ADDR(x) (((x >= 'A') && (x <= 'Z')) || ((x >= '0') && (x <= '9')) || (x == ' '))

int get_ax25packet(struct state *state, char *buffer, unsigned int *len);
int aprsax25_connect(struct state *state);
int send_ax25_beacon(struct state *state, char *packet);

#endif /* APRS_AX25_H */
