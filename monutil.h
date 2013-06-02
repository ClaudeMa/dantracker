/*
 * $Id: monutil.h  401 2013-05-14 22:02:10Z basil@pacabunga.com $
 */

#ifndef _MONUTIL_H
#define _MONUTIL_H	1

char* pid2str(int pid);
char* ord2frametypestring(int ordtype);
int frametype2ord(int type);

int  get16(unsigned char *);
int  get32(unsigned char *);
char *servname(int , char*);


char* ax25call2bufEnd(struct t_ax25call *, char *);
char* ax25call2strBuf(struct t_ax25call *, char *);
char* ax25call2str(struct t_ax25call *);

ssize_t readfull( int , void* , size_t);

void convertBufferToReadable( char *str, int size );

#endif  /* _MONUTIL_H */
