#ifndef __SERIALMANAGER_H__
#define __SERIALMANAGER_H__

int serial_open(int pn,int baudrate);
void serial_send(char* pData,int size);
void serial_close(void);
int serial_receive(char* buf,int size);

#endif /* __SERIALMANAGER_H__ */

