/* ax25dump.c
 * AX25 header tracing
 * Copyright 1991 Phil Karn, KA9Q
 * Extended for listen() by ???
 * 07.02.99 Changed for monixd by dg9ep
 *
 * $Id: ax25dump.c 326 2013-03-20 16:24:27Z basil@pacabunga.com $
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>

#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>

#define MAIN
#include "aprs.h"
#include "monixd.h"
#include "util.h"

#define LAPB_UNKNOWN	0
#define LAPB_COMMAND	1
#define LAPB_RESPONSE	2

#define SEG_FIRST	0x80
#define SEG_REM 	0x7F
#define SIZE_DISPLAY_BUF (3*1024)

/* unsigned char typeconv[14] =
 * { S, RR, RNR, REJ, U,
 *  SABM, SABME, DISC, DM, UA,
 *  FRMR, UI, PF, EPF
 * };
 */


#define MMASK		7

#define HDLCAEB 	0x01
#define SSID		0x1E
#define REPEATED	0x80
#define C		0x80
#define SSSID_SPARE	0x40
#define ESSID_SPARE	0x20

#define ALEN		6
#define AXLEN		7

#define W		1
#define X		2
#define Y		4
#define Z		8

static int  ftype(unsigned char *, int *, int *, int *, int *, int);
static char *decode_type(int);
int mem2ax25call(struct t_ax25call*, unsigned char*);
void data_dump(char *, unsigned char *data, int length, int dumpstyle);
void ai_dump(char *,unsigned char *data, int length);

#define NDAMA_STRING ""
#define DAMA_STRING "[DAMA] "

/*----------------------------------------------------------------------*/
char* ax25call2bufEnd(struct t_ax25call *pax25call, char *buf)
/* same as ax25call2str(), but caller have to provide own buffer
 * returns pointer to END of used buffer */
{
	if( pax25call->ssid == 0)
		return( buf+sprintf(buf,"%s",   pax25call->sCall                 ) );
	else
		return( buf+sprintf(buf,"%s-%d",pax25call->sCall, pax25call->ssid) );
}

char* ax25call2str(struct t_ax25call *pax25call)
{
	static char res[9+1]; /* fixme */
	ax25call2bufEnd(pax25call,res);
	return res;
}

char* pid2str(int pid)
{
	static char str[20];
	switch( pid ) {
		case PID_NO_L3:    return "Text";
		case PID_VJIP06:   return "IP-VJ.6";
		case PID_VJIP07:   return "IP-VJ.7";
		case PID_SEGMENT:  return "Segment";
		case PID_IP:	   return "IP";
		case PID_ARP:	   return "ARP";
		case PID_NETROM:   return "NET/ROM";
		case PID_X25:	   return "X.25";
		case PID_TEXNET:   return "Texnet";
		case PID_FLEXNET:  return "Flexnet";
		case PID_PSATFT:   return "PacSat FT";
		case PID_PSATPB:   return "PacSat PB";
		case -1: /*no PID*/return "";
		default:	   sprintf(str,"pid=0x%x",pid);
				   return str;
	}
}

void mprintf(char *accumStr, int dtype, char *fmt, ...)
{
        va_list args;
        char *pStr;
        int ret;

        va_start(args, fmt);
        ret=vasprintf(&pStr, fmt, args);
        va_end(args);

        /* Check for sufficient memory from vasprintf call */
        if(ret != -1) {
                if( (strlen(pStr)+strlen(accumStr)) > SIZE_DISPLAY_BUF) {
                        printf("%s: Display buffer too small\n", __FUNCTION__);
                        exit (ENOMEM);
                }
                strcat(accumStr, pStr);
                free(pStr);
        }
}
#if 0 /* for reference only, should never be called */
char * lprintf(int dtype, char *fmt, ...)
{
        va_list args;
        char str[1024];

        va_start(args, fmt);
        vsnprintf(str, 1024, fmt, args);
        va_end(args);

        printf("%s", str);
        return(str);
}
#endif /* DEBUG */

