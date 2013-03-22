/*
 * $Id: mondefs.h 259 2012-12-24 23:26:18Z basil@pacabunga.com $
 * Definitions of the server the client may need too
 */

#ifndef _MONDEFS_H
#define _MONDEFS_H	1

#include <config.h>
#include <time.h>

/* by request of Jochen :-) */
#include <sys/socket.h>
/*#include <linux/ax25.h>*/

#ifdef HAVE_NETAX25_AX25_H
#include <netax25/ax25.h>
#include <netrose/rose.h>
#include <netrom/netrom.h>
#else
#include <linux/ax25.h>
#include <linux/rose.h>
#include <linux/netrom.h>
#endif



#ifdef HAVE_NETAX25_AXLIB_H
   #include <netax25/axconfig.h>
   #include <netax25/axlib.h> 
#elif HAVE_AX25_AXUTILS_H
   #include <ax25/axconfig.h>
   #include <ax25/axutils.h>
#else
   #include <axconfig.h>
   #include <axutils.h>
#endif


/* Protversion changes if protelements are changed without compatibility */
#define PROTVERSION 2

/* TCP-Default-Port: 14090 = 10 * 1409 :-) */
#define MONIX_PORT  14090
/* these do not need commands */
#define MONIXPLAINTEXT_PORT  14091
#define MONIXPLAINBUF_PORT   14092

/*----------------------------------------------------------------------*/
/* From MonixD to client */
#define T_ERROR 	1

#define T_PORT		2

#define T_KISS		3
#define T_BPQ		4
#define T_DATA		5
#define T_PROTOCOL	6
#define T_AXHDR 	7
#define T_ADDR		8
#define T_IPHDR 	9
#define T_TCPHDR	10
#define T_ROSEHDR	11
#define T_TIMESTAMP	12
#define T_FLEXNET	13

#define T_ZIERRAT	14
/* Kindof T_ADDR */
#define T_CALL		15
/* Kindof T_CALL */
#define T_FMCALL	16
#define T_TOCALL	17
#define T_VIACALL	18

#define T_HDRVAL	19

#define T_MHEARD	   118
#define T_MHEARDSTRUCT	   119
#define T_CALLMHEARDSTRUCT 121
#define T_QSOMHEARDSTRUCT  122

#define T_DXCLUST	   120

/* see file programming */
#define T_SPYDATA	   123
#define T_SPYHEADER	   124
#define T_SPYEND	   125

#define T_RAWPACKET	   126

#define T_OFFERQSO	   127
#define T_QSOOFFER	   T_OFFERQSO

#define T_VERSION	   128
#define T_PROTVERSION	   129

/* End of data flag for variable count of data, see sendCallMHeard() */
#define T_ENDOFDATA	   200

/*----------------------------------------------------------------------*/
/* From client to MonixD*/
#define TT_CMD_FOLLOWS 001 /* Data contains Commandlline */

/*-----------------------------------------------------*/
#define U		0x03
#define S		0x01
#define PF		0x10
#define EPF		0x01
/* see .frametype */
#define I		0x00
#define FRMR		0x87
#define UI		0x03

#define RR		0x01
#define RNR		0x05
#define REJ		0x09

#define SABM		0x2F
#define SABME		0x6F
#define DISC		0x43
#define DM		0x0F
#define UA		0x63


#define PID_VJIP06	0x06
#define PID_VJIP07	0x07
#define PID_SEGMENT	0x08
#define PID_ARP 	0xCD
#define PID_NETROM	0xCF
#define PID_IP		0xCC
#define PID_X25 	0x01
#define PID_TEXNET	0xC3
#define PID_FLEXNET	0xCE
#define PID_PSATFT	0xBB
#define PID_PSATPB	0xBD
#define PID_NO_L3	0xF0
#define PID_TEXT	PID_NO_L3

#define nORD_FRAMETYP 12
/* Bestimmt u.a. auch die Ausgabereihenfolge */
#define ORD_OTHER   0
#define ORD_I	    1
#define ORD_UI	    2
#define ORD_SABM    3
#define ORD_SABME   4
#define ORD_UA	    5
#define ORD_DISC    6
#define ORD_DM	    7
#define ORD_FRMR    8
#define ORD_RR	    9
#define ORD_RNR    10
#define ORD_REJ    11

/*-----------------------------------------------------*/

typedef int t_dtype;
typedef int t_dsize;

