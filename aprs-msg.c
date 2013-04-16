/* -*- Mode: C; tab-width: 8;  indent-tabs-mode: nil; c-basic-offs_t: 8; c-brace-offset: -8; c-argdecl-indent: 8 -*- */
/* Copyright 2013 Basil Gunn <basil@pacabunga.com> */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>

#include "util.h"
#include "aprs.h"
#include "aprs-ax25.h"
#include "aprs-msg.h"

#ifdef  WEBAPP
char *format_msgtime(time_t *timestamp)
{
        static char timestr[32];
        struct tm *tm;
        time_t curtime;
        time_t *thistime;

        if(timestamp == NULL) {
                curtime = time(NULL);
                thistime=&curtime;
        }else {
                thistime=timestamp;
        }
        tm=localtime(thistime);

        strftime(timestr, sizeof(timestr), "%H%M", tm);

        return timestr;
}

bool isnewmessage(struct state *state, fap_packet_t *fap)
{
        aprsmsg_t *lastmsg;
        int i;

        /*
         * Handle an ack
         */
        if(fap->message_ack != NULL) {
                return(false);
        }

        for(i = 0; i < KEEP_MESSAGES; i++) {
                if(state->msghist[i] != NULL) {

                        lastmsg = state->msghist[i];

#if 0
                        printf("Message: Comparing these src calls: %s, %s, %d\n",
                               lastmsg->srccall, OBJNAME(fap), STREQ(lastmsg->srccall, OBJNAME(fap) ) );

                        printf("Message: Comparing these dst calls: %s, %s, %d\n",
                               lastmsg->dstcall, fap->destination, STREQ(lastmsg->dstcall, fap->destination) );

                        printf("Message: Comparing these msgs: %s, %s, %d\n",
                               lastmsg->message, fap->message, STREQ(lastmsg->message, fap->message) );
#endif

                        /*
                         * If the timestamps are ever compared need to add in
                         * some slop for jitter.
                         */
                        if( STREQ(lastmsg->srccall, OBJNAME(fap) ) &&
                            STREQ(lastmsg->dstcall, fap->destination) &&
                            STREQ(lastmsg->message, fap->message) ) {
                                return(false);
                        }
                }
        }

        return(true);
}

void save_message(struct state *state, fap_packet_t *fap)
{
        int msgind = state->msghist_idx;
        char basecall[MAX_CALLSIGN];
        int ssid;
        aprsmsg_t *aprsmsg;
        char aprs_ack[24]; /* largest APRS Message Ack is 19 bytes */

        assert(msgind < KEEP_MESSAGES && msgind >= 0);

        if(state->msghist[msgind] != NULL) {
                aprsmsg = state->msghist[msgind];
                free(aprsmsg->message);
                if (aprsmsg->message_id != NULL) {
                        free(aprsmsg->message_id);
                }
        } else {
                if( (state->msghist[msgind] = calloc(sizeof(aprsmsg_t), 1)) == NULL) {
                        printf("%s: malloc message struct error: %s\n",
                               __FUNCTION__, strerror(errno));
                        return;
                }
                aprsmsg = state->msghist[msgind];
        }

        if( (aprsmsg->message = malloc(strlen(fap->message) + 1)) == NULL){
                printf("%s: malloc message error: %s\n",
                       __FUNCTION__, strerror(errno));
                free(aprsmsg);
                return;
        }

        if(fap->timestamp == NULL) {
                aprsmsg->timestamp = time(NULL);
        } else {
                aprsmsg->timestamp = *fap->timestamp;
        }

        strcpy(aprsmsg->message, fap->message);
        strncpy(aprsmsg->srccall, fap->src_callsign, MAX_CALLSIGN);
        strncpy(aprsmsg->dstcall, fap->destination, MAX_CALLSIGN);

        printf("Message: storing this src call: %s, check: %s\n",
               aprsmsg->srccall, OBJNAME(fap) );

        /*
         * Handle message sent with an ID which is an ACK request
         */
        if(fap->message_id != NULL) {
                if( (aprsmsg->message_id=calloc(MAX_MSGID + 1, 1)) == NULL) {
                        printf("%s: malloc message id error: %s\n",
                               __FUNCTION__, strerror(errno));
                        return;
                }

                strncpy(aprsmsg->message_id, fap->message_id, MAX_MSGID);
                get_base_callsign(basecall, &ssid, aprsmsg->dstcall);
                printf("callsign check: %s %s\n", basecall, aprsmsg->dstcall);
                if(STREQ(basecall, state->basecall)) {
                        /* send an ack */
                        pr_debug("SEND an ack to %s\n", fap->src_callsign);
                        sprintf(aprs_ack,":%-9s:ack%s",
                                  aprsmsg->dstcall,
                                  aprsmsg->message_id);

                        send_message(state, aprs_ack);
                }
        } else {
                aprsmsg->message_id = NULL;
        }

        printf("save_message index: %d, from: %s, to: %s, ack: %s, id: %s, Msg: %s\n",
               msgind,
               fap->src_callsign, fap->destination,
               (fap->message_ack == NULL) ? "None" : fap->message_ack,
               (fap->message_id == NULL) ? "None" : fap->message_id,
               fap->message);


        state->msghist_idx = (state->msghist_idx == KEEP_MESSAGES - 1) ? 0 : ++msgind;
        assert(state->msghist_idx < KEEP_MESSAGES && state->msghist_idx >= 0);

}

