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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "SerialManager.h"

#define PORT_CIAA 1
#define BAUDRATE 115200
#define BUFFER_SIZE 50
#define CMP_SIZE 5
#define ONE_DATA 1
#define CMP_MATCH 0
#define BACKLOG_SIZE 5 //Tamaño cola de peticiones pendientes.
#define KEYS 4
#define FIRST_KEY '0'
#define LAST_KEY '3'

int newfd = 0; //Nuevo FD que devuelve accept().

/**
 * @brief Handler de tcp_thread que se encarga de crear el socket, aceptar conexiones, de recibir por el socket y de enviar por el puerto serie.
 * 
 * @param message 
 * @return void* 
 */
void *tcp_thread(void *message)
{
	struct sockaddr_in clientAddr;	//Donde accept() guarda IP:PORT del cliente.
	struct sockaddr_in serverAddr;	//Donde se cargan los datos del servidor.
	socklen_t addrLen;				//Tamaño de la estructura.
	char ipClient[32];				//Cadena para guardar la ipClient en lenguaje humano.
	char threadBuffer[BUFFER_SIZE]; //Buffer del hilo.
	int fd;							//File descriptor socket servidor.
	int bytesRead;					//Número de bytes leidos por recv.
	int bytesPrint;					//Número de impresos por snprintf.
	char key[KEYS];					//Pulsadores

	printf("%s\n", (const char *)message); //Imprime por consola el parámetro que recibio, previamente lo castea.

	//Se crea socket de internet tipo stream. Socket() devuelve un FD.
	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("error al crear socket");
		exit(1);
	}

	//Se cargan los 3 campos de la estructura serveraddr.
	bzero((char *)&serverAddr, sizeof(serverAddr)); //Se rellena con ceros la estructura.
	serverAddr.sin_family = AF_INET;				//Familia: socket de internet.
	serverAddr.sin_port = htons(10000);				//Número de puerto a escuchar cargado en formato de red.
	//En campo sin_addr se carga IP.
	if (inet_pton(AF_INET, "127.0.0.1", &(serverAddr.sin_addr)) <= 0) //inet_pton transforma a la IP en formato de red.
	{
		fprintf(stderr, "ERROR invalid server IP\r\n");
		exit(1);
	}

	//Se abre puerto con bind(). Asigna una dirección local al socket.
	if (bind(fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
	{
		close(fd);
		perror("listener: bind");
		exit(1);
	}

	//Se configura socket en modo Listening.
	if (listen(fd, BACKLOG_SIZE) == -1)
	{
		perror("error en listen");
		exit(1);
	}

	while (1)
	{
		//Se calcula tamaño de la estructura.
		addrLen = sizeof(struct sockaddr_in);

		//Se ejecuta accept(). Se queda bloqueado hasta recibir una solicitud de conexión en la cola.
		//Devuelve un nuevo FD que representa conexión con cliente y la cantidad de bytes que escribió en la estructura.
		newfd = accept(fd, (struct sockaddr *)&clientAddr, &addrLen);
		if (newfd == -1)
		{
			perror("error en accept");
			exit(1);
		}

		//Se imprimen los datos del cliente.
		inet_ntop(AF_INET, &(clientAddr.sin_addr), ipClient, sizeof(ipClient));
		printf("Server: conexión desde: %s\n", ipClient);

		while (1)
		{
			//Se leen datos del socket. Se queda bloqueado hasta que cliente envie algo por el socket.
			bytesRead = recv(newfd, threadBuffer, sizeof(threadBuffer), 0);
			if (bytesRead == -1)
			{
				perror("Error al leer mensaje del socket");
				exit(1);
			}
			else if (bytesRead > 0)
			{
				printf("LLegó por el socket: %.*s\n", bytesRead, threadBuffer);

				//Se procesa trama ':STATESXYWZ\n'
				if (sscanf(threadBuffer, ":STATES%c%c%c%c\n", &key[0], &key[1], &key[2], &key[3]) == KEYS)
				{
					//Ingresa si el número de entradas coinciden y se asignaron correctamente.
					for (int i = 0; i < KEYS; i++)
					{
						if (!(key[i] >= FIRST_KEY && key[i] <= LAST_KEY)) //Se valida que las teclas estén entre FIRST_KEY y LAST_KEY.
						{
							printf("Error en la trama recibida por el socket\n");
							bytesRead = 0;
							break;
						}
					}
					if (bytesRead > 0)
					{
						bytesPrint = snprintf(threadBuffer, sizeof(threadBuffer), ">OUTS:%c,%c,%c,%c\r\n", key[0], key[1], key[2], key[3]);
						if (bytesPrint > 0)
						{
							serial_send(threadBuffer, bytesPrint); //Se envia a EDU_CIAA la trama: '>OUTS:X,Y,W,Z\r\n'
							printf("Envio por el puerto serie: %.*s\n", bytesPrint, threadBuffer);
						}
						else
						{
							printf("Error al armar la trama'>OUTS:X,Y,W,Z\r\n'");
						}
						
					}
				}
			}
			else
			{
				break;
			}
		}
		//Se cierra conexión con el cliente y se vuelve al inicio del while.
		printf("Server: cliente %s cerró la conexión", ipClient);
		printf("\n");
		close(newfd);
	}
	return NULL;
}

/**
 * @brief Función principal que se encarga crear el tcp_thread, de recibir por el puerto serie y de enviar por el socket.
 * 
 * @return int 
 */
int main(void)
{
	int result;
	char mainBuffer[BUFFER_SIZE]; //Buffer del hilo principal.
	char key;
	pthread_t tcp;	//Se declara handler de hilos.
	int bytesSend;	//Número de bytes enviados por send.
	int bytesRead;	//Número de bytes leidos por serial_receive.
	int bytesPrint; //Número de impresos por snprintf.

	const char *message1 = "Inicio TCP Service\n";

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

	while (1)
	{
		bytesRead = serial_receive(mainBuffer, sizeof(mainBuffer));
		if (bytesRead > 0)
		{
			printf("LLegó por el puerto serie: %.*s\n", bytesRead, mainBuffer);
			if (sscanf(mainBuffer, ">TOGGLE STATE:%c\r\n", &key) == ONE_DATA)
			{
				//Ingresa si el número de entradas coinciden y se asignaron correctamente.
				if (key >= FIRST_KEY && key <= LAST_KEY)
				{
					//Se arma la trama ':LINEXTG\n'
					bytesPrint = snprintf(mainBuffer, sizeof(mainBuffer), ":LINE%cTG\n", key);
					if (bytesPrint > 0)
					{
						//Se envia por el socket trama ':LINEXTG\n'
						if ((bytesSend = send(newfd, mainBuffer, bytesPrint, 0)) == -1)
						{
							perror("Error escribiendo mensaje en socket");
							exit(1);
						}
						else
						{
							printf("Envio por el socket %d bytes: %s\n", bytesSend, mainBuffer);
						}
					}
					else
					{
						printf("Error al armar la trama ':LINEXTG'\n");
					}
				}
			}
		}
		usleep(100000); //100 ms
	}

	printf("Se espera por finalización de tcp thread\n");
	if (pthread_join(tcp, NULL) != 0) //Hilo principal se queda acá bloqueado.
	{
		perror("Error en tcp thread");
	}

	exit(EXIT_SUCCESS);
	return 0;
}