struct t_protHeader {
   int version;
   t_dtype dtype;
   t_dsize dsize;
};

/* ---------------------------------- */

struct t_ax25call {
    char sCall[6+1];
    int  ssid;
};

/*----------------------------*/

struct t_mheard {
    int version;
    time_t starttime;
    int pid[256];
    int frametype[nORD_FRAMETYP];
    int damaframes;
    int nTotalBytes; /* sum of all ax25.bytes */
    int nInfoBytes; /* sum of all ax25. payloads monitored, inclusive retransmissions */
    int protTCP, protUDP, protICMP;
    int nDXClust;
};

/*---------------------*/

struct t_CallMheard {
  int	 version;
  struct t_CallMheard *pNext;
  struct t_ax25call call;
  time_t firstheard;
  time_t lastheard;
  int	 nFrames;
  int	 nDirectFrames;
  int	 nPID[256];
  int	 frametype[nORD_FRAMETYP];
  int	 damaframes;
  int	 nTotalBytes; /* sum of all ax25.bytes */
  int	 nInfoBytes; /* sum of all ax25. payloads monitored, inclusive retransmissions */
  int	 protTCP, protUDP, protICMP;
  int	 nDXClust;
};

/*---------------------*/

#define MAXCLIENTPERQSO 50
typedef int t_qsoid;
/* its easier: */
#define t_qso t_QSOMheard

struct t_QSOMheard {
  int		     version;
  struct t_qso *pNext;	   /* to be ignored by the client */
  t_qsoid	     qsoid;
  struct t_ax25call  fmcall,
		     tocall;
  /* struct t_ax25call digi[8];
   * unsigned int ndigi; */ /* =-1 unknown Digicount; 0=no digi */
  /* unsigned int ndigirepeated; */ /* digi[ndigirepeated] has repeated, -1==none */
  int	 pid;	 /* -1 if not detected yet */
  /* -- */
  t_qsoid	     otherqsoid;
  struct t_qso *pOtherqso;  /* to be ignored by the client */
  /* Stats */
  time_t	     firstheard,
		     lastheard;
  int		     nFrames;
  int		     frametype[nORD_FRAMETYP];
  int		     nTotalBytes; /* sum of all ax25.bytes */
  int		     nInfoBytes;  /* sum of all ax25. payloads monitored, inclusive retransmissions */
  /* Spy */
  int		     spyclient[MAXCLIENTPERQSO]; /* Which client does a spy on this QSO? */ /* to be ignored by the client */
  int		     lastSpyedNumber; /* last ns transmitted to the clients */
  int		     crcPacketCollect[8]; /* to be ignored by the client */
  struct t_ax25packet *pPacketCollect[8]; /* to be ignored by the client */
  /* Misc */
  struct t_ax25packet* pFirstPacket; /* store for first the heard I-Packet (only if offered) */ /* to be ignored by the client */
};

/*------------------------------------------------------------*/
/* well, not really a ax25packet... */
struct t_ax25packet {
    int     valid;
    /* Moni */
    time_t  time;
    char    *port;
    /* KISS */
    int     fsmack,
	    fflexcrc,
	    fcrc;
    /* AX25 */
    /* adress */
    struct t_ax25call fmcall;
    struct t_ax25call tocall;
    struct t_ax25call digi[8];
    unsigned int      ndigi; /* =-1 unknown Digicount; 0=no digi */
    unsigned int      ndigirepeated; /* digi[ndigirepeated] has repeated, -1==none */
    int 	      adresshash; /* not yet used: only from and to, including SSID */
    int 	      pathhash;   /* not yet used: from, to and digipeater */
    int 	      fFmCallIsLower; /* sort criteria */
    struct t_qso      *pQSO;  /* pointer to QSO */
    /* ctrlinfo */
    int  fDama;    /* DAMA - Bit set? */
    int  fExtseq;  /* extended Sequenzno. in use? */
    int  nr, ns;
    int  frametype; /* Frametype (I,C,... */
    int  cmdrsp;    /* command or respond */
    int  pf;	    /* normal or poll/final  */
    int  pid;	    /* pid; -1 if no pid availble */
    int  totlength;
    int  protType;   /* not yet used: type of  struct pProtData points to */
    void *pProtData; /* not yet used: Pointer to next decoded packet-struct */
    int  datalength;
    char *pdata;
    int  datacrc;    /* for framecollector */
};




#endif

