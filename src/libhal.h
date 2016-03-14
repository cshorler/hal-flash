/*
 * libhal.h : HAL daemon C convenience library headers
 * 
 * NOTE: This file has been significantly simplified for use with HAL-Flash
 *
 * Copyright (C) 2014 Christopher Horler, <cshorler@googlemail.com>
 * Copyright (C) 2003 David Zeuthen, <david@fubar.dk>
 * Copyright (C) 2007 Codethink Ltd. Author Rob Taylor <rob.taylor@codethink.co.uk>
 *
 * Licensed under the Academic Free License version 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 **************************************************************************/

#ifndef LIBHAL_H
#define LIBHAL_H

#include <dbus/dbus.h>

#if defined(__cplusplus)
extern "C" {
#if 0
} /* shut up emacs indenting */
#endif
#endif


/** 
 * LibHalPropertyType:
 *
 * Possible types for properties on hal device objects 
 */
typedef enum {
        /** Used to report error condition */
	LIBHAL_PROPERTY_TYPE_INVALID =    DBUS_TYPE_INVALID,

	/** Type for 32-bit signed integer property */
	LIBHAL_PROPERTY_TYPE_INT32   =    DBUS_TYPE_INT32,

	/** Type for 64-bit unsigned integer property */
	LIBHAL_PROPERTY_TYPE_UINT64  =    DBUS_TYPE_UINT64,

	/** Type for double precision floating point property */
	LIBHAL_PROPERTY_TYPE_DOUBLE  =    DBUS_TYPE_DOUBLE,

	/** Type for boolean property */
	LIBHAL_PROPERTY_TYPE_BOOLEAN =    DBUS_TYPE_BOOLEAN,

	/** Type for UTF-8 string property */
	LIBHAL_PROPERTY_TYPE_STRING  =    DBUS_TYPE_STRING,

	/** Type for list of UTF-8 strings property */
	LIBHAL_PROPERTY_TYPE_STRLIST =    ((int) (DBUS_TYPE_STRING<<8)+('l'))
} LibHalPropertyType;


typedef struct LibHalContext_s LibHalContext;
typedef struct LibHalProperty_s LibHalProperty;
typedef struct LibHalPropertySet_s LibHalPropertySet;

/* Create a new context for a connection with hald */
LibHalContext *libhal_ctx_new                          (void);

/* Set DBus connection to use to talk to hald. */
dbus_bool_t    libhal_ctx_set_dbus_connection          (LibHalContext *ctx, DBusConnection *conn);

/* Initialize the connection to hald */
dbus_bool_t    libhal_ctx_init                         (LibHalContext *ctx, DBusError *error);

/* Shut down a connection to hald */
dbus_bool_t    libhal_ctx_shutdown                     (LibHalContext *ctx, DBusError *error);

/* Free a LibHalContext resource */
dbus_bool_t    libhal_ctx_free                         (LibHalContext *ctx);

/* Get the value of a property of type string. */
char *libhal_device_get_property_string (LibHalContext *ctx, 
					 const char *udi,
					 const char *key,
					 DBusError *error);

/* Get the value of a property of type signed integer. */
dbus_int32_t libhal_device_get_property_int (LibHalContext *ctx, 
					     const char *udi,
					     const char *key,
					     DBusError *error);

/* Get the value of a property of type unsigned integer. */
dbus_uint64_t libhal_device_get_property_uint64 (LibHalContext *ctx, 
						 const char *udi,
						 const char *key,
						 DBusError *error);

/* Get the value of a property of type double. */
double libhal_device_get_property_double (LibHalContext *ctx, 
					  const char *udi,
					  const char *key,
					  DBusError *error);

/* Get the value of a property of type bool. */
dbus_bool_t libhal_device_get_property_bool (LibHalContext *ctx,
					  const char *udi,
					  const char *key,
					  DBusError *error);

/* Query a property type of a device. */
LibHalPropertyType libhal_device_get_property_type (LibHalContext *ctx, 
						    const char *udi,
						    const char *key,
						    DBusError *error);


/* Frees a NULL-terminated array of strings. If passed NULL, does nothing. */
void libhal_free_string_array (char **str_array);

/* Frees a nul-terminated string */
void libhal_free_string (char *str);


/* Find a device in the GDL where a single string property matches a
 * given value.
 */
char **libhal_manager_find_device_string_match (LibHalContext *ctx,
						const char *key,
						const char *value,
						int *num_devices,
						DBusError *error);


#if defined(__cplusplus)
}
#endif

#endif /* LIBHAL_H */