/* FlexNet header compression display by Thomas Sailer sailer@ife.ee.ethz.ch */
/* Dump an AX.25 packet header */
struct t_ax25packet *
ax25_decode( struct t_ax25packet *ax25packet , unsigned char *data, int length)
{
	int ctlen;
	int eoa;

	ax25packet->pid = -1;

	ax25packet->totlength = length;

	/* check for FlexNet compressed header first; FlexNet header
	 * compressed packets are at least 8 bytes long  */
	if (length < 8) {
		/* Something wrong with the header */
		return ax25packet;
	}
	if (data[1] & HDLCAEB) {
		/* char tmp[15]; */
		/* this is a FlexNet compressed header	$TODO */
/*		 tmp[6] = tmp[7] = ax25packet->fExtseq = 0;
 *		 tmp[0] = ' ' + (data[2] >> 2);
 *		 tmp[1] = ' ' + ((data[2] << 4) & 0x30) + (data[3] >> 4);
 *		 tmp[2] = ' ' + ((data[3] << 2) & 0x3c) + (data[4] >> 6);
 *		 tmp[3] = ' ' + (data[4] & 0x3f);
 *		 tmp[4] = ' ' + (data[5] >> 2);
 *		 tmp[5] = ' ' + ((data[5] << 4) & 0x30) + (data[6] >> 4);
 *		 if (data[6] & 0xf)
 *			 sprintf(tmp+7, "-%d", data[6] & 0xf);
 *		 lprintf(T_ADDR, "%d->%s%s",
 *			 (data[0] << 6) | ((data[1] >> 2) & 0x3f),
 *			 strtok(tmp, " "), tmp+7);
 *		 ax25packet->cmdrsp = (data[1] & 2) ? LAPB_COMMAND : LAPB_RESPONSE;
 *		 dama = NDAMA_STRING;
 *		 data	+= 7;
 *		 length -= 7;
 */
		 return ax25packet; /* $TODO */
	} else {
		/* Extract the address header */
		if (length < (AXLEN + AXLEN + 1)) {
			/* length less then fmcall+tocall+ctlbyte */
			/* Something wrong with the header */
			return ax25packet;
		}

		ax25packet->fExtseq = (
			 (data[AXLEN + ALEN] & SSSID_SPARE) != SSSID_SPARE
		);

		ax25packet->fDama = ((data[AXLEN + ALEN] & ESSID_SPARE) != ESSID_SPARE);

		mem2ax25call( &ax25packet->tocall, data 	);
		mem2ax25call( &ax25packet->fmcall, data + AXLEN );

		ax25packet->cmdrsp = LAPB_UNKNOWN;
		if ((data[ALEN] & C) && !(data[AXLEN + ALEN] & C))
			ax25packet->cmdrsp = LAPB_COMMAND;

		if ((data[AXLEN + ALEN] & C) && !(data[ALEN] & C))
			ax25packet->cmdrsp = LAPB_RESPONSE;

		eoa = (data[AXLEN + ALEN] & HDLCAEB);

		data   += (AXLEN + AXLEN);
		length -= (AXLEN + AXLEN);

		ax25packet->ndigi = 0;
                ax25packet->ndigirepeated = -1;

                while (!eoa && (ax25packet->ndigi < MAX_AX25_DIGIS + 1) && (length > 0) ) {
                        mem2ax25call( &ax25packet->digi[ax25packet->ndigi], data );
                        if( data[ALEN] & REPEATED )
                                ax25packet->ndigirepeated = ax25packet->ndigi;

                        eoa = (data[ALEN] & HDLCAEB);
                        ax25packet->ndigi++;
                        data   += AXLEN;
                        length -= AXLEN;
                }
        }

	ax25packet->fFmCallIsLower = (
	    memcmp( &ax25packet->fmcall,
		    &ax25packet->tocall, sizeof(struct t_ax25call)
		  ) < 0
	);

	ax25packet->pdata = (char *)data;
	ax25packet->datalength = length;

	if (length == 0) {
	    ax25packet->valid = 1;
	    return ax25packet;
	}

	ctlen = ftype(data, &ax25packet->frametype, &ax25packet->ns, &ax25packet->nr, &ax25packet->pf, ax25packet->fExtseq);

	data   += ctlen;
	length -= ctlen;

	ax25packet->pdata = (char *)data;
	ax25packet->datalength = length;

	if ( ax25packet->frametype == I || ax25packet->frametype == UI ) {
		/* Decode I field */
		if ( length > 0 ) {	   /* Get pid */
			ax25packet->pid = *data++;
			length--;

/*  $TODO		if (ax25packet->pid == PID_SEGMENT) {
 *				 seg = *data++;
 *				 length--;
 *				 lprintf(T_AXHDR, "%s remain %u", seg & SEG_FIRST ? " First seg;" : "", seg & SEG_REM);
 *
 *				 if (seg & SEG_FIRST) {
 *					 ax25packet->pid = *data++;
 *					 length--;
 *				 }
 *			 }
 */
			ax25packet->pdata = (char *)data;
			ax25packet->datalength = length;
			ax25packet->datacrc = calc_abincrc((char *)data, length, 0x5aa5 );
		}
	} else if (ax25packet->frametype == FRMR && length >= 3) {
	/* FIX ME XXX
		lprintf(T_AXHDR, ": %s", decode_type(ftype(data[0])));
	*/
/*		  lprintf(T_AXHDR, ": %02X", data[0]);
 *  $TODO	 lprintf(T_AXHDR, " Vr = %d Vs = %d",
 *			 (data[1] >> 5) & MMASK,
 *			 (data[1] >> 1) & MMASK);
 *		 if(data[2] & W)
 *			 lprintf(T_ERROR, " Invalid control field");
 *		 if(data[2] & X)
 *			 lprintf(T_ERROR, " Illegal I-field");
 *		 if(data[2] & Y)
 *			 lprintf(T_ERROR, " Too-long I-field");
 *		 if(data[2] & Z)
 *			 lprintf(T_ERROR, " Invalid seq number");
 *		 lprintf(T_AXHDR,"%s\n", dama);
 */
	 } else if ((ax25packet->frametype == SABM || ax25packet->frametype == UA) && length >= 2) {
		/* FlexNet transmits the QSO "handle" for header
		 * compression in SABM and UA frame data fields
		 */
/*		  lprintf(T_AXHDR," [%d]%s\n", (data[0] << 8) | data[1], dama);
 */
	}
	/* ok, all fine */
	ax25packet->valid = 1;
	return ax25packet;
}

