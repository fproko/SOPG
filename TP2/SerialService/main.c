/*=============================================================================
 * Fernando Andres Prokopiuk <fernandoprokopiuk@gmail.com>
 * Version: 1.0
 * Fecha de creacion: 01/12/2021
 *===========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include "SerialManager.h"

#define PORT_CIAA 1
#define BAUDRATE 115200
#define BUFFER_SIZE 100
#define CMP_SIZE 5
#define ONE_DATA 1
#define CMP_MATCH 0

/**
 * @brief Handler de serial thread.
 * 
 * @param message 
 * @return void* 
 */
void *tcp_thread(void *message)
{
	printf("%s\n", (const char *)message);	   //Imprime por consola el parámetro que recibio, previamente lo castea.
	
	while (1)
	{
		usleep(10000); //10 ms
	}
	

	return NULL;
}

int main(void)
{
	int result;
	char inputBuffer[BUFFER_SIZE];
	char outputBuffer[BUFFER_SIZE];
	char key;
	char msj_hardcode[] = ">OUTS:2,2,2,2\r\n"; //FIXME: Borrar luego, test respuesta EDU-CIAA
	
	pthread_t tcp; //Se declara handler de hilos.
	const char *message1 = "Inicio TCP Service";

	if (pthread_create(&tcp, NULL, tcp_thread, (void *)message1) != 0)
	{
		perror("Error al crear tcp thread");
	}

	//Se abre comunicación serial
	if (result = serial_open(PORT_CIAA, BAUDRATE) == 1)
	{
		printf("Error al abrir comunicación serial\n");
		exit(1);
	}

	serial_send(msj_hardcode, sizeof(msj_hardcode)); //FIXME: Borrar luego, test respuesta EDU-CIAA

	while (1)
	{
		if (serial_receive(inputBuffer, sizeof(inputBuffer)) > 0)
		{
			printf("Se recibió:%s", inputBuffer);
			if (sscanf(inputBuffer, ">TOGGLE STATE:%c\r\n", &key) == ONE_DATA)
			{
				//Ingresa si el número de entradas coinciden y se asignaron correctamente.
				if (key >= '0' && key <= '3')
				{
					snprintf(outputBuffer, sizeof(outputBuffer), ":LINE%cTG\n", key);
					printf("Se envió:%s", outputBuffer);

					//TODO: enviar por socket.
					//write(fd,outputBuffer,strlen(outputBuffer)+1);
				}
			}
			else if (strncmp(inputBuffer, ">OK\r\n", CMP_SIZE) == CMP_MATCH)
			{
				printf("ok\n");
			}
			else
			{
				printf("Error en trama recibida\n");
			}
		}
		usleep(10000); //10 ms
	}

	printf("Se espera por finalización de tcp thread\n");
	if (pthread_join(tcp, NULL) != 0) //Hilo principal se queda acá bloqueado.
	{
		perror("Error en tcp thread");
	}

	exit(EXIT_SUCCESS);
	return 0;
}
