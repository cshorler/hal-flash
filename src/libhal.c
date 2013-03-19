/*
 * Compile with:
 *   gcc -Wall -std=gnu99 `pkg-config --libs --cflags glib-2.0 dbus-glib-1` libhal-flash.c -o hal-flash
 */

#include "libhal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

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
  DBusGConnection *connection;           /**< D-BUS connection */
  GMainContext *context;
  dbus_bool_t is_initialized;           /**< Are we initialised */
  void *user_data;                      /**< User data */
};

#define DBUS_SERVICE_UDISKS "org.freedesktop.UDisks"

#define DBUS_PATH_UDISKS "/org/freedesktop/UDisks"
#define DBUS_PATH_HAL_COMPUTER "/org/freedesktop/Hal/devices/computer"

#define DBUS_IFACE_UDISKS "org.freedesktop.UDisks"
#define DBUS_IFACE_UDISKS_DEVICE "org.freedesktop.UDisks.Device"

#define DBUS_TYPE_G_ARRAY_OF_OBJECT_PATH    (dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_OBJECT_PATH))

static GValue
get_udisks_property(LibHalContext *ctx, const char *device_path, const char *iface_name, const char *property_name)
{
    GError *error = NULL;
    DBusGProxy* proxy;
    GValue val = G_VALUE_INIT;
    
    proxy = dbus_g_proxy_new_for_name(ctx->connection,
                                      DBUS_SERVICE_UDISKS,
                                      device_path,
                                      DBUS_INTERFACE_PROPERTIES);
    
    if (!dbus_g_proxy_call(proxy, "Get", &error, G_TYPE_STRING, iface_name,
        G_TYPE_STRING, property_name, G_TYPE_INVALID, G_TYPE_VALUE, &val, G_TYPE_INVALID))
    {
        fprintf(stderr, "Interface: %s\nError Getting Property: %s\n", iface_name, property_name);
    }

    g_object_unref(proxy);
    return val;
}

static gboolean
device_is_drive(LibHalContext *ctx, const char *device_path)
{
    GValue val = G_VALUE_INIT;
    
    val = get_udisks_property(ctx, device_path, DBUS_IFACE_UDISKS_DEVICE, "DeviceIsDrive");    
    return g_value_get_boolean(&val);
}

static char *
device_storage_bus(LibHalContext *ctx, const char *device_path)
{
    GValue val;
    gchar **cnx_path;
    gchar *bus_start;
    gchar *bus = NULL;
    
    val = get_udisks_property(ctx, device_path, DBUS_IFACE_UDISKS_DEVICE, "DeviceFileByPath");    
    cnx_path = (gchar** ) g_value_get_boxed(&val);
    bus_start = rindex(cnx_path[0], '/');
    bus = strtok(bus_start+1, "-");

    return strdup(bus);
}

static gchar *
device_drive_serial(LibHalContext *ctx, const char *device_path)
{
    GValue val = G_VALUE_INIT;
    
    val = get_udisks_property(ctx, device_path, DBUS_IFACE_UDISKS_DEVICE, "DriveSerial");
    return g_value_dup_string(&val);
}

static dbus_uint64_t
device_size(LibHalContext *ctx, const char *device_path)
{
    GValue val = G_VALUE_INIT;
    
    val = get_udisks_property(ctx, device_path, DBUS_IFACE_UDISKS_DEVICE, "DeviceSize");
    return g_value_get_uint64(&val);
}

static char**
find_hard_drives(LibHalContext *ctx, int *num_devices)
{
    guint count = 0;
    GError *error = NULL;
    GPtrArray *hdd_array; // NULL
    DBusGProxy *proxy = NULL;
    char** buffer;
    gchar *device_path;
    
    buffer = (char **) malloc(sizeof(char *) * 8);

    proxy = dbus_g_proxy_new_for_name(ctx->connection, DBUS_SERVICE_UDISKS,
                                      DBUS_PATH_UDISKS, DBUS_IFACE_UDISKS);
    
    if (!dbus_g_proxy_call(proxy, "EnumerateDevices", &error, G_TYPE_INVALID,
                      DBUS_TYPE_G_ARRAY_OF_OBJECT_PATH, &hdd_array, G_TYPE_INVALID)) {
        // error
    }
  
    buffer[0] = NULL;
    for (guint i=0; hdd_array && i < hdd_array->len; i++) {
        device_path = g_ptr_array_index(hdd_array, i);
        if (device_is_drive(ctx, device_path)) {
            buffer[count] = strdup(device_path);
            count++;
        }

        if ((count % 8) == 0 && count != 0) {
            // TODO: realloc
        }
    }
    
    buffer[count] = NULL;
    *num_devices = count;
    g_ptr_array_free(hdd_array, TRUE);
    g_object_unref(proxy);

    return buffer;
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

  ctx->context = g_main_context_get_thread_default();
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
    GError *error = NULL;
    LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

    // this is essential as it also performs dbus-glib type initialisation
    // the other option is to duplicate _dbus_g_value_types_init()
    // and call it in main()
    ctx->connection = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
    
    if (ctx->connection == NULL || conn == NULL)
        return FALSE;

    // in case there's a need to retrieve conn via dbus-glib
    dbus_connection_setup_with_g_main(conn, NULL);

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