char * ax25packet_dump(struct t_ax25packet *pax25packet, int dumpstyle)
{
        char *dama;
        char *dumpbuf;


        dumpbuf = malloc(SIZE_DISPLAY_BUF);
        dumpbuf[0]='\0';

        /* First field displayed is a time stamp */
        mprintf(dumpbuf, T_TIMESTAMP, "%s ", time2str(&pax25packet->time, 0) );


	/* if( pax25packet->pQSO )
	 *   lprintf(T_ADDR,  "(%d) ",  pax25packet->pQSO->qsoid);
	 */

        /* Second field displayed is port name */
        mprintf(dumpbuf, T_PORT, "%s ", pax25packet->port);

	/* Nobody will need this:
	 *   if( pax25packet->fcrc )	   lprintf(T_PORT, "CRC-");
	 *   if( pax25packet->fflexcrc )   lprintf(T_PORT, "flex:");
	 *   if( pax25packet->fsmack )	   lprintf(T_PORT, "smack:");
	 */
	/* ------------------------------------- */

	if( !pax25packet->valid ) {
		/* Something wrong with the header */
                mprintf(dumpbuf, T_ERROR, "AX25: bad header! data length: %d\n", pax25packet->datalength);

		data_dump(dumpbuf, (unsigned char *)pax25packet->pdata, pax25packet->datalength, HEX);
		return dumpbuf;
	}
	/* ONLY looking at AX25 packets so display length instead */
#if 0
	if (pax25packet->fExtseq) {
		lprintf(T_PROTOCOL, "EAX25: ");
	} else {
		lprintf(T_PROTOCOL, "AX25: ");
	}
#else
        /* Display packet length */
        mprintf(dumpbuf, T_PROTOCOL, "%d - ", pax25packet->totlength);
#endif

	if(pax25packet->fDama) {
		dama = DAMA_STRING;
	} else {
		dama = NDAMA_STRING;
        }

        /* Start displaying actual packet */
        mprintf(dumpbuf, T_FMCALL, "%s", ax25call2str(&pax25packet->fmcall));
        mprintf(dumpbuf, T_ZIERRAT, "->");
        mprintf(dumpbuf, T_TOCALL, "%s", ax25call2str(&pax25packet->tocall));

	if( pax25packet->ndigi > 0 ) {
                int i;

                mprintf(dumpbuf, T_AXHDR, " v");
		for(i=0;i<pax25packet->ndigi;i++) {
                        mprintf(dumpbuf, T_VIACALL," %s%s", ax25call2str(&pax25packet->digi[i]),
                                  pax25packet->ndigirepeated == i ? "*" : "");
		}
        }
        /* Warning: using less than character, an HTML reserved
         * character */
        mprintf(dumpbuf, T_AXHDR, " <%s", decode_type(pax25packet->frametype));

	switch (pax25packet->cmdrsp) {
		case LAPB_COMMAND:
                        mprintf(dumpbuf, T_AXHDR, " C");
                        if( pax25packet->pf ) {
                                mprintf(dumpbuf, T_AXHDR, "+");
                        }
			break;
		case LAPB_RESPONSE:
                        mprintf(dumpbuf, T_AXHDR, " R");
                        if( pax25packet->pf ) {
                                mprintf(dumpbuf, T_AXHDR, "-");
                        }
			break;
		default:
			break;
	}
	if (pax25packet->ns > -1 )
                mprintf(dumpbuf, T_AXHDR, " S%d", pax25packet->ns);
	if (pax25packet->nr > -1 )
                mprintf(dumpbuf, T_AXHDR, " R%d", pax25packet->nr);
        /* Warning: using greater than character, an HTML reserved
         * character */
        mprintf(dumpbuf, T_AXHDR, ">");

	if (pax25packet->frametype == I || pax25packet->frametype == UI) {
		/* Decode I field */
		/* Took out \n after %s%s for a one line display */
                mprintf(dumpbuf, T_PROTOCOL," %s%s:",dama, pid2str(pax25packet->pid));

		switch (pax25packet->pid) {
			case PID_NO_L3: /* the most likly case */
				ai_dump(dumpbuf, (unsigned char *)pax25packet->pdata, pax25packet->datalength);
				break;
			case PID_VJIP06:
			case PID_VJIP07:
			case PID_SEGMENT:
				data_dump(dumpbuf, (unsigned char *)pax25packet->pdata, pax25packet->datalength, dumpstyle);
				break;
#if 0
			case PID_IP:
				ip_dump(pax25packet->pdata, pax25packet->datalength, dumpstyle);
				break;
			case PID_ARP:
				arp_dump(pax25packet->pdata, pax25packet->datalength);
				break;
			case PID_NETROM:
				netrom_dump(pax25packet->pdata, pax25packet->datalength, dumpstyle);
				break;
			case PID_X25:
				rose_dump(pax25packet->pdata, pax25packet->datalength, dumpstyle);
				break;
			case PID_TEXNET:
				data_dump(pax25packet->pdata, pax25packet->datalength, dumpstyle);
				break;
			case PID_FLEXNET:
				flexnet_dump(pax25packet->pdata, pax25packet->datalength, dumpstyle);
				break;
#else
			case PID_IP:
			case PID_ARP:
			case PID_NETROM:
			case PID_X25:
			case PID_TEXNET:
			case PID_FLEXNET:
                                printf("Unhandled PID found 0x%02x\n", pax25packet->pid);
#endif
                        default:
				ai_dump(dumpbuf, (unsigned char *)pax25packet->pdata, pax25packet->datalength);
				break;
		}
	} else if (pax25packet->frametype == FRMR && pax25packet->datalength >= 3) {
	/* FIX ME XXX
		lprintf(T_AXHDR, ": %s", decode_type(ftype(data[0])));
	*/
                mprintf(dumpbuf, T_AXHDR, ": %02X", pax25packet->pdata);
                mprintf(dumpbuf, T_AXHDR, " Vr = %d Vs = %d",
			(pax25packet->pdata[1] >> 5) & MMASK,
			(pax25packet->pdata[1] >> 1) & MMASK);
		if(pax25packet->pdata[2] & W)
                        mprintf(dumpbuf, T_ERROR, " Invalid control field");
		if(pax25packet->pdata[2] & X)
                        mprintf(dumpbuf, T_ERROR, " Illegal I-field");
		if(pax25packet->pdata[2] & Y)
                        mprintf(dumpbuf, T_ERROR, " Too-long I-field");
		if(pax25packet->pdata[2] & Z)
                        mprintf(dumpbuf, T_ERROR, " Invalid seq number");

                mprintf(dumpbuf, T_AXHDR,"%s\n", dama);
	} else if ((pax25packet->frametype == SABM || pax25packet->frametype == UA) && pax25packet->datalength >= 2) {
		/* FlexNet transmits the QSO "handle" for header
		 * compression in SABM and UA frame data fields
		 */
		/* lprintf(T_AXHDR," [%d]%s\n", (pax25packet->pdata << 8) | pax25packet->pdata[1], dama); */
	} else {
                mprintf(dumpbuf, T_AXHDR,"%s\n", dama);
        }
        return(dumpbuf);
}



