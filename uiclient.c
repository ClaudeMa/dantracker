/* -*- Mode: C; tab-width: 8;  indent-tabs-mode: nil; c-basic-offset: 8; c-brace-offset: -8; c-argdecl-indent: 8 -*- */
/* Copyright 2012 Dan Smith <dsmith@danplanet.com> */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "ui.h"

int ui_connect(struct sockaddr *dest, unsigned int dest_len)
{
	int sock;

	sock = socket(dest->sa_family, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		return -errno;
	}

	if (connect(sock, dest, dest_len)) {
		perror("connect");
		close(sock);
		return -errno;
	}

	return sock;
}

int ui_send(int sock, const char *name, const char *value)
{
	int ret;
	int len;
	int offset;
	struct ui_msg *msg;

	len = sizeof(*msg) + strlen(name) + strlen(value) + 2;
	msg = malloc(len);
	if (!msg)
		return -ENOMEM;

	msg->type = MSG_SETVALUE;
	msg->length = len;
	msg->name_value.name_len = strlen(name) + 1;
	msg->name_value.valu_len = strlen(value) + 1;

	offset = sizeof(*msg);
	memcpy((char*)msg + offset, name, msg->name_value.name_len);

	offset += msg->name_value.name_len;
	memcpy((char*)msg + offset, value, msg->name_value.valu_len);

	ret = send(sock, msg, len, MSG_NOSIGNAL);

	free(msg);
	return ret;
}

int ui_send_to(struct sockaddr *dest, unsigned int dest_len,
	       const char *name, const char *value)
{
	int sock;
	int ret;

	if (strlen(name) == 0)
		return -EINVAL;

	sock = ui_connect(dest, dest_len);
	if (sock < 0)
		return sock;

	ret = ui_send(sock, name, value);

	close(sock);

	return ret;
}

int ui_get_msg(int sock, struct ui_msg **msg)
{
	struct ui_msg hdr;
	int ret;

	ret = read(sock, &hdr, sizeof(hdr));
	if (ret <= 0)
		return ret;

	*msg = malloc(hdr.length);
	if (!*msg)
		return -ENOMEM;

	memcpy(*msg, &hdr, sizeof(hdr));

	ret = read(sock, ((char *)*msg)+sizeof(hdr), hdr.length - sizeof(hdr));

	return 1;
}

char *ui_get_msg_name(struct ui_msg *msg)
{
	if (msg->type != MSG_SETVALUE)
		return NULL;

	return (char*)msg + sizeof(*msg);
}

void filter_to_ascii(char *string)
{
	int i;

	for (i = 0; string[i]; i++) {
		if ((string[i] < 0x20 || string[i] > 0x7E) &&
		    (string[i] != '\r' && string[i] != '\n'))
			string[i] = ' ';
	}
}

char *ui_get_msg_valu(struct ui_msg *msg)
{
	if (msg->type != MSG_SETVALUE)
		return NULL;

	filter_to_ascii((char *)msg + sizeof(*msg) + msg->name_value.name_len);

	return (char*)msg + sizeof(*msg) + msg->name_value.name_len;
}

#ifdef MAIN
#include <getopt.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>

#include "util.h"

struct opts {
	int addr_family;
	char name[64];
	char value[64];
};

int parse_opts(int argc, char **argv, struct opts *opts)
{
	int retval = 1;

	static struct option lopts[] = {
		{"window",    0, 0, 'w'},
		{"inet",      0, 0, 'i'},
		{NULL,        0, 0,  0 },
	};

	memset(opts, 0, sizeof(opts));

	/* Set default options */
	opts->addr_family = AF_INET;
	strcpy(opts->name, "AI_CALLSIGN");
	strcpy(opts->value, "DEFAULT");

	while (1) {
		int c;
		int optidx;

		c = getopt_long(argc, argv, "wi", lopts, &optidx);
		if (c == -1)
			break;

		switch(c) {
			case 'w':
				opts->addr_family = AF_UNIX;
				break;
			case 'i':
				opts->addr_family = AF_INET;
		}
	}

	/*
	 * Set positional args
	 */
	/* get the NAME */
	if(optind < argc) {
		strcpy(opts->name, argv[optind]);
		strupper(opts->name);
		optind++;
	}

	/* get the VALUE */
	if(optind < argc) {
		strcpy(opts->value, argv[optind]);
		optind++;
	}

	if(argc == 1)
		retval = 0;
	else if(argc < 4)
		printf("Using defaults\n");

	return (retval);
}

int ui_send_default(struct opts *opts)
{

	if(opts->addr_family == AF_INET) {
		printf("using AF_INET\n");
		char hostname[]="127.0.0.1";
		struct sockaddr_in sin;
		struct hostent *host;

		sin.sin_family = AF_INET;
		sin.sin_port = htons(SOCKPORT);

		host = gethostbyname(hostname);
		if (!host) {
			perror(hostname);
			return -errno;
		}

		if (host->h_length < 1) {
			fprintf(stderr, "No address for %s\n", hostname);
			return -EINVAL;
		}
		memcpy(&sin.sin_addr, host->h_addr_list[0], sizeof(sin.sin_addr));

		return ui_send_to((struct sockaddr *)&sin, sizeof(sin),
				  opts->name, opts->value);
	} else {
		printf("using AF_UNIX\n");
		struct sockaddr_un sun;
		sun.sun_family = AF_UNIX;
		strcpy(sun.sun_path, SOCKPATH);
		return ui_send_to((struct sockaddr *)&sun, sizeof(sun),
				  opts->name, opts->value);
	}
}

int main(int argc, char **argv)
{
	struct opts opts;

	if(!parse_opts(argc, argv, &opts)) {
		printf("Usage: %s -<i><w> [NAME] [VALUE]\n", argv[0]);
		return 1;
	}
	printf("%d, %s %s\n", opts.addr_family, opts.name, opts.value);

	return ui_send_default(&opts);
}
#endif
