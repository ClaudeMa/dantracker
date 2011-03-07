#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <gtk/gtk.h>
#include <pango/pango.h>

#include "ui.h"

#define BG_COLOR "black"
#define FG_COLOR_TEXT "white"

enum {
	TYPE_TEXT_LABEL,
	TYPE_IMAGE,
	TYPE_MAX,
};

struct named_element {
	const char *name;
	unsigned int type;
	GtkWidget *widget;
	int (*update_fn)(struct named_element *e, const char *value);
};

struct layout {
	GtkWidget *window;
	GtkWidget *fixed;
	GtkWidget *sep1;
	GtkWidget *sep2;

	unsigned int max;
	unsigned int nxt;
	struct named_element *elements;
};

struct element_layout {
	const char *name;
	int x;
	int y;
};

GdkPixbuf *aprs_pri_img;

struct element_layout aprs_info_elements[] = {
	{"AI_ICON",     20, 0},
	{"AI_CALLSIGN", 90,  10},
	{"AI_COURSE",   30,  50},
	{"AI_COMMENT",  30,  80},
	{"AI_DISTANCE", 450, 15},
	{NULL, 0, 0}
};

struct element_layout aprs_list_elements[] = {
	{"AL_00", 30, 120},
	{"AL_01", 30, 150},
	{"AL_02", 30, 180},
	{"AL_03", 30, 210},

	{"AL_04", 370, 120},
	{"AL_05", 370, 150},
	{"AL_06", 370, 180},
	{"AL_07", 370, 210},
	{NULL, 0, 0}
};

struct element_layout gps_info_elements[] = {
	{"G_LATLON",  30, 250},
	{"G_ALT",    370, 250},

	{"G_SPD",     30, 280},
	{"G_STATUS", 370, 280},

	{"G_MYCALL",  30, 310},
	{"G_LASTBEACON",   250, 310},
	{"G_REASON",       400, 310},
	{"G_SIGBARS", 640, 310},
	{NULL, 0, 0}
};

struct named_element *get_element(struct layout *l, const char *name)
{
	int i;

	for (i = 0; i < l->nxt; i++)
		if (strcmp(l->elements[i].name, name) == 0)
			return &l->elements[i];
	return NULL;
}

int update_text_label(struct named_element *e, const char *value)
{
	gtk_label_set_markup(GTK_LABEL(e->widget), value);

	return 0;
}

int make_text_label(struct layout *l,
		    const char *name,
		    const char *initial,
		    const char *font)
{
	struct named_element *e = &l->elements[l->nxt++];
	GdkColor color;

	gdk_color_parse(FG_COLOR_TEXT, &color);

	if (l->nxt == l->max) {
		printf("Exceeded max elements\n");
		exit(1);
	}

	e->name = name;
	e->type = TYPE_TEXT_LABEL;
	e->widget = gtk_label_new(initial);
	e->update_fn = update_text_label;

	gtk_widget_show(e->widget);

	gtk_widget_modify_font(e->widget,
			       pango_font_description_from_string(font));
	gtk_widget_modify_fg(e->widget, GTK_STATE_NORMAL, &color);

	return 0;
}

int update_icon(struct named_element *e, const char *value)
{
	GdkPixbuf *icon;
	int index = value[1] - '!';
	int x = 63 * (index % 16);
	int y = 63 * (index / 16);

	icon = gdk_pixbuf_new_subpixbuf(aprs_pri_img,
					x, y, 60, 60);
	gtk_image_set_from_pixbuf(GTK_IMAGE(e->widget), icon);

	printf("Updated icon %i,%i for %s\n", x,y,value);

	return 0;
}

int make_icon(struct layout *l, const char *name)
{
	struct named_element *e = &l->elements[l->nxt++];

	e->name = name;
	e->type = TYPE_IMAGE;
	e->widget = gtk_image_new();
	e->update_fn = update_icon;

	gtk_widget_show(e->widget);
}

int update_bars(struct named_element *e, const char *value)
{
	GdkPixbuf *icon;
	int bars = atoi(value);
	char *path = NULL;

	if (asprintf(&path, "images/bars_%i.png", bars) == -1)
		return 0;

	icon = gdk_pixbuf_new_from_file(path, NULL);
	gtk_image_set_from_pixbuf(GTK_IMAGE(e->widget), icon);

	/* FIXME: Free icon? */

	printf("Updated bars to %i\n", bars);

	free(path);

	return 0;
}

int make_bars(struct layout *l, const char *name)
{
	struct named_element *e = &l->elements[l->nxt++];

	e->name = name;
	e->type = TYPE_IMAGE;
	e->widget = gtk_image_new();
	e->update_fn = update_bars;

	gtk_widget_show(e->widget);
}

int put_widgets(struct layout *l, struct element_layout *layouts)
{
	int i;

	for (i = 0; layouts[i].name; i++) {
		struct element_layout *el = &layouts[i];
		struct named_element *e = get_element(l, el->name);
		gtk_fixed_put(GTK_FIXED(l->fixed), e->widget, el->x, el->y);
	}

	return 0;
}

int make_aprs_info(struct layout *l)
{
	make_text_label(l, "AI_CALLSIGN", "N0CALL", "Monospace Bold 32");
	make_text_label(l, "AI_COURSE",   "", "Monospace 22");
	make_text_label(l, "AI_COMMENT",  "", "Sans 18");
	make_text_label(l, "AI_DISTANCE", "", "Monospace 22");
	make_icon(l, "AI_ICON");

	put_widgets(l, aprs_info_elements);

	return 0;
}

