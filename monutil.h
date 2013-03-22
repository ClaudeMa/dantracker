/*
 * $Id: monutil.h 260 2012-12-26 18:35:28Z basil@pacabunga.com $
 */

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