static char * decode_type(int type)
{
	switch (type) {
		case I:     return "I";
		case SABM:  return "C";
		case SABME: return "CE";
		case DISC:  return "D";
		case DM:    return "DM";
		case UA:    return "UA";
		case RR:    return "RR";
		case RNR:   return "RNR";
		case REJ:   return "REJ";
		case FRMR:  return "FRMR";
		case UI:    return "UI";
		default:    return "[invalid]";
	}
}

int mem2ax25call(struct t_ax25call *presbuf, unsigned char *data)
{
	int i;
	char *s;
	char c;

	s = presbuf->sCall;
	memset(s,0,sizeof(*presbuf));
	for (i = 0; i < ALEN; i++) {
		c = (data[i] >> 1) & 0x7F;
		if (c != ' ') *s++ = c;
	}
	/* *s = '\0'; */
	/* decode ssid */
	presbuf->ssid = (data[ALEN] & SSID) >> 1;
	return 0;
}



char * pax25(char *buf, unsigned char *data)
/* shift-ASCII-Call to ASCII-Call */
{
	int i, ssid;
	char *s;
	char c;

	s = buf;

	for (i = 0; i < ALEN; i++) {
		c = (data[i] >> 1) & 0x7F;

		if (!isalnum(c) && c != ' ') {
			strcpy(buf, "[invalid]");
			return buf;
		}

		if (c != ' ') *s++ = c;
	}
	/* decode ssid */
	if ((ssid = (data[ALEN] & SSID)) != 0)
		sprintf(s, "-%d", ssid >> 1);
	else
		*s = '\0';

	return(buf);
}


