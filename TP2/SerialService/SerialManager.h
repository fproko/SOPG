#ifndef __SERIALMANAGER_H__
#define __SERIALMANAGER_H__

/**
 * @brief Función para abrir puerto serie en la PC.
 * 
 * @param pn Número de puerto.
 * @param baudrate Baudrate deseado.
 * @return int File Descriptor.
 */
int serial_open(int pn,int baudrate);

/**
 * @brief Función para mandar datos por puerto serie de la PC.
 * 
 * @param pData Ptr a buffer a enviar.
 * @param size Tamaño del dato a enviar.
 */
void serial_send(char* pData,int size);
void serial_close(void);

/**
 * @brief Función recibir datos por puerto serie de la PC. No es bloqueante.
 * 
 * @param buf Ptr a buffer donde se recibe.
 * @param size Tamaño del buffer.
 * @return int <0> Si no hay datos.
 *             <1> Si hay datos en buffer.
 */
int serial_receive(char* buf,int size);

#endif /* __SERIALMANAGER_H__ */

