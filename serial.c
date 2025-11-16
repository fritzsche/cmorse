#include "serial.h"
#include "morse.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <termios.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdatomic.h>
#include <time.h>

// Detect platform
#if defined(__APPLE__)
#define PLATFORM_APPLE
#else
#define PLATFORM_LINUX
#endif

// ---------------------------------------------------------------------------
// Common definitions
// ---------------------------------------------------------------------------
#define MAX_PORTS 32
#define PORT_NAME_MAX_LEN 256

typedef struct {
    char path[PORT_NAME_MAX_LEN];
    char base_name[PORT_NAME_MAX_LEN];
} SerialPortInfo;

typedef struct {
    int fd;
    key_state_type *p_key;
} serial_parameter;

// ---------------------------------------------------------------------------
// Serial device enumeration
// ---------------------------------------------------------------------------
int query_serial_devices(SerialPortInfo devices[], int max_ports)
{
    DIR *dir;
    struct dirent *ent;
    int count = 0;

    if ((dir = opendir("/dev/")) == NULL) {
        perror("Could not open /dev/");
        return 0;
    }

    while ((count < max_ports) && (ent = readdir(dir)) != NULL) {

#if defined(PLATFORM_APPLE)
        // macOS: cu.*
        if (strncmp(ent->d_name, "cu.", 3) == 0 ||
            strncmp(ent->d_name, "ttyUSB", 6) == 0 ||
            strncmp(ent->d_name, "ttyACM", 6) == 0)
#else
        // Linux: ttyUSB*, ttyACM*
        if (strncmp(ent->d_name, "ttyUSB", 6) == 0 ||
            strncmp(ent->d_name, "ttyACM", 6) == 0)
#endif
        {
            strncpy(devices[count].base_name, ent->d_name, PORT_NAME_MAX_LEN - 1);
            devices[count].base_name[PORT_NAME_MAX_LEN - 1] = '\0';

            snprintf(devices[count].path, PORT_NAME_MAX_LEN, "/dev/%s", ent->d_name);

            count++;
        }
    }

    closedir(dir);
    return count;
}

// ---------------------------------------------------------------------------
// Configure serial port
// ---------------------------------------------------------------------------
void configure_serial_port(int fd)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        return;
    }

    cfmakeraw(&tty);
    cfsetspeed(&tty, B9600);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
    }
}

// ---------------------------------------------------------------------------
// Open port
// ---------------------------------------------------------------------------
int open_serial_port(const char *port_name)
{
    int fd = open(port_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd == -1) {
        perror("open");
    }
    return fd;
}

// ---------------------------------------------------------------------------
// Platform-specific modem line monitoring
// ---------------------------------------------------------------------------

#if defined(PLATFORM_APPLE)

// macOS: TIOCMIWAIT is missing → use nanosleep polling
int poll_modem_lines(int fd, int old_status)
{
    int status;

    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 200000; // 0.2 ms

    while (1) {
        if (ioctl(fd, TIOCMGET, &status) == -1) {
            perror("ioctl TIOCMGET");
            return -1;
        }
        if (status != old_status)
            return status;

        nanosleep(&timeout, NULL);
    }
}

#else  // PLATFORM_LINUX

// Linux: use efficient blocking ioctl TIOCMIWAIT
int poll_modem_lines(int fd, int old_status)
{
    int mask = TIOCM_CTS | TIOCM_CAR; // CAR = CD on Linux

    if (ioctl(fd, TIOCMIWAIT, mask) < 0) {
        perror("ioctl TIOCMIWAIT");
        return -1;
    }

    int status;
    if (ioctl(fd, TIOCMGET, &status) < 0) {
        perror("ioctl TIOCMGET");
        return -1;
    }

    return status;
}

#endif // PLATFORM_LINUX

// ---------------------------------------------------------------------------
// Background serial monitoring thread
// ---------------------------------------------------------------------------
void *monitor_serial(void *arg)
{
    serial_parameter *parameter = (serial_parameter *)arg;

    int fd = parameter->fd;
    key_state_type *p_key = parameter->p_key;

    int old_status = 0;

    if (ioctl(fd, TIOCMGET, &old_status) == -1) {
        perror("ioctl TIOCMGET initial");
        return NULL;
    }

    int old_dit_status = (old_status & TIOCM_CTS) ? SET : UNSET;
    int old_dah_status = (old_status & TIOCM_CAR) ? SET : UNSET;

    while (1) {
        int status = poll_modem_lines(fd, old_status);
        if (status < 0)
            break;

        int dit_status = (status & TIOCM_CTS) ? SET : UNSET;
        int dah_status = (status & TIOCM_CAR) ? SET : UNSET;

        if (dit_status != old_dit_status) {
            if (dit_status == SET) {
                atomic_store(&(p_key->memory[DIT]), SET);
                atomic_store(&(p_key->state[DIT]), SET);
            } else {
                atomic_store(&(p_key->state[DIT]), UNSET);
            }
        }

        if (dah_status != old_dah_status) {
            if (dah_status == SET) {
                atomic_store(&(p_key->memory[DAH]), SET);
                atomic_store(&(p_key->state[DAH]), SET);
            } else {
                atomic_store(&(p_key->state[DAH]), UNSET);
            }
        }

        old_dit_status = dit_status;
        old_dah_status = dah_status;
        old_status = status;
    }

    return NULL;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
int list_serial()
{
    SerialPortInfo devices[MAX_PORTS];

    int count = query_serial_devices(devices, MAX_PORTS);

    if (count == 0) {
        printf("No serial devices found.\n");
        return 0;
    }

    printf("%d available Serial Devices\n", count);

    for (int i = 0; i < count; i++) {
        printf("  %d: %s\n", i + 1, devices[i].path);
    }

    return count;
}

int open_serial(void *p_key_state, int serial_device)
{
    SerialPortInfo devices[MAX_PORTS];
    serial_parameter parameter;

    int count = query_serial_devices(devices, MAX_PORTS);

    if (serial_device > count || serial_device < 1) {
        printf("Serial Device number %d invalid. Found %d devices.\n",
               serial_device, count);
        return -1;
    }

    printf("Using serial port: %s\n", devices[serial_device - 1].path);
    int fd = open_serial_port(devices[serial_device - 1].path);
    if (fd < 0) return -1;

    configure_serial_port(fd);

    parameter.fd = fd;
    parameter.p_key = p_key_state;

    pthread_t serial_thread;
    if (pthread_create(&serial_thread, NULL, monitor_serial, &parameter) != 0) {
        perror("pthread_create");
        close(fd);
        return -1;
    }

    pthread_detach(serial_thread);
    return 0;
}
