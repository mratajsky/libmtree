/*-
 * Copyright (c) 2015 Michal Ratajsky <michal@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "compat.h"
#include "mtree_private.h"

#define ERR_STR_SIZE		128

/*
 * Custom error codes
 */
#define ERR_PACK_MAJOR		(-1)
#define ERR_PACK_MINOR		(-2)
#define ERR_PACK_UNIT		(-3)
#define ERR_PACK_SUBUNIT	(-4)
#define ERR_PACK_NFIELDS	(-5)

struct mtree_device *
mtree_device_create(void)
{
	struct mtree_device *dev;

	dev = calloc(1, sizeof(struct mtree_device));
	if (dev != NULL)
		dev->format = MTREE_DEVICE_NATIVE;
	return (dev);
}

struct mtree_device *
mtree_device_copy(struct mtree_device *dev)
{
	struct mtree_device *copy;

	assert(dev != NULL);

	copy = mtree_device_create();
	if (copy != NULL)
		mtree_device_copy_data(copy, dev);
	return (copy);
}

void
mtree_device_copy_data(struct mtree_device *dev, struct mtree_device *from)
{

	assert(dev != NULL);
	assert(from != NULL);

	dev->format  = from->format;
	dev->fields  = from->fields;
	dev->number  = from->number;
	dev->major   = from->major;
	dev->minor   = from->minor;
	dev->unit    = from->unit;
	dev->subunit = from->subunit;
}

void
mtree_device_free(struct mtree_device *dev)
{

	assert(dev != NULL);

	free(dev);
}

mtree_device_format
mtree_device_get_format(struct mtree_device *dev)
{

	assert(dev != NULL);

	return (dev->format);
}

void
mtree_device_set_format(struct mtree_device *dev, mtree_device_format format)
{

	assert(dev != NULL);

	dev->format = format;
}

dev_t
mtree_device_get_value(struct mtree_device *dev, int field)
{

	assert(dev != NULL);

	if (dev->fields & field) {
		switch (field) {
		case MTREE_DEVICE_FIELD_NUMBER:
			return (dev->number);
		case MTREE_DEVICE_FIELD_MAJOR:
			return (dev->major);
		case MTREE_DEVICE_FIELD_MINOR:
			return (dev->minor);
		case MTREE_DEVICE_FIELD_UNIT:
			return (dev->unit);
		case MTREE_DEVICE_FIELD_SUBUNIT:
			return (dev->subunit);
		}
	}
	return (0);
}

void
mtree_device_set_value(struct mtree_device *dev, int field, dev_t value)
{

	assert(dev != NULL);

	switch (field) {
	case MTREE_DEVICE_FIELD_NUMBER:
		dev->number = value;
		break;
	case MTREE_DEVICE_FIELD_MAJOR:
		dev->major = value;
		break;
	case MTREE_DEVICE_FIELD_MINOR:
		dev->minor = value;
		break;
	case MTREE_DEVICE_FIELD_UNIT:
		dev->unit = value;
		break;
	case MTREE_DEVICE_FIELD_SUBUNIT:
		dev->subunit = value;
		break;
	default:
		return;
	}
	dev->fields |= field;
}

int
mtree_device_get_fields(struct mtree_device *dev)
{

	assert(dev != NULL);

	return (dev->fields);
}

void
mtree_device_unset_fields(struct mtree_device *dev, int fields)
{

	assert(dev != NULL);

	dev->fields &= ~fields;
}

/*
 * Set errno and error message for errors that occured in mtree_device_parse()
 * and mtree_device_string().
 */
static void
set_error(struct mtree_device *dev, int err, const char *format, ...)
{
	va_list args;

	errno = err;
	if (format == NULL) {
		free(dev->errstr);
		dev->errstr = NULL;
		return;
	}
	if (dev->errstr == NULL) {
		dev->errstr = malloc(ERR_STR_SIZE);
		if (dev->errstr == NULL)
			return;
	}
	va_start(args, format);
	vsnprintf(dev->errstr, ERR_STR_SIZE, format, args);
	va_end(args);
}

/*
 * The packing code is largely based on NetBSD's mknod/pack_dev.c
 */

