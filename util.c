/* -*- Mode: C; tab-width: 8;  indent-tabs-mode: nil; c-basic-offset: 8; c-brace-offset: -8; c-argdecl-indent: 8 -*- */

/* dantracker aprs tracker
 *
 * Copyright 2012 Dan Smith <dsmith@danplanet.com>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#include "util.h"

const char *CARDINALS[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };

const char *direction(double degrees)
{
        return CARDINALS[((int)((degrees + 360 - 22.5) / 45.0)) % 7];
}

double get_direction(double fLng, double fLat, double tLng, double tLat)
{
        double rads;
        double result;

        fLng = DEG2RAD(fLng);
        fLat = DEG2RAD(fLat);
        tLng = DEG2RAD(tLng);
        tLat = DEG2RAD(tLat);

        rads = atan2(sin(tLng-fLng)*cos(tLat),
                     cos(fLat)*sin(tLat)-sin(fLat)*cos(tLat)*cos(tLng-fLng));

        result = RAD2DEG(rads);

        if (result < 0)
                return result + 360;
        else
                return result;
}

char *get_escaped_string(char *string)
{
        int i;
        char *escaped = NULL;
        int length = strlen(string) * 2 + 1;

        escaped = calloc(length, sizeof(char));

        /* Escape values */
        for (i = 0; i < strlen(string); i++) {
                if (strlen(escaped) + 6 >= length) {
                        char *tmp;
                        tmp = realloc(escaped, length * 2);
                        escaped = tmp;
                        length *= 2;
                }

                if (string[i] == '&')
                        strcat(escaped, "&amp;");
                else if (string[i] == '<')
                        strcat(escaped, "&lt;");
                else if (string[i] == '>')
                        strcat(escaped, "&gt;");
                else
                        strncat(escaped, &string[i], 1);
        }

        return escaped;
}

char *time2str(time_t *ptime)
{
        struct tm *ptm_now;
        static char tmstring[40];

        ptm_now = localtime(ptime);
        strftime(tmstring, sizeof(tmstring)-1,
                 "%d.%m.%y %H:%M:%S"
                 ,ptm_now );
        return( tmstring );
}

char *strupper(char *s)
{
        unsigned char *cp;

        if (s == NULL) {
                return NULL;
        }
        for (cp = (unsigned char *) s; *cp; cp++) {
                if (islower(*cp)) {
                        *cp = toupper(*cp);
                }
        }
        return s;
}
