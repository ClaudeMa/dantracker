/* -*- Mode: C; tab-width: 8;  indent-tabs-mode: nil; c-basic-offset: 8; c-brace-offset: -8; c-argdecl-indent: 8 -*- */
/* Copyright 2013 Basil Gunn <basil@pacabunga.com> */

#ifndef __APRS_MSG_H
#define __APRS_MSG_H

bool isnewmessage(struct state *state, fap_packet_t *fap);
void webdisplay_message(struct state *state, fap_packet_t *fap);
void webdisplay_thirdparty(struct state *state, fap_packet_t *fap);
void save_message(struct state *state, fap_packet_t *fap);
int send_message(struct state *state, char *msg);
void handle_aprsMessages(struct state *state, fap_packet_t *fap, char *packet);
void handle_thirdparty(struct state *state, fap_packet_t *fap);

/* Debug only */
void display_fap_message(struct state *state, fap_packet_t *fap);

#endif /* APRS_MSG_H */