/* Copyright (c) 1998, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
static int
pack_native(long numbers[], int n, struct mtree_device *dev)
{
	dev_t num, major, minor;

	if (n == 2) {
		num = makedev(numbers[0], numbers[1]);
		major = major(num);
		minor = minor(num);
		if (major != (dev_t) numbers[0])
			return (ERR_PACK_MAJOR);
		if (minor != (dev_t) numbers[1])
			return (ERR_PACK_MINOR);
		dev->number = num;
		dev->major  = major;
		dev->minor  = minor;
		dev->fields = MTREE_DEVICE_FIELD_NUMBER |
		    MTREE_DEVICE_FIELD_MAJOR |
		    MTREE_DEVICE_FIELD_MINOR;
		return (0);
	}
	return (ERR_PACK_NFIELDS);
}

#define	major_netbsd(x)		((((x) & 0x000fff00) >>  8))
#define	minor_netbsd(x)		((((x) & 0xfff00000) >> 12) |	\
				 (((x) & 0x000000ff) >>  0))
#define	makedev_netbsd(x,y)	((((x) <<  8) & 0x000fff00) |	\
				 (((y) << 12) & 0xfff00000) |	\
				 (((y) <<  0) & 0x000000ff))

static int
pack_netbsd(long numbers[], int n, struct mtree_device *dev)
{
	dev_t num, major, minor;

	if (n == 2) {
		num = makedev_netbsd(numbers[0], numbers[1]);
		major = major_netbsd(num);
		minor = minor_netbsd(num);
		if (major != (dev_t) numbers[0])
			return (ERR_PACK_MAJOR);
		if (minor != (dev_t) numbers[1])
			return (ERR_PACK_MINOR);
		dev->number = num;
		dev->major  = major;
		dev->minor  = minor;
		dev->fields = MTREE_DEVICE_FIELD_NUMBER |
		    MTREE_DEVICE_FIELD_MAJOR |
		    MTREE_DEVICE_FIELD_MINOR;
		return (0);
	}
	return (ERR_PACK_NFIELDS);
}

#define	major_freebsd(x)	((((x) & 0x0000ff00) >> 8))
#define	minor_freebsd(x)	((((x) & 0xffff00ff) >> 0))
#define	makedev_freebsd(x,y)	((((x) << 8) & 0x0000ff00) | \
				 (((y) << 0) & 0xffff00ff))

static int
pack_freebsd(long numbers[], int n, struct mtree_device *dev)
{
	dev_t num, major, minor;

	if (n == 2) {
		num = makedev_freebsd(numbers[0], numbers[1]);
		major = major_freebsd(num);
		minor = minor_freebsd(num);
		if (major != (dev_t) numbers[0])
			return (ERR_PACK_MAJOR);
		if (minor != (dev_t) numbers[1])
			return (ERR_PACK_MINOR);
		dev->number = num;
		dev->major  = major;
		dev->minor  = minor;
		dev->fields = MTREE_DEVICE_FIELD_NUMBER |
		    MTREE_DEVICE_FIELD_MAJOR |
		    MTREE_DEVICE_FIELD_MINOR;
		return (0);
	}
	return (ERR_PACK_NFIELDS);
}

#define	major_8_8(x)		((((x) & 0x0000ff00) >> 8))
#define	minor_8_8(x)		((((x) & 0x000000ff) >> 0))
#define	makedev_8_8(x,y)	((((x) << 8) & 0x0000ff00) | \
				 (((y) << 0) & 0x000000ff))

static int
pack_8_8(long numbers[], int n, struct mtree_device *dev)
{
	dev_t num, major, minor;

	if (n == 2) {
		num = makedev_8_8(numbers[0], numbers[1]);
		major = major_8_8(num);
		minor = minor_8_8(num);
		if (major != (dev_t) numbers[0])
			return (ERR_PACK_MAJOR);
		if (minor != (dev_t) numbers[1])
			return (ERR_PACK_MINOR);
		dev->number = num;
		dev->major  = major;
		dev->minor  = minor;
		dev->fields = MTREE_DEVICE_FIELD_NUMBER |
		    MTREE_DEVICE_FIELD_MAJOR |
		    MTREE_DEVICE_FIELD_MINOR;
		return (0);
	}
	return (ERR_PACK_NFIELDS);
}

#define	major_12_20(x)		((((x) & 0xfff00000) >> 20))
#define	minor_12_20(x)		((((x) & 0x000fffff) >>  0))
#define	makedev_12_20(x,y)	((((x) << 20) & 0xfff00000) | \
				 (((y) <<  0) & 0x000fffff))

static int
pack_12_20(long numbers[], int n, struct mtree_device *dev)
{
	dev_t num, major, minor;

	if (n == 2) {
		num = makedev_12_20(numbers[0], numbers[1]);
		major = major_12_20(num);
		minor = minor_12_20(num);
		if (major != (dev_t) numbers[0])
			return (ERR_PACK_MAJOR);
		if (minor != (dev_t) numbers[1])
			return (ERR_PACK_MINOR);
		dev->number = num;
		dev->major  = major;
		dev->minor  = minor;
		dev->fields = MTREE_DEVICE_FIELD_NUMBER |
		    MTREE_DEVICE_FIELD_MAJOR |
		    MTREE_DEVICE_FIELD_MINOR;
		return (0);
	}
	return (ERR_PACK_NFIELDS);
}

#define	major_14_18(x)		((((x) & 0xfffc0000) >> 18))
#define	minor_14_18(x)		((((x) & 0x0003ffff) >>  0))
#define	makedev_14_18(x,y)	((((x) << 18) & 0xfffc0000) | \
				 (((y) <<  0) & 0x0003ffff))

static int
pack_14_18(long numbers[], int n, struct mtree_device *dev)
{
	dev_t num, major, minor;

	if (n == 2) {
		num = makedev_14_18(numbers[0], numbers[1]);
		major = major_14_18(num);
		minor = minor_14_18(num);
		if (major != (dev_t) numbers[0])
			return (ERR_PACK_MAJOR);
		if (minor != (dev_t) numbers[1])
			return (ERR_PACK_MINOR);
		dev->number = num;
		dev->major  = major;
		dev->minor  = minor;
		dev->fields = MTREE_DEVICE_FIELD_NUMBER |
		    MTREE_DEVICE_FIELD_MAJOR |
		    MTREE_DEVICE_FIELD_MINOR;
		return (0);
	}
	return (ERR_PACK_NFIELDS);
}

#define	major_8_24(x)		((((x) & 0xff000000) >> 24))
#define	minor_8_24(x)		((((x) & 0x00ffffff) >>  0))
#define	makedev_8_24(x,y)	((((x) << 24) & 0xff000000) | \
				 (((y) <<  0) & 0x00ffffff))

static int
pack_8_24(long numbers[], int n, struct mtree_device *dev)
{
	dev_t num, major, minor;

	if (n == 2) {
		num = makedev_8_24(numbers[0], numbers[1]);
		major = major_8_24(num);
		minor = minor_8_24(num);
		if (major != (dev_t) numbers[0])
			return (ERR_PACK_MAJOR);
		if (minor != (dev_t) numbers[1])
			return (ERR_PACK_MINOR);
		dev->number = num;
		dev->major  = major;
		dev->minor  = minor;
		dev->fields = MTREE_DEVICE_FIELD_NUMBER |
		    MTREE_DEVICE_FIELD_MAJOR |
		    MTREE_DEVICE_FIELD_MINOR;
		return (0);
	}
	return (ERR_PACK_NFIELDS);
}

#define	major_12_12_8(x)	((((x) & 0xfff00000) >> 20))
#define	unit_12_12_8(x)		((((x) & 0x000fff00) >>  8))
#define	subunit_12_12_8(x)	((((x) & 0x000000ff) >>  0))
#define	makedev_12_12_8(x,y,z)	((((x) << 20) & 0xfff00000) |	\
				 (((y) <<  8) & 0x000fff00) |	\
				 (((z) <<  0) & 0x000000ff))

static int
pack_bsdos(long numbers[], int n, struct mtree_device *dev)
{
	dev_t num, major, minor, unit, subunit;

	if (n == 2) {
		num = makedev_12_20(numbers[0], numbers[1]);
		major = major_12_20(num);
		minor = minor_12_20(num);
		if (major != (dev_t) numbers[0])
			return (ERR_PACK_MAJOR);
		if (minor != (dev_t) numbers[1])
			return (ERR_PACK_MINOR);
		dev->number = num;
		dev->major  = major;
		dev->minor  = minor;
		dev->fields = MTREE_DEVICE_FIELD_NUMBER |
		    MTREE_DEVICE_FIELD_MAJOR |
		    MTREE_DEVICE_FIELD_MINOR;
		return (0);
	}
	if (n == 3) {
		num = makedev_12_12_8(numbers[0], numbers[1], numbers[2]);
		major   = major_12_12_8(num);
		unit    = unit_12_12_8(num);
		subunit = subunit_12_12_8(num);
		if (major != (dev_t) numbers[0])
			return (ERR_PACK_MAJOR);
		if (unit != (dev_t) numbers[1])
			return (ERR_PACK_UNIT);
		if (subunit != (dev_t) numbers[2])
			return (ERR_PACK_SUBUNIT);
		dev->number  = num;
		dev->major   = major;
		dev->unit    = unit;
		dev->subunit = subunit;
		dev->fields  = MTREE_DEVICE_FIELD_NUMBER |
		    MTREE_DEVICE_FIELD_MAJOR |
		    MTREE_DEVICE_FIELD_UNIT |
		    MTREE_DEVICE_FIELD_SUBUNIT;
		return (0);
	}
	return (ERR_PACK_NFIELDS);
}

typedef	int pack_t(long [], int, struct mtree_device *);
/*
 * List of formats and pack functions.
 * The list must be lexically sorted and array indices match format constants.
 */
