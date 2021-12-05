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
#include <signal.h>

#define PORT_CIAA 1
#define PORT 10000
#define IP "127.0.0.1"
#define BAUDRATE 115200
#define BUFFER_SIZE 50
#define CMP_SIZE 5
#define ONE_DATA 1
#define CMP_MATCH 0
#define BACKLOG_SIZE 5 //Tamaño cola de peticiones pendientes.
#define KEYS 4
#define FIRST_KEY '0'
#define LAST_KEY '3'
#define SIGN_INT "LLegó señal SIGINT\n"
#define SIGN_TERM "LLegó señal SIGTERM\n"
#define STDOUT 1
int fd;				  //File descriptor del socket servidor.
int newfd;			  //Nuevo FD del socket que representa la conexión con el cliente.
pthread_t tcp_thread; //Handle de thread secundario.

/**
 * @brief Bloquea señales configuradas.
 * 
 */
void bloquearSign(void)
{
	sigset_t set;
	int s;
	if (sigemptyset(&set) == -1) //Se inicializa en vacio.
	{
		perror("Error en sigemptyset bloquear");
		exit(1);
	}
	if (sigaddset(&set, SIGINT) == -1 && sigaddset(&set, SIGTERM) == -1) //Se agregan señales al set.
	{
		perror("Error en sigaddset bloquear");
		exit(1);
	}
	if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0)
	{
		perror("Error en pthread_sigmask bloquear");
		exit(1);
	}
}

/**
 * @brief Desbloquea señales configuradas.
 * 
 */
void desbloquearSign(void)
{
	sigset_t set;
	int s;
	if (sigemptyset(&set) == -1) //Se inicializa en vacio.
	{
		perror("Error en sigemptyset desbloquear");
		exit(1);
	}
	if (sigaddset(&set, SIGINT) == -1 && sigaddset(&set, SIGTERM) == -1)
	{
		perror("Error en sigaddset desbloquear");
		exit(1);
	}
	if (pthread_sigmask(SIG_UNBLOCK, &set, NULL) != 0)
	{
		perror("Error en pthread_sigmask desbloquear");
		exit(1);
	}
}

/**
 * @brief Handler de señales.
 * 
 * @param sig Número de señal recibida.
 */
void signal_handler(int sig)
{
	if (sig == SIGTERM)
	{
		write(STDOUT, SIGN_TERM, strlen(SIGN_TERM));
	}
	if (sig == SIGINT)
	{
		write(STDOUT, SIGN_INT, strlen(SIGN_INT)); //Write es de bajo nivel.
	}

	//Se cancela thread secundario.
	pthread_cancel(tcp_thread);

	//Se espera por finalización de tcp_thread.
	if (pthread_join(tcp_thread, NULL) != 0)
	{
		perror("Error en tcp_thread");
		exit(1);
	}

	//Se cierra comunicación serial.
	serial_close();

	//Se cierran los sockets.
	if (close(newfd) == -1)
	{
		perror("Error al cerra newfd");
		exit(1);
	}
	if (close(fd) == -1)
	{
		perror("Error al cerra fd");
		exit(1);
	}
	exit(EXIT_SUCCESS);
}

/**
 * @brief Handler de tcp_thread que se encarga de crear el socket, aceptar conexiones, de recibir por el socket y de enviar por el puerto serie.
 * 
 * @param message 
 * @return void* 
 */
