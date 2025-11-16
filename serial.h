#ifndef SERIAL_H_
#define SERIAL_H_


#define SERIAL_SUPPORT 


#ifdef SERIAL_SUPPORT
int open_serial(void *,int);
int list_serial();
#endif

#endif