static const struct format {
	const char		*name;
	mtree_device_format	 format;
	pack_t			*pack;
} formats[] = {
	{"386bsd",	MTREE_DEVICE_386BSD,	pack_8_8},
	{"4bsd",	MTREE_DEVICE_4BSD,	pack_8_8},
	{"bsdos",	MTREE_DEVICE_BSDOS,	pack_bsdos},
	{"freebsd",	MTREE_DEVICE_FREEBSD,	pack_freebsd},
	{"hpux",	MTREE_DEVICE_HPUX,	pack_8_24},
	{"isc",		MTREE_DEVICE_ISC,	pack_8_8},
	{"linux",	MTREE_DEVICE_LINUX,	pack_8_8},
	{"native",	MTREE_DEVICE_NATIVE,	pack_native},
	{"netbsd",	MTREE_DEVICE_NETBSD,	pack_netbsd},
	{"osf1",	MTREE_DEVICE_OSF1,	pack_12_20},
	{"sco",		MTREE_DEVICE_SCO,	pack_8_8},
	{"solaris",	MTREE_DEVICE_SOLARIS,	pack_14_18},
	{"sunos",	MTREE_DEVICE_SUNOS,	pack_8_8},
	{"svr3",	MTREE_DEVICE_SVR3,	pack_8_8},
	{"svr4",	MTREE_DEVICE_SVR4,	pack_14_18},
	{"ultrix",	MTREE_DEVICE_ULTRIX,	pack_8_8},
};