int make_aprs_list(struct layout *l)
{
	make_text_label(l, "AL_00",  "", "Monospace 20");
	make_text_label(l, "AL_01",  "", "Monospace 20");
	make_text_label(l, "AL_02",  "", "Monospace 20");
	make_text_label(l, "AL_03",  "", "Monospace 20");

	make_text_label(l, "AL_04",  "", "Monospace 20");
	make_text_label(l, "AL_05",  "", "Monospace 20");
	make_text_label(l, "AL_06",  "", "Monospace 20");
	make_text_label(l, "AL_07",  "", "Monospace 20");

	put_widgets(l, aprs_list_elements);

	return 0;
}

int make_gps_info(struct layout *l)
{
	make_text_label(l, "G_LATLON",  "", "Sans 18");
	make_text_label(l, "G_ALT",     "", "Sans 18");
	make_text_label(l, "G_SPD",     "", "Sans 20");
	make_text_label(l, "G_STATUS",  "", "Sans 20");

	make_text_label(l, "G_MYCALL",  "", "Sans 20");
	make_text_label(l, "G_LASTBEACON",  "", "Sans 20");
	make_text_label(l, "G_REASON",  "", "Sans 20");
	make_bars(l, "G_SIGBARS");

	put_widgets(l, gps_info_elements);
}

int make_window(struct layout *l)
{
	GdkColor color = {0, 0, 0};
	GdkCursor *cursor;
	GdkWindow *window;

	l->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(l->window), 720, 482);

	l->fixed = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(l->window), l->fixed);
	gtk_widget_show(l->fixed);

	gtk_widget_modify_bg(l->window, GTK_STATE_NORMAL, &color);
	gtk_window_maximize(GTK_WINDOW(l->window));

	gtk_widget_show(l->window);

	cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
	window = gtk_widget_get_window(GTK_WIDGET(l->window));
	gdk_window_set_cursor(window, cursor);

	make_aprs_info(l);
	make_aprs_list(l);
	make_gps_info(l);

	l->sep1 = gtk_hseparator_new();
	gtk_widget_set_size_request(l->sep1, 720, 10);
	gtk_widget_show(l->sep1);
	gtk_fixed_put(GTK_FIXED(l->fixed), l->sep1, 0, 110);

	l->sep2 = gtk_hseparator_new();
	gtk_widget_set_size_request(l->sep2, 720, 10);
	gtk_widget_show(l->sep2);
	gtk_fixed_put(GTK_FIXED(l->fixed), l->sep2, 0, 240);

}

int server_setup()
{
	int sock;
	struct sockaddr_un sockaddr;

	sockaddr.sun_family = AF_UNIX;
	strcpy(sockaddr.sun_path, SOCKPATH);
	unlink(SOCKPATH);

	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		return -errno;
	}

	if (bind(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr))) {
		perror("bind");
		return -errno;
	}

	return sock;
}

gboolean server_handle(GIOChannel *source, GIOCondition cond, gpointer user)
{
	int fd;
	struct layout *l = (void *)user;
	struct named_element *e;
	struct ui_msg msg;
	int ret;
	char *name = NULL;
	char *valu = NULL;

	fd = g_io_channel_unix_get_fd(source);

	ret = read(fd, &msg, sizeof(msg));
	if (ret != sizeof(msg)) {
		printf("Failed to read message (%i != %i)\n", ret, sizeof(msg));
		goto out;
	}

	if (msg.name_value.name_len) {
		name = calloc(1, msg.name_value.name_len + 1);
		if (!name) {
			printf("Can't alloc %i\n", msg.name_value.name_len);
			goto out;
		}

		ret = read(fd, name, msg.name_value.name_len);
		if (ret != msg.name_value.name_len) {
			printf("Failed to read name\n");
			goto out;
		}
	}

	if (msg.name_value.valu_len) {
		valu = calloc(1, msg.name_value.valu_len + 1);
		if (!valu) {
			printf("Can't alloc %i\n", msg.name_value.valu_len);
			goto out;
		}

		ret = read(fd, valu, msg.name_value.valu_len);
		if (ret != msg.name_value.valu_len) {
			printf("Failed to read value\n");
			goto out;
		}
	} else if (name)
		valu = strdup("");

	if (!name)
		goto out;

	e = get_element(l, name);
	if (!e) {
		printf("Unknown element `%s'\n", name);
		goto out;
	}

	//printf("Setting %s -> %s\n", name, valu);
	e->update_fn(e, valu);
 out:
	free(name);
	free(valu);

	return TRUE;
}

int server_loop(struct layout *l)
{
	int sock;
	GIOChannel *channel;
	guint id;

	sock = server_setup();
	if (sock < 0)
		return sock;

	channel = g_io_channel_unix_new(sock);
	g_io_channel_set_encoding(channel, NULL, NULL);
	id = g_io_add_watch_full(channel, 0, G_IO_IN, server_handle, l, NULL);

	gtk_main();

	return 0;
}

int main(int argc, char **argv)
{
	struct layout layout;

	gtk_init(&argc, &argv);

	aprs_pri_img = gdk_pixbuf_new_from_file("images/aprs_pri.png", NULL);

	layout.max = 32;
	layout.nxt = 0;
	layout.elements = calloc(layout.max, sizeof(struct named_element));

	make_window(&layout);

	server_loop(&layout);

	return 0;
}