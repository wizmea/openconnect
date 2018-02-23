/*
 * compat.c - compatibility functions for non-Linux hosts
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "stoken-internal.h"

#ifndef HAVE_STRCASESTR

/*
 * Source: OpenConnect
 * Copyright © 2008-2014 Intel Corporation.
 * Authors: David Woodhouse <dwmw2@infradead.org>
 */
char *stoken__strcasestr(const char *haystack, const char *needle)
{
	int hlen = strlen(haystack);
	int nlen = strlen(needle);
	int i, j;

	for (i = 0; i < hlen - nlen + 1; i++) {
		for (j = 0; j < nlen; j++) {
			if (tolower(haystack[i + j]) !=
			    tolower(needle[j]))
				break;
		}
		if (j == nlen)
			return (char *)haystack + i;
	}
	return NULL;
}
#endif /* HAVE_STRCASESTR */

#ifndef HAVE_MKSTEMPS

/*
 * Source: FreeBSD libc
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 */

static const unsigned char padchar[] =
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static int
_gettemp(char *path, int *doopen, int domkdir, int slen)
{
	char *start, *trv, *suffp, *carryp;
	char *pad;
	struct stat sbuf;
	int rval;
	char carrybuf[MAXPATHLEN];

	if ((doopen != NULL && domkdir) || slen < 0) {
		errno = EINVAL;
		return (0);
	}

	for (trv = path; *trv != '\0'; ++trv)
		;
	if (trv - path >= MAXPATHLEN) {
		errno = ENAMETOOLONG;
		return (0);
	}
	trv -= slen;
	suffp = trv;
	--trv;
	if (trv < path || NULL != strchr(suffp, '/')) {
		errno = EINVAL;
		return (0);
	}

	/* Fill space with random characters */
	while (trv >= path && *trv == 'X') {
		uint32_t r = rand() % (sizeof(padchar) - 1);
		*trv-- = padchar[r];
	}
	start = trv + 1;

	/* save first combination of random characters */
	memcpy(carrybuf, start, suffp - start);

	/*
	 * check the target directory.
	 */
	if (doopen != NULL || domkdir) {
		for (; trv > path; --trv) {
			if (*trv == '/') {
				*trv = '\0';
				rval = stat(path, &sbuf);
				*trv = '/';
				if (rval != 0)
					return (0);
				if (!S_ISDIR(sbuf.st_mode)) {
					errno = ENOTDIR;
					return (0);
				}
				break;
			}
		}
	}

	for (;;) {
		if (doopen) {
			if ((*doopen =
			    open(path, O_CREAT|O_EXCL|O_RDWR, 0600)) >= 0)
				return (1);
			if (errno != EEXIST)
				return (0);
		} /* else if (domkdir) {
			if (mkdir(path, 0700) == 0)
				return (1);
			if (errno != EEXIST)
				return (0);
		} else if (lstat(path, &sbuf))
			return (errno == ENOENT); */

		/* If we have a collision, cycle through the space of filenames */
		for (trv = start, carryp = carrybuf;;) {
			/* have we tried all possible permutations? */
			if (trv == suffp)
				return (0); /* yes - exit with EEXIST */
			pad = strchr(padchar, *trv);
			if (pad == NULL) {
				/* this should never happen */
				errno = EIO;
				return (0);
			}
			/* increment character */
			*trv = (*++pad == '\0') ? padchar[0] : *pad;
			/* carry to next position? */
			if (*trv == *carryp) {
				/* increment position and loop */
				++trv;
				++carryp;
			} else {
				/* try with new name */
				break;
			}
		}
	}
	/*NOTREACHED*/
}

int stoken__mkstemps(char *path, int slen)
{
	int fd;

	return (_gettemp(path, &fd, 0, slen) ? fd : -1);
}

#endif /* HAVE_MKSTEMPS */

#if !defined(HAVE_GMTIME_R) && defined(_WIN32)
struct tm *stoken__gmtime_r(const time_t *timep, struct tm *result)
{
	/*
	 * This trick only works on Windows, because Windows gmtime()
	 * provides a dedicated buffer per-thread:
	 *
	 * http://msdn.microsoft.com/en-us/library/0z9czt0w.aspx
	 */
	memcpy(result, gmtime(timep), sizeof(struct tm));
	return result;
}
#endif /* !defined(HAVE_GMTIME_R) && defined(_WIN32) */

#ifndef HAVE_TIMEGM

/*
 * Source: Android Bionic libc
 * Copyright (c) 2007-2008  Michael G Schwern
 * Originally derived from Paul Sheer's pivotal_gmtime_r.c
 * License: MIT
 */

static const int julian_days_by_month[2][12] = {
	{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
	{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335},
};

static const int length_of_year[2] = { 365, 366 };
static const int days_in_gregorian_cycle = ((365 * 400) + 100 - 4 + 1);

#define IS_LEAP(n)      ((!(((n) + 1900) % 400) || \
			   (!(((n) + 1900) % 4) && \
			     (((n) + 1900) % 100))) != 0)

time_t stoken__timegm(struct tm *date)
{
	time_t days = 0;
	time_t seconds = 0;
	int64_t year;
	int64_t orig_year = (int64_t) date->tm_year;
	int cycles = 0;

	if (orig_year > 100) {
		cycles = (orig_year - 100) / 400;
		orig_year -= cycles * 400;
		days += (time_t) cycles *days_in_gregorian_cycle;
	} else if (orig_year < -300) {
		cycles = (orig_year - 100) / 400;
		orig_year -= cycles * 400;
		days += (time_t) cycles *days_in_gregorian_cycle;
	}

	if (orig_year > 70) {
		year = 70;
		while (year < orig_year) {
			days += length_of_year[IS_LEAP(year)];
			year++;
		}
	} else if (orig_year < 70) {
		year = 69;
		do {
			days -= length_of_year[IS_LEAP(year)];
			year--;
		} while (year >= orig_year);
	}

	days += julian_days_by_month[IS_LEAP(orig_year)][date->tm_mon];
	days += date->tm_mday - 1;

	seconds = days * 60 * 60 * 24;

	seconds += date->tm_hour * 60 * 60;
	seconds += date->tm_min * 60;
	seconds += date->tm_sec;

	return (seconds);
}

#endif /* HAVE_TIMEGM */