static int
compare_format(const void *key, const void *format)
{

	return (strcmp((const char *)key, ((struct format *)format)->name));
}

/*
 * Convert mtree_device to a string that can be used with the "device" keyword.
 */
int
mtree_device_string(struct mtree_device *dev, char **s)
{
	const struct format	*format;
	char			 buf[64];

	assert(dev != NULL);
	assert(s != NULL);

	/*
	 * Supported field combinations are:
	 *   number (only native format)
	 *   major + minor (all formats)
	 *   major + unit + subunit (only bsdos format)
	 */
	if (dev->format == MTREE_DEVICE_NATIVE &&
	    (dev->fields & MTREE_DEVICE_FIELD_NUMBER)) {
		snprintf(buf, sizeof(buf), "%ju", (uintmax_t) dev->number);
	} else if ((dev->fields & MTREE_DEVICE_FIELD_MAJOR) &&
	    (dev->fields & MTREE_DEVICE_FIELD_MINOR)) {
		format = &formats[dev->format];
		snprintf(buf, sizeof(buf), "%s,%ju,%ju",
		    format->name,
		    (uintmax_t) dev->major,
		    (uintmax_t) dev->minor);
	} else if ((dev->fields & MTREE_DEVICE_FIELD_MAJOR) &&
	    (dev->fields & MTREE_DEVICE_FIELD_UNIT) &&
	    (dev->fields & MTREE_DEVICE_FIELD_SUBUNIT)) {
		if (dev->format != MTREE_DEVICE_BSDOS) {
			set_error(dev, EINVAL,
			    "Unit and subunit fields are only supported with the bsdos format");
			return (-1);
		}
		format = &formats[dev->format];
		snprintf(buf, sizeof(buf), "%s,%ju,%ju,%ju",
		    format->name,
		    (uintmax_t) dev->major,
		    (uintmax_t) dev->unit,
		    (uintmax_t) dev->subunit);
	} else {
		set_error(dev, EINVAL, "Required field(s) missing");
		return (-1);
	}
	*s = strdup(buf);
	if (*s == NULL) {
		set_error(dev, errno, NULL);
		return (-1);
	}
	return (0);
}