void webdisplay_message(struct state *state, fap_packet_t *fap)
{
        char buf[512];

        printf("webdisplay_message: from: %s, to: %s, ack: %s, nack: %s, id: %s, Msg: %s\n",
               fap->src_callsign, fap->destination,
               (fap->message_ack == NULL) ? "None" : fap->message_ack,
               (fap->message_nack == NULL) ? "None" : fap->message_nack,
               (fap->message_id == NULL) ? "None" : fap->message_id,
               fap->message);

        snprintf(buf, sizeof(buf), "%s->%s@%s:%s",
                 fap->src_callsign,
                 fap->destination,
                 format_msgtime(fap->timestamp),
                 fap->message);

        _ui_send(state, "MS_MESSAGE", buf);
}

/*
 * Copy a string containing a callsign to a destination buffer.
 */
int parse_callsign(char *pSign, char *callBuf)
{
        char *inBuf=pSign;
        int signLen=0;

        while( (signLen < 10) && (isalnum(*inBuf) || *inBuf == '-') ) {
                callBuf[signLen] = *inBuf;
                signLen++;
                inBuf++;
        }
        callBuf[signLen] = '\0';
        return( strlen(callBuf) );
}

bool isEncap(fap_packet_t *fap, char **encap) {

        char *encap_start;

        /* Check for a third party packet */
        if( ((encap_start = strchr(fap->orig_packet, ':')) != NULL)
            && *(encap_start + 1) == '}' ) {
                *encap = encap_start + 2;
                return true;
        }

        return false;
}

void handle_aprsMessages(struct state *state, fap_packet_t *fap, char *packet) {

        printf("\n%s Msg [%lu]: type: 0x%02x -> %s\n",
               time2str(fap->timestamp, 0),
               state->stats.inPktCount,
               *fap->type, packet);
}

void handle_thirdparty(struct state *state, fap_packet_t *fap) {
        printf("%s: handle encap\n", time2str(fap->timestamp, 0));
}

/*
 * Display third party packets
 */
