/*
 * Compile with:
 *   gcc -Wall -std=gnu99 `pkg-config --libs --cflags glib-2.0 dbus-glib-1` libhal-flash.c -o hal-flash
 */

#include "libhal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <assert.h>

#include <dbus/dbus.h>

/**
 * LIBHAL_CHECK_PARAM_VALID:
 * @_param_: the prameter to check for 
 * @_name_:  the name of the prameter (for debug output) 
 * @_ret_:   what to use for return value if the prameter is NULL
 *
 * Handy macro for checking whether a parameter is valid and not NULL.
 */
#define LIBHAL_CHECK_PARAM_VALID(_param_,_name_,_ret_)        \
  do {                  \
    if (_param_ == NULL) {            \
      fprintf (stderr,          \
         "%s %d : invalid paramater. %s is NULL.\n",    \
         __FILE__, __LINE__, _name_);     \
      return _ret_;           \
    }               \
  } while(0)

struct LibHalContext_s {
  DBusConnection *connection;           /**< D-BUS connection */
  dbus_bool_t is_initialized;           /**< Are we initialised */
  void *user_data;                      /**< User data */
};

#define DBUS_SERVICE_UDISKS "org.freedesktop.UDisks"

#define DBUS_PATH_UDISKS "/org/freedesktop/UDisks"
#define DBUS_PATH_HAL_COMPUTER "/org/freedesktop/Hal/devices/computer"

#define DBUS_IFACE_UDISKS "org.freedesktop.UDisks"
#define DBUS_IFACE_UDISKS_DEVICE "org.freedesktop.UDisks.Device"

static DBusMessage*
_get_udisks_property(LibHalContext *ctx, const char *device_path, const char *iface_name, const char *property_name)
{
    DBusConnection* conn = ctx->connection;
    DBusMessage* msg;
    DBusMessage* reply;

    DBusError err;
    dbus_error_init(&err);
    
    msg = dbus_message_new_method_call(DBUS_SERVICE_UDISKS,
                                       device_path,
                                       DBUS_INTERFACE_PROPERTIES,
                                       "Get");
    
    if (!msg) {
        fprintf(stderr, "Failed to allocate DBus messaga\n");
        return NULL;
    }
    
    if (!dbus_message_append_args(msg,
                                  DBUS_TYPE_STRING, &iface_name, DBUS_TYPE_STRING, &property_name,
                                  DBUS_TYPE_INVALID)) {
        fprintf(stderr, "Failed to append arguments to DBus message\n");
        return NULL;
    }

    reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
    if (dbus_error_is_set(&err))
    {
        fprintf(stderr, "DBus Error: %s\n", err.message);
        dbus_error_free(&err);
        return NULL;
    }

    return reply;
}


static void
_extract_variant(DBusMessage* reply, int type, void* value_p)
{
    DBusMessageIter iter;
    DBusMessageIter child_iter;
    int variant_type;
    
    dbus_message_iter_init(reply, &iter);
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_VARIANT) {
        dbus_message_iter_recurse(&iter, &child_iter);  // variant iterator
        variant_type = dbus_message_iter_get_arg_type(&child_iter);
        if (variant_type == type) {
            dbus_message_iter_get_basic(&child_iter, value_p);
        } else {
            fprintf(stderr, "DBus Error: unexpected variant type: %c\n", variant_type);
        }
    } else {
        fprintf(stderr, "DBus Error: not a variant type\n");
    }
}

static dbus_bool_t
device_is_drive(LibHalContext *ctx, const char *device_path)
{
    DBusMessage* reply;
    dbus_bool_t is_drive;

    reply = _get_udisks_property(ctx, device_path, DBUS_IFACE_UDISKS_DEVICE, "DeviceIsDrive");    

    if (!reply)
        return FALSE;
    
    _extract_variant(reply, DBUS_TYPE_BOOLEAN, &is_drive);
    return is_drive;
}


static char *
device_storage_bus(LibHalContext *ctx, const char *device_path)
{
    DBusMessage* reply;
    char *interface_bus;
    
    reply = _get_udisks_property(ctx, device_path, DBUS_IFACE_UDISKS_DEVICE,
                                 "DriveConnectionInterface");

    if (!reply)
        return NULL;
    
    _extract_variant(reply, DBUS_TYPE_STRING, &interface_bus);
    return strdup(interface_bus);
}


static char *
device_drive_serial(LibHalContext *ctx, const char *device_path)
{
    DBusMessage* reply;
    char* drive_serial;
    
    reply = _get_udisks_property(ctx, device_path, DBUS_IFACE_UDISKS_DEVICE, "DriveSerial");
    
    if (!reply)
        return NULL;
    
    _extract_variant(reply, DBUS_TYPE_STRING, &drive_serial);    
    return strdup(drive_serial);
}


static dbus_uint64_t
device_size(LibHalContext *ctx, const char *device_path)
{
    DBusMessage* reply;
    dbus_uint64_t size;
    
    reply = _get_udisks_property(ctx, device_path, DBUS_IFACE_UDISKS_DEVICE, "DeviceSize");

    if (!reply)
        return 0;
    
    _extract_variant(reply, DBUS_TYPE_UINT64, &size);
    return size;
}

