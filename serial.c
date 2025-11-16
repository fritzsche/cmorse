/* serial.c
 * Cross-platform serial monitor: macOS, Linux, Windows (MinGW-w64)
 *
 * Public API:
 *   int list_serial(void);                       // prints serial ports, returns count
 *   int open_serial(void *p_key_state, int idx); // open port by index (1..N) and start monitor thread
 *
 * Requires:
 *   - serial.h, morse.h
 *   - C11 atomics (stdatomic.h)
 *   - pthread support (macOS, Linux, MinGW-w64)
 */

#include "serial.h"
#include "morse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS
#include <windows.h>
#include <stdint.h>
#include <pthread.h>
#else
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#endif

#define MAX_PORTS 32
#define PORT_NAME_MAX_LEN 256

typedef struct {
    char path[PORT_NAME_MAX_LEN];
    char base_name[PORT_NAME_MAX_LEN];
} SerialPortInfo;

typedef struct {
    void *handle;           /* POSIX: int fd via intptr_t, Windows: HANDLE */
    key_state_type *p_key;
} serial_parameter;

/* ---------------------------------------------------------------------- */
/* Serial device enumeration */

#if defined(PLATFORM_WINDOWS)
int query_serial_devices(SerialPortInfo devices[], int max_ports)
{
    int count = 0;
    for (int i = 1; i <= 256 && count < max_ports; ++i) {
        char comName[64];
        snprintf(comName, sizeof(comName), "\\\\.\\COM%d", i);

        HANDLE h = CreateFileA(comName, GENERIC_READ | GENERIC_WRITE,
                               0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            strncpy(devices[count].path, comName, PORT_NAME_MAX_LEN - 1);
            devices[count].path[PORT_NAME_MAX_LEN - 1] = '\0';
            snprintf(devices[count].base_name, PORT_NAME_MAX_LEN, "COM%d", i);
            CloseHandle(h);
            count++;
        }
    }
    return count;
}
#else
int query_serial_devices(SerialPortInfo devices[], int max_ports)
{
    DIR *dir = opendir("/dev/");
    if (!dir) return 0;
    struct dirent *ent;
    int count = 0;
    while ((count < max_ports) && (ent = readdir(dir))) {
#if defined(__APPLE__)
        if (strncmp(ent->d_name, "cu.", 3) == 0 ||
            strncmp(ent->d_name, "ttyUSB", 6) == 0 ||
            strncmp(ent->d_name, "ttyACM", 6) == 0 ||
            strncmp(ent->d_name, "tty.", 4) == 0)
#else
        if (strncmp(ent->d_name, "ttyUSB", 6) == 0 ||
            strncmp(ent->d_name, "ttyACM", 6) == 0 ) //||
       //     strncmp(ent->d_name, "ttyS", 4) == 0 )
#endif
        {
            snprintf(devices[count].path, PORT_NAME_MAX_LEN, "/dev/%.*s", (int)(PORT_NAME_MAX_LEN - 6), ent->d_name);

//            snprintf(devices[count].path, PORT_NAME_MAX_LEN, "/dev/%s", ent->d_name);
            strncpy(devices[count].base_name, ent->d_name, PORT_NAME_MAX_LEN - 1);
            devices[count].base_name[PORT_NAME_MAX_LEN - 1] = '\0';
            count++;
        }
    }
    closedir(dir);
    return count;
}
#endif

/* ---------------------------------------------------------------------- */
/* Serial open & configure */

#if defined(PLATFORM_WINDOWS)
static HANDLE open_serial_port_platform(const char *path)
{
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                           0, NULL, OPEN_EXISTING,
                           FILE_FLAG_OVERLAPPED, NULL);
    if (h == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) return INVALID_HANDLE_VALUE;

    dcb.BaudRate = CBR_9600;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    SetCommState(h, &dcb);

    COMMTIMEOUTS to = {1,1,0,1,0};
    SetCommTimeouts(h, &to);

    return h;
}

static DWORD get_modem_status_platform(HANDLE h)
{
    DWORD stat = 0;
    if (!GetCommModemStatus(h, &stat)) return 0;
    return stat;
}

#else
static int open_serial_port_platform(const char *path)
{
    int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return -1;
    struct termios tty;
    if (tcgetattr(fd, &tty) == 0) {
        cfmakeraw(&tty);
        cfsetspeed(&tty, B9600);
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cflag &= ~CRTSCTS;
        tcsetattr(fd, TCSANOW, &tty);
    }
    return fd;
}