/*
 * Fill the mtree_device according to the given string in one of the following
 * formats:
 *   format,major,minor
 *   format,major,unit,subunit
 *   number
 */
int
mtree_device_parse(struct mtree_device *dev, const char *s)
{
	struct format	*format;
	char		*name;
	char		*endptr;
	char		*sep;
	long		 numbers[3];
	dev_t		 number;
	int		 n;

	assert(dev != NULL);
	assert(s != NULL);

	/* Prevent empty string from being converted to 0. */
	if (*s == '\0') {
		set_error(dev, EINVAL, "Empty string not allowed");
		return (-1);
	}
	sep = strchr(s, ',');
	if (sep != NULL) {
		/*
		 * If there is a comma in the string, the first part must
		 * include the name of the format.
		 */
		name = strndup(s, sep - s);
		if (name == NULL) {
			set_error(dev, errno, NULL);
			return (-1);
		}
		format = bsearch(name, formats,
		    __arraycount(formats),
		    sizeof(formats[0]), compare_format);
		if (format == NULL) {
			set_error(dev, EINVAL, "Unsupported format '%s'", name);
			free(name);
			return (-1);
		}
		free(name);
		/*
		 * The format name is followed by 2-3 numbers, consecutive
		 * and trailing commas are ignored.
		 */
		s = sep;
		for (n = 0; n < 3; n++) {
			while (*s == ',')
				s++;
			if (*s == '\0')
				break;
			errno = 0;
			numbers[n] = strtol(s, &endptr, 10);
			if (errno == 0) {
				if (*endptr == ',' || *endptr == '\0') {
					s = endptr;
					continue;
				}
				set_error(dev, EINVAL,
				    "Format may only be followed by numbers");
			} else
				set_error(dev, errno, NULL);
			return (-1);
		}
		if (n < 2) {
			set_error(dev, EINVAL,
			    "Format must be followed by at least 2 numbers");
			return (-1);
		}
		n = format->pack(numbers, n, dev);
		if (n != 0) {
			switch (n) {
			case ERR_PACK_MAJOR:
				set_error(dev, EINVAL, "Invalid major number");
				break;
			case ERR_PACK_MINOR:
				set_error(dev, EINVAL, "Invalid minor number");
				break;
			case ERR_PACK_UNIT:
				set_error(dev, EINVAL, "Invalid unit number");
				break;
			case ERR_PACK_SUBUNIT:
				set_error(dev, EINVAL, "Invalid subunit number");
				break;
			case ERR_PACK_NFIELDS:
				set_error(dev, EINVAL,
				    "Too many fields for format '%s'", format->name);
				break;
			default:
				set_error(dev, EINVAL, NULL);
				break;
			}
			return (-1);
		}
		dev->format = format->format;
	} else {
		/*
		 * No comma, use the value as device number.
		 */
		errno = 0;
		number = strtol(s, &endptr, 10);
		/*
		 * Don't accept the input if it includes some non-numeric part.
		 */
		if (errno != 0 || *endptr != '\0') {
			if (errno == 0)
				set_error(dev, EINVAL,
				    "Format may only be followed by numbers");
			else
				set_error(dev, errno, NULL);
			return (-1);
		}
		dev->format = MTREE_DEVICE_NATIVE;
		dev->number = number;
		dev->fields = MTREE_DEVICE_FIELD_NUMBER;
	}
	return (0);
}

const char *
mtree_device_error(struct mtree_device *dev)
{

	assert(dev != NULL);

	return (dev->errstr != NULL ? dev->errstr : strerror(errno));
}