static char**
find_hard_drives(LibHalContext *ctx, int *num_devices)
{
    DBusConnection* conn = ctx->connection;
    DBusError err;
    DBusMessage* msg;
    DBusMessage* reply;
    char** object_paths;
    char* device_path;
    unsigned int object_paths_len;
    char** hdd_object_paths;
    unsigned int num_hdd = 0;
    dbus_uint64_t hdd_path_mask = 0;
    dbus_uint64_t tmp = 0;
    
    dbus_error_init(&err);
    
    msg = dbus_message_new_method_call(DBUS_SERVICE_UDISKS,
                                       DBUS_PATH_UDISKS,
                                       DBUS_IFACE_UDISKS,
                                       "EnumerateDevices");
    
    if (!msg) {
        fprintf(stderr, "DBus Error: Failed to allocate messaga\n");
        return NULL;
    }
    
    reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
    if (!reply)
    {
        fprintf(stderr, "DBus Error: %s\n", err.message);
        dbus_error_free(&err);
        return NULL;
    }
    
    if (!dbus_message_get_args(reply, &err, DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH,
                               &object_paths, &object_paths_len, DBUS_TYPE_INVALID)) {
        fprintf(stderr, "DBus Error: %s", err.message);
        dbus_error_free(&err);
        return NULL;        
    }
    
    assert(object_paths_len <= 64);
    
    for (unsigned int i=0; object_paths && i < object_paths_len; i++) {
        device_path = object_paths[i];
        if (device_is_drive(ctx, device_path)) {
            hdd_path_mask |= (1<<i);
        }
    }
    
    tmp = hdd_path_mask;    
    while ((tmp &= (tmp-1)) > 0)
        num_hdd++;
    
    hdd_object_paths = (char**) malloc(sizeof(char*)*(num_hdd+1));
    
    for (unsigned int i=0, j=0; object_paths && i < object_paths_len; i++) {
        if ((hdd_path_mask>>i) & 1) {
            hdd_object_paths[j++] = strdup(object_paths[i]);
        }
    }
    
    hdd_object_paths[num_hdd] = NULL;
    *num_devices = num_hdd;
    
    dbus_free_string_array(object_paths);
    return hdd_object_paths;
}

/**
 * libhal_manager_find_device_string_match:
 * @ctx: the context for the connection to hald
 * @key: name of the property
 * @value: the value to match
 * @num_devices: pointer to store number of devices
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Find a device in the GDL where a single string property matches a
 * given value.
 *
 * Returns: UDI of devices; free with libhal_free_string_array()
 */
char **
libhal_manager_find_device_string_match (LibHalContext *ctx, 
           const char *key,
           const char *value, int *num_devices, DBusError *error)
{
    char **hal_device_names;

    LIBHAL_CHECK_LIBHALCONTEXT(ctx, NULL);
    LIBHAL_CHECK_PARAM_VALID(key, "*key", NULL);
    LIBHAL_CHECK_PARAM_VALID(value, "*value", NULL);
  
    if (strcmp(key, "storage.drive_type") == 0 && strcmp(value, "disk") == 0) {
        hal_device_names = find_hard_drives(ctx, num_devices);
    } else {
        hal_device_names = (char **) malloc(sizeof(char *));
        hal_device_names[0] = NULL;
        *num_devices = 0;
    }
    
    return hal_device_names;
}

/**
 * libhal_free_string_array:
 * @str_array: the array to be freed
 *
 * Frees a NULL-terminated array of strings. If passed NULL, does nothing.
 */
void
libhal_free_string_array (char **str_array)
{
    if (str_array != NULL) {
        int i;

        for (i = 0; str_array[i] != NULL; i++) {
            free (str_array[i]);
            str_array[i] = NULL;
        }
        free (str_array);
        str_array = NULL;
    }
}

/**
 * libhal_ctx_new:
 *
 * Create a new LibHalContext
 *
 * Returns: a new uninitialized LibHalContext object
 */
LibHalContext *
libhal_ctx_new (void)
{
  LibHalContext *ctx;

  ctx = calloc (1, sizeof (LibHalContext));
  if (ctx == NULL) {
    fprintf (stderr, 
       "%s %d : Failed to allocate %lu bytes\n",
       __FILE__, __LINE__, (unsigned long) sizeof (LibHalContext));
    return NULL;
  }

  ctx->is_initialized = FALSE;
  ctx->connection = NULL;

  return ctx;
}

/**
 * libhal_ctx_set_dbus_connection:
 * @ctx: context to set connection for
 * @conn: DBus connection to use
 *
 * Set DBus connection to use to talk to hald.
 *
 * Returns: TRUE if connection was successfully set, FALSE otherwise
 */
dbus_bool_t
libhal_ctx_set_dbus_connection (LibHalContext *ctx, DBusConnection *conn)
{
    LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

    ctx->connection = conn;
    
    if (ctx->connection == NULL || conn == NULL)
        return FALSE;

    return TRUE;
}

