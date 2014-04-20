/*
 * libhal.c : HAL compatibility library for Flash using UDisks2
 *
 * Copyright (C) 2014 Chris Horler, <cshorler@googlemail.com>
 *
 * Contributors to the Original HAL code are stated below:
 * Copyright (C) 2003 David Zeuthen, <david@fubar.dk>
 * Copyright (C) 2006 Sjoerd Simons, <sjoerd@luon.net>
 * Copyright (C) 2007 Codethink Ltd. Author Rob Taylor <rob.taylor@codethink.co.uk>
 *
 * Licensed under the GNU General Public Licence v2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.         See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307         USA
 *
 **************************************************************************/

#include <glib.h>
#include <gio/gio.h>
#include <glib/gprintf.h>

#include "libhal.h"

#define DBUS_SERVICE_UDISKS2 "org.freedesktop.UDisks2"
#define DBUS_PATH_UDISKS2 "/org/freedesktop/UDisks2"
#define DBUS_IFACE_UDISKS2_DRIVE "org.freedesktop.UDisks2.Drive"
#define DBUS_IFACE_UDISKS2_DRIVE_ATA "org.freedesktop.UDisks2.Drive.Ata"

#define DBUS_PATH_HAL_COMPUTER "/org/freedesktop/Hal/devices/computer"

struct LibHalContext_s {
    DBusConnection *connection;           /**< D-BUS connection */
    dbus_bool_t is_initialized;           /**< Are we initialised */
    GDBusObjectManager* obj_manager;
    void *user_data;                      /**< User data */
};

static char*
get_connection_bus(GDBusObjectManager *obj_manager, const char* hdd_object_path) {
    GDBusProxy *hdd_interface;
    GDBusProxy *ata_interface = NULL;
    GVariant* property = NULL;
    const char* vdata_p = NULL;
    gchar* bus = NULL;
    gboolean is_ata = FALSE;
    
    hdd_interface = (GDBusProxy* ) g_dbus_object_manager_get_interface(obj_manager,
                                                                       hdd_object_path, DBUS_IFACE_UDISKS2_DRIVE);
    
    if (hdd_interface == NULL)
        goto out;
    
    ata_interface = (GDBusProxy* ) g_dbus_object_manager_get_interface(obj_manager,
                                                                       hdd_object_path, DBUS_IFACE_UDISKS2_DRIVE_ATA);
    
    if (ata_interface != NULL)
        is_ata = TRUE;
    
    property = g_dbus_proxy_get_cached_property(hdd_interface, "ConnectionBus");
    if (property == NULL)
        goto out;
    
    bus = g_variant_dup_string(property, NULL);
    if (g_strcmp0(bus, "") == 0 && is_ata) {
        g_free(bus);
        bus = g_strdup("ata");
    }
    
out:
    if (hdd_interface != NULL)
        g_object_unref(hdd_interface);
    
    if (ata_interface != NULL)
        g_object_unref(ata_interface);
    
    if (property != NULL)
        g_variant_unref(property);
    
    return g_strcmp0(bus,"") == 0 ? NULL : bus;
}

static char**
find_hard_drives(GDBusObjectManager *obj_manager, guint* count) {
    GList* object_list = NULL;
    GList* l;
    GPtrArray* object_path_list;
    GDBusObject* object;
    GDBusInterface* drive_proxy = NULL;
    char** hdd_list;
    const gchar *object_path;
    
    object_path_list = g_ptr_array_new();
    
    object_list = g_dbus_object_manager_get_objects(obj_manager);
    for (l = object_list; l != NULL; l = l->next) {
        object = l->data;
        object_path = g_dbus_object_get_object_path(object);
        drive_proxy = g_dbus_object_get_interface(object, DBUS_IFACE_UDISKS2_DRIVE);
        g_object_unref(object);
        if (drive_proxy != NULL)
            g_ptr_array_add(object_path_list, g_strdup(object_path));
    }
    
    *count = object_path_list->len;
    g_ptr_array_add(object_path_list, NULL);
    hdd_list = (char** ) g_ptr_array_free(object_path_list, FALSE);

    if (object_list != NULL)
        g_list_free(object_list);
    
    return hdd_list;
}

static char *
get_drive_serial(GDBusObjectManager *mgr, const char *object_path)
{
    GDBusProxy* drive_proxy = NULL;
    GVariant* property = NULL;
    char* drive_serial;
    
    drive_proxy = (GDBusProxy* ) g_dbus_object_manager_get_interface(mgr, object_path, DBUS_IFACE_UDISKS2_DRIVE);
    if (drive_proxy == NULL)
        goto out;
    
    property = g_dbus_proxy_get_cached_property(drive_proxy, "Serial");
    drive_serial = g_variant_dup_string(property, NULL);
    
out:
    if (property != NULL)
        g_variant_unref(property);
    
    if (drive_proxy != NULL)
        g_object_unref(drive_proxy);
    
    return drive_serial;
}

static guint64
get_drive_size(GDBusObjectManager *mgr, const char *object_path)
{
    GDBusProxy* drive_proxy = NULL;
    GVariant* property = NULL;
    guint64 size = 0L;
    
    drive_proxy = (GDBusProxy* ) g_dbus_object_manager_get_interface(mgr, object_path, DBUS_IFACE_UDISKS2_DRIVE);
    if (drive_proxy == NULL)
        goto out;
    
    property = g_dbus_proxy_get_cached_property(drive_proxy, "Size");
    size = g_variant_get_uint64(property);
    
out:
    if (property != NULL)
        g_variant_unref(property);
    
    if (drive_proxy != NULL)
        g_object_unref(drive_proxy);
    
    return size;
}