void *tcp_thread_handler(void *message)
{
	struct sockaddr_in clientAddr;	//Donde accept() guarda IP:PORT del cliente.
	struct sockaddr_in serverAddr;	//Donde se cargan los datos del servidor.
	socklen_t addrLen;				//Tamaño de la estructura.
	char ipClient[32];				//Cadena para guardar la ipClient en lenguaje humano.
	char threadBuffer[BUFFER_SIZE]; //Buffer del thread.
	int bytesRead;					//Número de bytes leidos por recv.
	int bytesPrint;					//Número de impresos por snprintf.
	char key[KEYS];					//Pulsadores

	printf("%s\n", (const char *)message); //Imprime por consola el parámetro que recibio, previamente lo castea.

	//Se crea socket de internet tipo stream. Socket() devuelve un FD.
	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("Error al crear socket");
		exit(1);
	}

	//Se cargan los 3 campos de la estructura serveraddr.
	bzero((char *)&serverAddr, sizeof(serverAddr)); //Se rellena con ceros la estructura.
	serverAddr.sin_family = AF_INET;				//Familia: socket de internet.
	serverAddr.sin_port = htons(PORT);				//Número de puerto a escuchar cargado en formato de red.
	//En campo sin_addr se carga IP.
	if (inet_pton(AF_INET, IP, &(serverAddr.sin_addr)) <= 0) //inet_pton transforma a la IP en formato de red.
	{
		fprintf(stderr, "ERROR invalid server IP\r\n");
		exit(1);
	}

	//Se abre puerto con bind(). Asigna una dirección local al socket.
	if (bind(fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
	{
		close(fd);
		perror("Listener: bind");
		exit(1);
	}

	//Se configura socket en modo Listening.
	if (listen(fd, BACKLOG_SIZE) == -1)
	{
		perror("Error en listen");
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
			perror("Error en accept");
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
				break; //Se sale del while porque se recibió EOF(0) debido a que cliente cerró la conexión.
			}
		}
		//Se cierra socket asociado con el cliente y se vuelve al inicio del while.
		printf("Server: cliente %s cerró la conexión\n", ipClient);
		if (close(newfd) == -1)
		{
			perror("Error al cerra newfd");
			exit(1);
		}
		newfd = 0;
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
	char mainBuffer[BUFFER_SIZE]; //Buffer del thread principal.
	char key;
	int bytesSend;			   //Número de bytes enviados por send.
	int bytesRead;			   //Número de bytes leidos por serial_receive.
	int bytesPrint;			   //Número de impresos por snprintf.
	struct sigaction sa1, sa2; //Estructura del tipo sigaction.

	sa1.sa_handler = signal_handler; //Se asigna al campo sa_handler el handler de la señal.
	sa2.sa_handler = signal_handler;
	sa1.sa_flags = 0;
	sa2.sa_flags = 0;
	//Se coloca la máscara en 0 utilizando la función sigemptyset.
	if (sigemptyset(&sa1.sa_mask) == -1 && sigemptyset(&sa2.sa_mask) == -1)
	{
		perror("sigemptyset");
		exit(1);
	}
	//Se llama a sigaction pasandole el número de señal a escuchar.
	if (sigaction(SIGINT, &sa1, NULL) == -1 && (sigaction(SIGTERM, &sa2, NULL) == -1))
	{
		perror("sigaction");
		exit(1);
	}

	//Se bloquean señales.
	bloquearSign();

	//Se abre comunicación serial.
	if (result = serial_open(PORT_CIAA, BAUDRATE) == 1)
	{
		printf("Error al abrir comunicación serial\n");
		exit(1);
	}

	const char *message1 = "Inicio TCP Service\n";

	//Se crea tcp_thread con señales bloqueadas.
	if (pthread_create(&tcp_thread, NULL, tcp_thread_handler, (void *)message1) != 0)
	{
		perror("Error al crear tcp thread");
	}

	//Se desbloquean señales para que las maneje thread principal.
	desbloquearSign();

	while (1)
	{
		bytesRead = serial_receive(mainBuffer, sizeof(mainBuffer));
		if (bytesRead > 0 && newfd > 0)
		{
			printf("LLegó por el puerto serie: %.*s\n", bytesRead, mainBuffer);
			if (sscanf(mainBuffer, ">TOGGLE STATE:%c\r\n", &key) == ONE_DATA)
			{
				//Ingresa si el número de entradas coinciden y se asignaron correctamente.
				if (key >= FIRST_KEY && key <= LAST_KEY)
				{
					//Se arma la trama ':LINEXTG\n'.
					bytesPrint = snprintf(mainBuffer, sizeof(mainBuffer), ":LINE%cTG\n", key);
					if (bytesPrint > 0)
					{
						//Se envia por el socket trama ':LINEXTG\n'.
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
	return 0;
}
