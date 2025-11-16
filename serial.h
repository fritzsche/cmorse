#ifndef SERIAL_H_
#define SERIAL_H_

#if defined(__APPLE__) || defined(__linux__)
 #define SERIAL_SUPPORT 
#endif

#ifdef SERIAL_SUPPORT
int open_serial(void *,int);
int list_serial();
#endif

#endif