static int ftype(unsigned char *data, int *type, int *ns, int *nr, int *pf, int extseq)
/* returns then length of the controlfield, this could be > 1 ... */
{
	*nr = *ns = -1; /* defaults */
	if (extseq) {
		if ((*data & 0x01) == 0) {	/* An I frame is an I-frame ... */
			*type = I;
			*ns   = (*data >> 1) & 127;
			data++;
			*nr   = (*data >> 1) & 127;
			*pf   = *data & EPF;
			return 2;
		}
		if (*data & 0x02) {
			*type = *data & ~PF;
			*pf   = *data & PF;
			return 1;
		} else {
			*type = *data;
			data++;
			*nr   = (*data >> 1) & 127;
			*pf   = *data & EPF;
			return 2;
		}
	} else { /* non extended */
		if ((*data & 0x01) == 0) {	/* An I frame is an I-frame ... */
			*type = I;
			*ns   = (*data >> 1) & 7;
			*nr   = (*data >> 5) & 7;
			*pf   = *data & PF;
			return 1;
		}
		if (*data & 0x02) {		/* U-frames use all except P/F bit for type */
			*type = *data & ~PF;
			*pf   = *data & PF;
			return 1;
		} else {			/* S-frames use low order 4 bits for type */
			*type = *data & 0x0F;
			*nr   = (*data >> 5) & 7;
			*pf   = *data & PF;
			return 1;
		}
	}
}


