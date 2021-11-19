/* 
   Proceso escritor. 
   Crea una cola nombrada, y la abre como escritura cuando alguien la quiere leer.
   En ese momento pide que se ingresen caracteres por consola, y lo que se ingresó lo escribe en la cola.
   Devuelve la cantidad de bytes que pudo escribir.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>

#define FIFO_NAME "myfifo"
#define BUFFER_SIZE 300
#define SIGN1 "SIGN:1\n"
#define SIGN2 "SIGN:2\n"

int32_t fd;

void recibiSIGUSR1(int sig)
{
    if (write(fd, SIGN1, strlen(SIGN1)) == -1)
    {
        perror("Error de escritura señal SIGUSR1");
    }
}

void recibiSIGUSR2(int sig)
{
    if (write(fd, SIGN2, strlen(SIGN2)) == -1)
    {
        perror("Error de escritura señal SIGUSR2");
    }
}

int main(void)
{
    char consoleBuffer[BUFFER_SIZE];
    char outputBuffer[BUFFER_SIZE];
    int32_t returnCode;
    struct sigaction sa1,sa2;                   /* Se define estructura del tipo sigaction */

    /* Se crea cola nombrada de nombre myfifo. Retorno -1 significa que ya existe esta cola */
    if ((returnCode = mknod(FIFO_NAME, S_IFIFO | 0666, 0)) < -1)
    {
        printf("Error al crear cola nombrada: %d\n", returnCode);
        exit(1);
    }

    printf("Esperando por proceso lector...\n");

    /* Abrir cola nombrada. Se bloquea hasta que otro proceso la abra para leer */
    if ((fd = open(FIFO_NAME, O_WRONLY)) < 0)
    {
        printf("Error al abrir archivo de cola nombrada: %d\n", fd);
        exit(1);
    }

    sa1.sa_handler = recibiSIGUSR1;             /* Se asigna al campo sa_handler mi handler */
    sa1.sa_flags = 0;
    sigemptyset(&sa1.sa_mask);                  /* Se coloca la máscara en 0 utilizando la función sigemptyset */

    if (sigaction(SIGUSR1, &sa1, NULL) == -1)   /* Se llama a sigaction pasandole el número de señal a escuchar */
    {
        perror("SIGUSR1");
        exit(1);
    }

    sa2.sa_handler = recibiSIGUSR2;
    sa2.sa_flags = 0;
    sigemptyset(&sa2.sa_mask);

    if (sigaction(SIGUSR2, &sa2, NULL) == -1)
    {
        perror("SIGUSR2");
        exit(1);
    }

    /* Syscall open sin error -> otro proceso se encuentra asociado a la cola nombrada */
    printf("Se obtuvo un proceso lector. Escriba algo:\n");

    while (1)
    {
        /* Obtiene texto de la consola. Se bloquea esperando ENTER */
        if (fgets(consoleBuffer, BUFFER_SIZE, stdin) != NULL)
        {
            /* Se copia el encabezado "DATA:" al principio del outputBuffer */
            strcpy(outputBuffer, "DATA:");
            /* Se agrega consoleBuffer al final de outputBuffer */
            strcat(outputBuffer, consoleBuffer);
            /* Se escribe en la cola. Strlen para mandar el caracter \n y que sea más legible */
            if (write(fd, outputBuffer, strlen(outputBuffer)) == -1)
            {
                perror("Error de escritura en cola nombrada");
            }
        }
    }
    return 0;
}