static int poll_modem_lines_platform(int fd, int old_status)
{
    int status;
    struct timespec t = {0,200000};
    while(1) {
        if (ioctl(fd,TIOCMGET,&status)<0) return -1;
        if (status!=old_status) return status;
        nanosleep(&t,NULL);
    }
}


#endif

/* ---------------------------------------------------------------------- */
/* Monitor thread */

static void *monitor_serial_thread(void *arg)
{
    serial_parameter *param = (serial_parameter*)arg;
    if (!param) return NULL;

#if defined(PLATFORM_WINDOWS)
    HANDLE h = (HANDLE)param->handle;
    key_state_type *p_key = param->p_key;

    DWORD old_status = get_modem_status_platform(h);
    int old_dit = (old_status & MS_CTS_ON) ? SET : UNSET;
    int old_dah = (old_status & MS_RLSD_ON) ? SET : UNSET;

    while(1) {
        Sleep(1);
        DWORD status = get_modem_status_platform(h);
        if (status != 0) {
            printf("Out");
        }

        int dit = (status & MS_CTS_ON) ? SET : UNSET;
        int dah = (status & MS_RLSD_ON) ? SET : UNSET;

        if (dit != old_dit) {
            if (dit==SET) atomic_store(&(p_key->memory[DIT]),SET);
            atomic_store(&(p_key->state[DIT]),dit);
            old_dit=dit;
        }
        if (dah != old_dah) {
            if (dah==SET) atomic_store(&(p_key->memory[DAH]),SET);
            atomic_store(&(p_key->state[DAH]),dah);
            old_dah=dah;
        }
    }
#else
    int fd = (int)(intptr_t)param->handle;
    key_state_type *p_key = param->p_key;
    int old_status = 0;
    ioctl(fd,TIOCMGET,&old_status);
    int old_dit = (old_status & TIOCM_CTS) ? SET : UNSET;
    int old_dah = (old_status & TIOCM_CAR) ? SET : UNSET;

    while(1) {
        int status = poll_modem_lines_platform(fd,old_status);
        if (status<0) break;
        int dit = (status & TIOCM_CTS)?SET:UNSET;
        int dah = (status & TIOCM_CAR)?SET:UNSET;

        if (dit!=old_dit) {
            if (dit==SET) atomic_store(&(p_key->memory[DIT]),SET);
            atomic_store(&(p_key->state[DIT]),dit);
            old_dit=dit;
        }
        if (dah!=old_dah) {
            if (dah==SET) atomic_store(&(p_key->memory[DAH]),SET);
            atomic_store(&(p_key->state[DAH]),dah);
            old_dah=dah;
        }
        old_status=status;
    }
    close(fd);
#endif

    free(param);
    return NULL;
}

/* ---------------------------------------------------------------------- */
/* Public API */

int list_serial(void)
{
    SerialPortInfo devices[MAX_PORTS];
    int count = query_serial_devices(devices,MAX_PORTS);
    if (count<=0) {
        printf("No serial devices found.\n");
        return 0;
    }
    printf("%d available Serial Devices\n",count);
    for(int i=0;i<count;i++)
        printf(" %d: %s\n",i+1,devices[i].path);
    return count;
}

int open_serial(void *p_key_state,int serial_device)
{
    SerialPortInfo devices[MAX_PORTS];
    int count = query_serial_devices(devices,MAX_PORTS);
    if (count<=0) { fprintf(stderr,"No serial devices.\n"); return -1; }
    if (serial_device<1 || serial_device>count) {
        fprintf(stderr,"Invalid serial device number %d\n",serial_device);
        return -1;
    }

    const char *path = devices[serial_device-1].path;
    printf("Using serial port: %s\n",path);

    serial_parameter *param = malloc(sizeof(serial_parameter));
    if(!param) { fprintf(stderr,"Out of memory\n"); return -1; }
    param->p_key = (key_state_type*)p_key_state;

#if defined(PLATFORM_WINDOWS)
    HANDLE h = open_serial_port_platform(path);
    if(h==INVALID_HANDLE_VALUE) { free(param); return -1; }
    param->handle = (void*)h;
#else
    int fd = open_serial_port_platform(path);
    if(fd<0) { free(param); return -1; }
    param->handle = (void*)(intptr_t)fd;
#endif

    pthread_t thr;
    if(pthread_create(&thr,NULL,monitor_serial_thread,param)!=0){
        fprintf(stderr,"pthread_create failed\n");
#if defined(PLATFORM_WINDOWS)
        CloseHandle((HANDLE)param->handle);
#else
        close(fd);
#endif
        free(param);
        return -1;
    }
    pthread_detach(thr);
    return 0;
}