void ax25_dump(struct t_ax25packet *pax25packet, unsigned char *data, int length, int dumpstyle)
{
	ax25_decode( pax25packet, data, length );
#if 0
    /* look for matching QSO */
    pax25packet->pQSO = doQSOMheard(pax25packet);
    tryspy(pax25packet);

    /* switch temp. all those clients off, which do not want to see supervisor
     * frames, if it is not an I-Frame. Yes. This is hacklike... */
    clientEnablement( ceONLYINFO, (pax25packet->pid > -1) );
    clientEnablement( ceONLYIP,   (pax25packet->pid == PID_IP) );
#endif
    ax25packet_dump( pax25packet, dumpstyle );
#if 0
    /* and switch all disabled clients on */
    clientEnablement( ceONLYIP,   1 );
    clientEnablement( ceONLYINFO, 1 );
#endif
}

void ascii_dump(char *dispbuf, unsigned char *data, int length)
/* Example:  "0000 Dies ist ein Test.."
 */
{
        unsigned char c;
	int  i, j;
	char buf[100];

	for (i = 0; length > 0; i += 64) {
		sprintf(buf, "%04X  ", i);

		for (j = 0; j < 64 && length > 0; j++) {
			c = *data++;
			length--;

			if ((c != '\0') && (c != '\n'))
				strncat(buf,(char *)&c, 1);
			else
				strcat(buf, ".");
		}

                mprintf(dispbuf, T_DATA, "%s\n", buf);
	}
}

void readable_dump(char *dispbuf, unsigned char *data, int length)
/* Example "Dies ist ein Test.
 *	   "
 */
{
	unsigned char c;
	int  i;
	int  cr = 1;
	char buf[BUFSIZE];

	for (i = 0; length > 0; i++) {
		c = *data++;
		length--;
		switch (c) {
			case 0x00:
				buf[i] = ' ';
			case 0x0A: /* hum... */
			case 0x0D:
				if (cr) buf[i] = '\n';
				else i--;
				break;
			default:
				buf[i] = c;
		}
		cr = (buf[i] != '\n');
	}
	if (cr)
		buf[i++] = '\n';
	buf[i++] = '\0';
        mprintf(dispbuf, T_DATA, "%s", buf);
}

void hex_dump(char *dispbuf, unsigned char *data, int length)
/*  Example: "0000 44 69 65 ......               Dies ist ein Test."
 */
{
	int  i, j, length2;
	unsigned char c;
        char *data2;
	char buf[4], hexd[49], ascd[17];

	length2 = length;
	data2	= (char *)data;

	for (i = 0; length > 0; i += 16) {
		hexd[0] = '\0';
		for (j = 0; j < 16; j++) {
			c = *data2++;
			length2--;

			if (length2 >= 0)
				sprintf(buf, "%2.2X ", c);
			else
				strcpy(buf, "   ");
			strcat(hexd, buf);
		}

		ascd[0] = '\0';
		for (j = 0; j < 16 && length > 0; j++) {
			c = *data++;
			length--;

			sprintf(buf, "%c", ((c != '\0') && (c != '\n')) ? c : '.');
			strcat(ascd, buf);
		}

                mprintf(dispbuf, T_DATA, "%04X  %s | %s\n", i, hexd, ascd);
	}
}

void ai_dump(char *dispbuf, unsigned char *data, int length)
{
	int testsize, i;
	char c;
	char *p;
	int dumpstyle = READABLE;

	/* make a smart guess how to dump data */
	testsize = (10>length) ? length:10;
	p = (char *)data;
	for (i = testsize; i>0; i--) {
		c = *p++;
		if( iscntrl(c) && (!isspace(c)) ) {
			dumpstyle = ASCII; /* Hey! real smart! $TODO */
			break;
		}
	}
	/* anything else */
	data_dump(dispbuf, data, length, dumpstyle);
}

void data_dump(char *dispbuf, unsigned char *data, int length, int dumpstyle)
{
	switch (dumpstyle) {

		case READABLE:
			readable_dump(dispbuf, data, length);
			break;
		case HEX:
			hex_dump(dispbuf, data, length);
			break;
		default:
			ascii_dump(dispbuf, data, length);
	}
}