void webdisplay_thirdparty(struct state *state, fap_packet_t *fap)
{
        char buf[512];
        char fapbuf[512];
        char tmpbuf[16];
        char *nextchar = NULL;
        char *dst_callsign;
        fap_packet_t *encap_fap = NULL;

        if(isEncap(fap, &nextchar)) {
                strcpy(fapbuf, nextchar);
        } else {
                printf("%s: Failed to verify encapsultated packet %s\n",
                       __FUNCTION__, fap->orig_packet);
                display_fap_pkt("third party verify failed", fap);
                return;
        }

        /* Validated a 3rd party message packet, now parse it */
        encap_fap=dan_parseaprs(fapbuf, strlen(fapbuf), 0);

        if(encap_fap == NULL) {
                printf("%s: Failed to parse encapsultated packet %s\n",
                       __FUNCTION__, fap->orig_packet);
                display_fap_pkt("third party parse failed", fap);
                return;
        }

        if (encap_fap->error_code != NULL) {
                display_fap_error("Third Party", state, encap_fap);
                fap_free_wrapper("Third party", state, encap_fap);
                return;
        }

        display_fap_pkt("third party parse ok", encap_fap);

        state->stats.encapCount++;

        /* Check for a third party packet that's a message */
        if(encap_fap->message) {
                display_fap_message(state, encap_fap);   /* DEBUG only - look at message packets */
                if(isnewmessage(state, encap_fap)) {
                        save_message(state, encap_fap);
                        webdisplay_message(state, encap_fap);
                }
        } else {

                if(encap_fap->src_callsign == NULL) {
                        asprintf(&encap_fap->src_callsign, "NULL");
                }

                dst_callsign = encap_fap->destination;
                if(encap_fap->destination == NULL) {
                        if(encap_fap->dst_callsign != NULL) {
                                dst_callsign = encap_fap->dst_callsign;
                        } else if( (nextchar=strchr(encap_fap->orig_packet, '>')) != NULL) {
                                if(parse_callsign(nextchar+1, tmpbuf) > 0) {
                                        dst_callsign = tmpbuf;
                                }
                        }  else {
                                asprintf(&dst_callsign, "NULL");
                        }
                }

                if(encap_fap->orig_packet == NULL) {
                        asprintf(&encap_fap->orig_packet, "NULL");
                }

#if 0
                store_packet(state, fap);
#endif

                snprintf(buf, sizeof(buf), "%s->%s @%s: type: %d %s",
                         encap_fap->src_callsign,
                         dst_callsign,
                         format_msgtime(encap_fap->timestamp),
                         (encap_fap->type == NULL) ? -1 : *encap_fap->type,
                         encap_fap->orig_packet);


                printf("Processed thirdparty: %s\n", buf);fflush(stdout);

                _ui_send(state, "DB_PACKET", buf);
                fap_free_wrapper("Third Party", state, encap_fap);
        }
}

void webdisplay_unhandled(struct state *state, fap_packet_t *fap)
{
        char buf[512];

        printf("%s: Call to: %s, Msg to: %s, pkt: %s\n",
               __FUNCTION__, fap->dst_callsign, fap->destination, fap->orig_packet);

        snprintf(buf, sizeof(buf), "%s->%s@%s: type: %02x %s",
                 fap->src_callsign,
                 fap->destination,
                 format_msgtime(fap->timestamp),
                 *fap->type,
                 fap->orig_packet);
        _ui_send(state, "DB_PACKET", buf);
}

/*
 * Send APRS message packet out one of the interfaces:
 * serial, ax.25 or network stack
 */
int send_message(struct state *state, char *msg)
{

        if (STREQ(state->conf.tnc_type, "KISS")) {
                _ui_send(state, "I_TX", "1000");
                return send_kiss_beacon(state->tncfd, msg);
        } else if (STREQ(state->conf.tnc_type, "AX25")) {
#ifdef HAVE_AX25_TRUE
                _ui_send(state, "I_TX", "1000");
                return send_ax25_beacon(state, msg);
#else
                printf("%s:%s(): tnc type has been configured to use AX.25 but AX.25 lib has not been installed.\n",
                       __FILE__, __FUNCTION__);
                return 0;
#endif /* #ifdef HAVE_AX25_TRUE */

        } else {
                return send_net_beacon(state->tncfd, msg);
        }
}

/*
 * Debug only - look at message packets
 */
void display_fap_message(struct state *state, fap_packet_t *fap)
{
        state->stats.inMsgCount++;

        printf("MESSAGE[%lu]: ", state->stats.inMsgCount);
        if(fap->messaging) {
                printf("msg capab: %s ", (*fap->messaging == 1) ? "yes" : "no");
        }
        if(fap->destination) {
                printf("dest: %s ", fap->destination);
        }
        if(fap->message) {
                printf("msg[%zu]: %s ", strlen(fap->message), fap->message);
        }
        if(fap->message_ack) {
                printf("ack: %s ", fap->message_ack);
        }
        if(fap->message_nack) {
                printf("nack: %s ", fap->message_nack);
        }
        if(fap->message_id) {
                printf("id: %s", fap->message_id);
        }
        printf("\n");
        fflush(stdout);
}\

#endif /* WEBAPP */
