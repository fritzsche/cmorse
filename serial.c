#include "serial.h"
#include "morse.h"
#if defined(__APPLE__)
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <termios.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define MAX_PORTS 32
#define PORT_NAME_MAX_LEN 256

typedef struct
{
    char path[PORT_NAME_MAX_LEN];
    char base_name[PORT_NAME_MAX_LEN];
} SerialPortInfo;

typedef struct
{
    int fd;
    key_state_type *p_key;
} serial_parameter;

/**
 * Queries all available serial devices and populates the provided array.
 * This is the core logic, called whenever a fresh list is needed.
 *
 * @param devices Array of SerialPortInfo to store the results.
 * @param max_ports The maximum capacity of the 'devices' array.
 * @return The number of devices found and stored, or 0 on failure.
 */
int query_serial_devices(SerialPortInfo devices[], int max_ports)
{
    DIR *dir;
    struct dirent *ent;
    int count = 0;

    if ((dir = opendir("/dev/")) == NULL)
    {
        perror("Could not open /dev/");
        return 0;
    }

    while ((count < max_ports) && (ent = readdir(dir)) != NULL)
    {

        // Filter common serial device names (macOS: cu.*, Linux: ttyUSB/ttyACM)
        if (strncmp(ent->d_name, "cu.", 3) == 0 ||
            strncmp(ent->d_name, "ttyUSB", 6) == 0 ||
            strncmp(ent->d_name, "ttyACM", 6) == 0)
        {
            // Store the base name
            strncpy(devices[count].base_name, ent->d_name, PORT_NAME_MAX_LEN - 1);
            devices[count].base_name[PORT_NAME_MAX_LEN - 1] = '\0';

            // Construct the full path
            strncpy(devices[count].path, "/dev/", PORT_NAME_MAX_LEN);
            strncat(devices[count].path, ent->d_name, PORT_NAME_MAX_LEN - strlen(devices[count].path) - 1);

            count++;
        }
    }
    closedir(dir);
    return count;
}

// ------------------------------------------------------------
// Configure port
// ------------------------------------------------------------
void configure_serial_port(int fd)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) != 0)
    {
        perror("tcgetattr");
        return;
    }

    cfmakeraw(&tty);
    cfsetspeed(&tty, B9600);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        perror("tcsetattr");
    }
}

// ------------------------------------------------------------
// Open port
// ------------------------------------------------------------
int open_serial_port(const char *port_name)
{
    int fd = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)
    {
        perror("open");
    }
    return fd;
}

// ------------------------------------------------------------
// Low-latency macOS-friendly "TIOCMIWAIT replacement"
// Polls every 200 µs with nanosleep
// ------------------------------------------------------------
int poll_modem_lines(int fd, int old_status)
{
    int status;

    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 200000; // 0.2 ms

    while (1)
    {
        if (ioctl(fd, TIOCMGET, &status) == -1)
        {
            perror("ioctl TIOCMGET");
            return -1;
        }

        if (status != old_status)
            return status;

        nanosleep(&timeout, NULL);
    }
}

void *monitor_serial(void *arg)
{
    printf("Thread Started!");

    serial_parameter *parameter;
    parameter = (serial_parameter *)arg;

    int fd = parameter->fd;
    int old_status = 0;

    key_state_type *p_key = parameter->p_key;

    if (ioctl(fd, TIOCMGET, &old_status) == -1)
    {
        perror("ioctl TIOCMGET initial");
        //    close(fd);
        return NULL;
    }

    int old_dit_status = (old_status & TIOCM_CTS) ? SET : UNSET;
    int old_dah_status = (old_status & TIOCM_CD) ? SET : UNSET;

    while (1)
    {
        // bool did = (old_status & TIOCM_CTS) ? 1 : 0)

        //    atomic_store(&(p_key->memory[DIT]), (old_status & TIOCM_CTS) ? SET:UNSET);
        //    atomic_store(&(p_key->memory[DAH]), (old_status & TIOCM_CD) ? SET:UNSET);

        int status = poll_modem_lines(fd, old_status);
        if (status < 0)
            break;
        int dit_status = (status & TIOCM_CTS) ? SET : UNSET;
        int dah_status = (status & TIOCM_CD) ? SET : UNSET;

        if (dit_status != old_dit_status)
        {

            if (dit_status == SET)
            {
                atomic_store(&(p_key->memory[DIT]), SET);
                atomic_store(&(p_key->state[DIT]), SET);
            }
            else
            {
                atomic_store(&(p_key->state[DIT]), UNSET);
            }
        }
        if (dah_status != old_dah_status)
        {
            if (dah_status == SET)
            {
                atomic_store(&(p_key->memory[DAH]), SET);
                atomic_store(&(p_key->state[DAH]), SET);
            }
            else
            {
                atomic_store(&(p_key->state[DAH]), UNSET);
            }
        }

        old_dit_status = dit_status;
        old_dah_status = dah_status;

        old_status = status;
    }

    return NULL;
}

#endif

#ifdef SERIAL_SUPPORT
// Requires the SerialPortInfo typedef and query_serial_devices function above.

/**
 * Lists serial devices by querying them and printing a numbered list.
 *
 * @return The number of ports found and listed, or 0 on failure.
 */
int list_serial()
{

#if defined(__APPLE__)
    SerialPortInfo devices[MAX_PORTS];

    // Query the devices
    int count = query_serial_devices(devices, MAX_PORTS);

    if (count == 0)
    {
        printf("No serial devices found.\n");
        return 0;
    }

    // Display the results

    printf("%d available Serial Devices\n", count);

    for (int i = 0; i < count; i++)
    {
        printf("  %d: %s \n",
               i + 1, devices[i].path);
    }

    return 0;
#endif
}

int open_serial(void *p_key_state, int serial_device)
{
#if defined(__APPLE__)
    SerialPortInfo devices[MAX_PORTS];
    serial_parameter parameter;

    // Query the devices
    int count = query_serial_devices(devices, MAX_PORTS);

    if (serial_device > count)
    {
        printf("Serial Device number %d too high. Only found %d devices.\n", serial_device, count);
        return -1;
    }
    printf("Using serial port: %s\n", devices[serial_device - 1].path);
    int fd = open_serial_port(devices[serial_device - 1].path);
    configure_serial_port(fd);

    pthread_t serialinthread;
    int status;

    parameter.fd = fd;
    parameter.p_key = p_key_state;
    status = pthread_create(&serialinthread, NULL, monitor_serial, &parameter);
    if (status == -1)
    {
        printf("Unable to create serial input thread.\n");
        return -1;
    }

    return 0;
#endif
}

#endif
