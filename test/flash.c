#include <stdlib.h>
#include <stdio.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#include "libhal.h"

#define DBUS_PATH_HAL_COMPUTER "/org/freedesktop/Hal/devices/computer"

void 
print_property(LibHalContext *hal_ctx, const char *udi, const char *key) {
  int type;
  char *val;
  DBusError error;

  dbus_error_init (&error);

  type = libhal_device_get_property_type(hal_ctx, udi, key, &error);
  switch (type) {
    case LIBHAL_PROPERTY_TYPE_STRING:
      val = libhal_device_get_property_string(hal_ctx, udi, key, &error);
      printf ("\t%s: %s\n", key, val);
      libhal_free_string(val);
      break;
    case LIBHAL_PROPERTY_TYPE_UINT64:
      {
        dbus_uint64_t value = libhal_device_get_property_uint64(hal_ctx, udi,
                                                                key, &error);
        printf("\t%s: %llu\n", key, (long long unsigned int) value);
      }
      break;
    default:
      fprintf (stderr, "Unexpected type %d='%c'\n", type, type);
      break;
  }
}

int
main(int argc, char* argv[])
{
    LibHalContext *hal_ctx;
    DBusConnection* conn;
    DBusError derror;
    void *handle;
    char *error;
    char** hdd_list;
    char** processor_list;
    unsigned int i;
    int num_devices;

    /* Initialize GType system */
    g_type_init ();

    dbus_error_init(&derror);
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &derror);
    
    if (!conn) {
        fprintf(stderr, "error connecting\n");
        return -1;
    }

    hal_ctx = libhal_ctx_new();
    if (!hal_ctx)
        return -2;

    if (!libhal_ctx_set_dbus_connection(hal_ctx, conn))
        return -3;


    if (!libhal_ctx_init(hal_ctx, &derror))
        return -4;
    
    /* return dbus_get_local_machine_id() */
    printf("sysinfo:\n"); 
    print_property(hal_ctx, DBUS_PATH_HAL_COMPUTER, "system.hardware.serial");
    
    i = 0;
    hdd_list = libhal_manager_find_device_string_match(hal_ctx,
                                                       "storage.drive_type",
                                                       "disk", &num_devices,
                                                       &derror);    
    while (hdd_list[i] != NULL) {
        char* udi_disk = hdd_list[i];
        printf("hdd: %s\n", udi_disk);
        print_property(hal_ctx, udi_disk, "storage.bus");
        print_property(hal_ctx, udi_disk, "storage.serial");
        print_property(hal_ctx, udi_disk, "storage.size");
        i++;
    }
    libhal_free_string_array(hdd_list);
    
    i = 0;
    processor_list = libhal_manager_find_device_string_match(hal_ctx,
                                                             "info.category",
                                                             "processor",
                                                             &num_devices,
                                                             &derror);
    while (processor_list[i] != NULL) {
        printf("processor: %s\n", processor_list[i]);
        i++;
    }
    libhal_free_string_array(processor_list);

    exit(EXIT_SUCCESS);
}