/**
 * libhal_ctx_init:
 * @ctx: Context for connection to hald (D-BUS connection should be set with libhal_ctx_set_dbus_connection)
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Initialize the connection to hald.
 *
 * Returns: TRUE if initialization succeeds, FALSE otherwise
 */
dbus_bool_t 
libhal_ctx_init (LibHalContext *ctx, DBusError *error)
{
  LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
    
  if (ctx->connection == NULL)
    return FALSE;
  
  ctx->is_initialized = TRUE;
  return TRUE;
}

/**
 * libhal_ctx_shutdown:
 * @ctx: the context for the connection to hald
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Shut down a connection to hald.
 *
 * Returns: TRUE if connection successfully shut down, FALSE otherwise
 */
dbus_bool_t    
libhal_ctx_shutdown (LibHalContext *ctx, DBusError *error)
{
  LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

  ctx->is_initialized = FALSE;

  return TRUE;
}

/**
 * libhal_ctx_free:
 * @ctx: pointer to a LibHalContext
 *
 * Free a LibHalContext resource.
 *
 * Returns: TRUE
 */
dbus_bool_t    
libhal_ctx_free (LibHalContext *ctx)
{
  free (ctx);
  return TRUE;
}

/**
 * libhal_device_get_property_type:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Query a property type of a device.
 *
 * Returns: A LibHalPropertyType. LIBHAL_PROPERTY_TYPE_INVALID is
 * return if the property doesn't exist.
 */
LibHalPropertyType
libhal_device_get_property_type (LibHalContext *ctx, const char *udi, const char *key, DBusError *error)
{
  LIBHAL_CHECK_LIBHALCONTEXT(ctx, LIBHAL_PROPERTY_TYPE_INVALID); /* or return NULL? */
  LIBHAL_CHECK_PARAM_VALID(key, "*key", LIBHAL_PROPERTY_TYPE_INVALID);

  if (strcmp(udi, DBUS_PATH_HAL_COMPUTER) == 0 && strcmp(key, "system.hardware.serial") == 0)
    return LIBHAL_PROPERTY_TYPE_STRING;
  if (strncmp(udi, DBUS_PATH_UDISKS, strlen(DBUS_PATH_UDISKS)) == 0 && (
      strcmp(key, "storage.bus") == 0 || strcmp(key, "storage.serial") == 0))
    return LIBHAL_PROPERTY_TYPE_STRING;
  else if (strncmp(udi, DBUS_PATH_UDISKS, strlen(DBUS_PATH_UDISKS)) == 0 && 
           strcmp(key, "storage.size") == 0)
    return LIBHAL_PROPERTY_TYPE_UINT64;
  else
    return LIBHAL_PROPERTY_TYPE_INVALID;
}

/**
 * libhal_device_get_property_string:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: the name of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 * 
 * Get the value of a property of type string. 
 *
 * Returns: UTF8 nul-terminated string. The caller is responsible for
 * freeing this string with the function libhal_free_string(). Returns
 * NULL if the property didn't exist or we are OOM.
 */
char *
libhal_device_get_property_string(LibHalContext *ctx,
           const char *udi, const char *key, DBusError *error)
{     
    LIBHAL_CHECK_LIBHALCONTEXT(ctx, NULL);
    LIBHAL_CHECK_PARAM_VALID(key, "*key", NULL);

    if (strcmp(key, "system.hardware.serial") == 0)
        return dbus_get_local_machine_id();
    else if (strcmp(key, "storage.bus") == 0)
        return device_storage_bus(ctx, udi);
    else if (strcmp(key, "storage.serial") == 0)
        return device_drive_serial(ctx, udi);
    else
        return NULL;
}

/**
 * libhal_free_string:
 * @str: the nul-terminated sting to free
 *
 * Used to free strings returned by libhal.
 */
void
libhal_free_string (char *str)
{
  if (str != NULL) {
    free (str);
    str = NULL;
  }
}

/**
 * libhal_device_get_property_uint64:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Get the value of a property of type signed integer.
 *
 * Returns: Property value (64-bit unsigned integer)
 */
dbus_uint64_t
libhal_device_get_property_uint64 (LibHalContext *ctx, 
           const char *udi, const char *key, DBusError *error)
{
  LIBHAL_CHECK_LIBHALCONTEXT(ctx, -1);
  LIBHAL_CHECK_PARAM_VALID(key, "*key", -1);

  if (strcmp(key, "storage.size") == 0)
    return device_size(ctx, udi);
  else
    return -1;
}

/**
 * libhal_device_get_property_int:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Get the value of a property of type integer. 
 *
 * Returns: Property value (32-bit signed integer)
 */
dbus_int32_t
libhal_device_get_property_int (LibHalContext *ctx, 
                const char *udi, const char *key, DBusError *error)
{
    return -1;
}

/**
 * libhal_device_get_property_double:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Get the value of a property of type double.
 *
 * Returns: Property value (IEEE754 double precision float)
 */
double
libhal_device_get_property_double (LibHalContext *ctx, 
                   const char *udi, const char *key, DBusError *error)
{
    return -1.0f;
}
