#include <libusb-1.0/libusb.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define AOA_GET_PROTOCOL 51
#define USB_MAX_DEPTH 7

static volatile sig_atomic_t g_exit_flag = 0;

static void exit_signal_handler(int signo) { g_exit_flag = 1; }

static void *usb_device_thread_proc(void *param) {
  int ret = 0;
  u_int16_t aoa_version = 0;
  u_int8_t bnum;
  u_int8_t port_numbers[USB_MAX_DEPTH];
  int pnum;
  struct libusb_device *dev = param;
  struct libusb_device_handle *handle = NULL;
  char port_path[20];
  unsigned char manufacturer[200];

  if ((ret = libusb_open(dev, &handle)) != 0) {
    fprintf(stderr, "Error opening usb device: %s\n", libusb_strerror(ret));
    return NULL;
  }

  struct libusb_device_descriptor desc;

  libusb_get_device_descriptor(dev, &desc);

  libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, manufacturer,
                                     sizeof(manufacturer));

  bnum = libusb_get_bus_number(dev);
  pnum = libusb_get_port_numbers(dev, port_numbers, sizeof(port_numbers));

  int len = sprintf(port_path, "%d-", bnum);

  for (int i = 0; i < pnum; i++) {
    len += sprintf(port_path + len, i ? ".%d" : "%d", port_numbers[i]);
  }

  printf("Found device %4.4x:%4.4x, port path is: %s, manufacturer is %s\n",
         desc.idVendor, desc.idProduct, port_path, manufacturer);

  ret = libusb_control_transfer(
      handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR, AOA_GET_PROTOCOL,
      0, 0, (uint8_t *)&aoa_version, sizeof(aoa_version), 0);

  if (ret < 0 && ret != LIBUSB_ERROR_PIPE) {
    fprintf(stderr, "libusb_control_transfer failed: %s.\n",
            libusb_strerror(ret));
  }

  if (!aoa_version) {
    printf("not found AOA supports.\n");
    return NULL;
  }

  printf("Device supports AOA %d.0!, %x\n", aoa_version, aoa_version);

  return NULL;
}

void run_usb_probe_thread_detached(struct libusb_device *dev) {
  pthread_t th;
  pthread_attr_t attrs;
  pthread_attr_init(&attrs);
  pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
  pthread_create(&th, &attrs, usb_device_thread_proc, dev);
}

static int hotplug_callback(struct libusb_context *ctx,
                            struct libusb_device *dev,
                            libusb_hotplug_event event, void *arg) {
  printf("got hotplug event callback.\n");

  if (event != LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
    fprintf(stderr, "Unknown libusb_hotplug_event: %d\n", event);
    return 0;
  }

  run_usb_probe_thread_detached(dev);

  return 0;
}

int main(int argc, char *argv[]) {
  int rc = 0;
  libusb_hotplug_callback_handle callback_handle;
  libusb_init(NULL);

  signal(SIGINT, exit_signal_handler);

  /* libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);
   */

  rc = libusb_hotplug_register_callback(
      NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, LIBUSB_HOTPLUG_ENUMERATE,
      LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
      LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, NULL, &callback_handle);

  if (rc != LIBUSB_SUCCESS) {
    fprintf(stderr, "Error creating a hotplug callback\n");
    return EXIT_FAILURE;
  }

  while (!g_exit_flag) {
    libusb_handle_events_completed(NULL, NULL);
  }

  libusb_hotplug_deregister_callback(NULL, callback_handle);

  libusb_exit(NULL);

  return EXIT_SUCCESS;
}