/* Public Interface */

char **
libhal_manager_find_device_string_match(LibHalContext *ctx, const char *key, const char *value, int *num_devices,
                                        DBusError *error)
{
    gchar **default_r_val = {NULL,};
    char **hal_device_names;

    g_return_val_if_fail(ctx, NULL);
    g_return_val_if_fail(key, NULL);
    g_return_val_if_fail(value, NULL);
  
    if (g_strcmp0(key, "storage.drive_type") == 0 && g_strcmp0(value, "disk") == 0) {
        hal_device_names = find_hard_drives(ctx->obj_manager, num_devices);
    } else {
        hal_device_names = g_strdupv(default_r_val);
        *num_devices = 0;
    }
    
    return hal_device_names;
}


void
libhal_free_string_array(char **str_array)
{
    g_strfreev(str_array);
}


LibHalContext *
libhal_ctx_new(void)
{
    LibHalContext *ctx;

    ctx = g_try_malloc0(sizeof(LibHalContext));
    if (ctx == NULL) {
        g_printerr("%s %d : Failed to allocate %lu bytes\n", __FILE__, __LINE__, sizeof(LibHalContext));
        return NULL;
    }

    ctx->is_initialized = FALSE;
    ctx->connection = NULL;
    ctx->obj_manager = NULL;

    return ctx;
}


dbus_bool_t
libhal_ctx_set_dbus_connection(LibHalContext *ctx, DBusConnection *conn)
{
    g_return_val_if_fail(ctx, FALSE);
    g_return_val_if_fail(conn, FALSE);
    GError *error = NULL;
    
    ctx->connection = conn;

    ctx->obj_manager = g_dbus_object_manager_client_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                                                     G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                                                     DBUS_SERVICE_UDISKS2,
                                                                     DBUS_PATH_UDISKS2,
                                                                     NULL, NULL, NULL, NULL,
                                                                     &error);
    
    if (ctx->obj_manager == NULL) {
        g_printerr("Error creating DBus Object Manager: %s\n", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    return TRUE;
}


dbus_bool_t 
libhal_ctx_init(LibHalContext *ctx, DBusError *error)
{
    g_return_val_if_fail(ctx, FALSE);

    if (ctx->connection == NULL)
        return FALSE;

    if (ctx->obj_manager == NULL)
        return FALSE;
        
    ctx->is_initialized = TRUE;
    return TRUE;
}


dbus_bool_t    
libhal_ctx_shutdown(LibHalContext *ctx, DBusError *error)
{
    g_return_val_if_fail(ctx, FALSE);

    ctx->is_initialized = FALSE;
    return TRUE;
}


dbus_bool_t    
libhal_ctx_free(LibHalContext *ctx)
{
    g_free(ctx);
    return TRUE;
}


LibHalPropertyType
libhal_device_get_property_type (LibHalContext *ctx, const char *udi, const char *key, DBusError *error)
{
    g_return_val_if_fail(ctx, LIBHAL_PROPERTY_TYPE_INVALID);
    g_return_val_if_fail(key, LIBHAL_PROPERTY_TYPE_INVALID);
    
    int property_type = LIBHAL_PROPERTY_TYPE_INVALID;

    if (g_strcmp0(udi, DBUS_PATH_HAL_COMPUTER) == 0) {
        if (g_strcmp0(key, "system.hardware.serial") == 0)
            property_type = LIBHAL_PROPERTY_TYPE_STRING;
    } else if (g_str_has_prefix(udi, DBUS_PATH_UDISKS2)) {
        if (g_strcmp0(key, "storage.bus") == 0 || g_strcmp0(key, "storage.serial") == 0) {
            property_type = LIBHAL_PROPERTY_TYPE_STRING;
        } else if (g_strcmp0(key, "storage.size") == 0) {
            property_type = LIBHAL_PROPERTY_TYPE_UINT64;
        }
    }
    
    return property_type;
}


char *
libhal_device_get_property_string(LibHalContext *ctx, const char *udi, const char *key, DBusError *error)
{
    g_return_val_if_fail(ctx, NULL);
    g_return_val_if_fail(key, NULL);

    if (g_strcmp0(key, "system.hardware.serial") == 0)
        return dbus_get_local_machine_id();
    else if (g_strcmp0(key, "storage.bus") == 0)
        return get_connection_bus(ctx->obj_manager, udi);
    else if (g_strcmp0(key, "storage.serial") == 0)
        return get_drive_serial(ctx->obj_manager, udi);
    else
        return NULL;
}


void
libhal_free_string (char *str)
{
    g_free(str);
    str = NULL;
}


dbus_uint64_t
libhal_device_get_property_uint64 (LibHalContext *ctx, const char *udi, const char *key, DBusError *error)
{
    g_return_val_if_fail(ctx, -1);
    g_return_val_if_fail(key, -1);

    if (g_strcmp0(key, "storage.size") == 0)
        return get_drive_size(ctx->obj_manager, udi);
    else
        return -1;
}


dbus_int32_t
libhal_device_get_property_int (LibHalContext *ctx, const char *udi, const char *key, DBusError *error)
{
    return -1;
}


double
libhal_device_get_property_double (LibHalContext *ctx, const char *udi, const char *key, DBusError *error)
{
    return -1.0f;
}

