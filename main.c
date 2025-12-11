#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/sendfile.h>
#include <systemd/sd-device.h>
#include <systemd/sd-event.h>
#include <unistd.h>

// ID_FS_LABEL PROPRIERTIES
#define GLOVE_R "GLV80RHBOOT"
#define GLOVE_L "GLV80LHBOOT"
#define MOUNT_POINT "/tmp/glove80"
#define FIRMWARE_FILE_EXTENSION "uf2"

static void catAsciiArt(void) {
  printf("\n");
  printf(" +------------------------------------------+\n");
  printf(" |                                          |\n");
  printf(" |       GLOVE80 FIRMWARE INSTALLER         |\n");
  printf(" |                 by dave                  |\n");
  printf(" |                                          |\n");
  printf(" +------------------------------------------+\n");
  printf("\n");
}

__attribute__((unused)) static void printAllProps(sd_device *d) {
  const char *key;
  const char *value;

  key = sd_device_get_property_first(d, &value);

  printf("\n");
  while (key) {
    printf("%-30s = %s\n", key, value);

    key = sd_device_get_property_next(d, &value);
  }
  printf("\n");
}

const char *getFileExt(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot || dot == filename)
    return "";
  return dot + 1;
}

static int mountDevice(const char *devnode) {
  if (mkdir(MOUNT_POINT, 0755) < 0 && errno != EEXIST) {
    perror("Could not create directory");
    return -1;
  }

  int m = mount(devnode, MOUNT_POINT, "vfat", MS_NOATIME | MS_NOSUID | MS_NODEV,
                NULL);
  if (m < 0) {
    perror("Could not mount device");
    return -1;
  }

  printf("Glove80 FileSystem successfully mounted!");
  return 0;
}

static int safeInstall(const char *source, const char *dest_dir) {
  char dest_path[512];
  snprintf(dest_path, sizeof(dest_path), "%s/update.uf2", dest_dir);

  int src_fd = open(source, O_RDONLY);
  if (src_fd < 0)
    return -1;
  struct stat st;
  fstat(src_fd, &st);

  // Open Dest
  int dst_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (dst_fd < 0) {
    close(src_fd);
    return -1;
  }

  // Sendfile (Zero copy, fast)
  off_t offset = 0;
  if (sendfile(dst_fd, src_fd, &offset, st.st_size) < 0) {
    perror("Sendfile failed");
    close(src_fd);
    close(dst_fd);
    return -1;
  }

  // SYNC for flash memory
  fsync(dst_fd);

  close(src_fd);
  close(dst_fd);
  return 0;
}

static int install(const char *devnode, const char *firmware) {
  if (mountDevice(devnode) == -1)
    return -1;

  printf("Copying firmware (please wait)...\n");
  if (safeInstall(firmware, MOUNT_POINT) == -1) {
    printf("Copy failed!\n");
    umount(MOUNT_POINT); // Clean up
    return -1;
  }

  umount(MOUNT_POINT);
  printf("Firmware installed and device unmounted.\n");
  return 0;
}

static void exitLoop(sd_device_monitor *m, sd_device *d) {
  sd_device_monitor_stop(m);
  sd_event *loop = sd_device_monitor_get_event(m);
  sd_device_unref(d);
  sd_event_exit(loop, 0);
}

static int gloveMonitor(sd_device_monitor *m, sd_device *d, void *userdata) {
  const char *action = NULL;
  const char *devname = NULL;
  const char *fslabel = NULL;
  const char *firmware = userdata;

  // printAllProps(d);

  sd_device_get_property_value(d, "ACTION", &action);
  sd_device_get_property_value(d, "DEVNAME", &devname);
  sd_device_get_property_value(d, "ID_FS_LABEL", &fslabel);

  if (action == NULL || fslabel == NULL || devname == NULL) {
    fprintf(stderr, "Error Could not read device proprierties!\n");
    return 0;
  }

  static int rsfi = 0; // Right Side Firmware Installed state
  int is_add = strcmp(action, "add") == 0;
  int is_remove = strcmp(action, "remove") == 0;
  int is_left =
      (strcmp(fslabel, GLOVE_L) == 0); // Left side and right side state
  int is_right = (strcmp(fslabel, GLOVE_R) == 0);

  // printf("Event: %s\n Label: %s\nLeft Side: %d\nRight Side: %d\nRSFI:
  // %d\n",
  //        action, fslabel, ls, rs, rsfi);

  if (is_add) {
    if (is_right) {
      if (!rsfi) {
        printf(">>> RIGHT SIDE detected. Installing...\n");
        if (install(devname, firmware) == 0) {
          rsfi = 1;
          printf(">>> DONE. Please connect LEFT side.\n");
        }
      } else {
        printf(">>> RIGHT SIDE detected again. Already done.\n");
      }
    } else if (is_left) {
      if (rsfi) {
        printf(">>> LEFT SIDE detected. Installing...\n");
        if (install(devname, firmware) == 0) {
          printf(">>> ALL DONE. Exiting.\n");
          exitLoop(m, d);
        }
      } else {
        printf("!!! WRONG ORDER. Connect RIGHT side first.\n");
      }
    }
  } else if (is_remove) {
    if (is_right)
      printf("... Right side removed.\n");
  }

  return 0;
}

int main(int argc, char **argv) {
  sd_event *e = NULL;
  sd_device_monitor *m = NULL;
  sd_event_source *s = NULL;
  int r;

  if (argc < 2 || argc > 2) {
    printf("Usage: glovefi <firmware.utf>");
    return 0;
  }

  char *firmware = argv[1];
  if (access(firmware, F_OK) != 0) {
    fprintf(stderr, "Error: %s", strerror(errno));
    return -1;
  }

  const char *fext = getFileExt(firmware);
  if (strcmp(fext, FIRMWARE_FILE_EXTENSION) != 0) {
    fprintf(stderr, "File is not a Glove80 Firmware!\n");
    return -1;
  }

  r = sd_event_default(&e);
  if (r < 0) {
    fprintf(stderr, "Error sd_event_default: %s\n", strerror(-r));
    return -1;
  }

  r = sd_device_monitor_new(&m);
  if (r < 0) {
    fprintf(stderr, "Error monitor_new: %s\n", strerror(-r));
    return -1;
  }

  r = sd_device_monitor_filter_add_match_subsystem_devtype(m, "block", NULL);
  if (r < 0) {
    fprintf(stderr, "Failed to add subsystem match: %s\n", strerror(-r));
    return -1;
  }

  r = sd_device_monitor_attach_event(m, e);
  if (r < 0) {
    fprintf(stderr, "Error attach_event: %s\n", strerror(-r));
    return -1;
  }

  r = sd_device_monitor_start(m, gloveMonitor, (void *)firmware);

  catAsciiArt();
  printf(">>> To start please enter bootloader mode on the left side of "
         "the Glove80...\n");
  sd_event_loop(e);
  sd_device_monitor_unref(m);
  sd_event_source_unref(s);

  return 0;
}
