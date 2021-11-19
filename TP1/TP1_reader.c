#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

#define FIFO_NAME "myfifo"
#define BUFFER_SIZE 300
#define CMP_SIZE 4

int main(void)
{
    char inputBuffer[BUFFER_SIZE];
    int32_t bytesRead, returnCode, fd;
    /* Se define punteros a estructura tipo FILE */
    FILE *plog, *psignals;

    /* Se crea cola nombrada. Retorno -1 significa que ya existe esta cola */
    if ((returnCode = mknod(FIFO_NAME, S_IFIFO | 0666, 0)) < -1)
    {
        printf("Error al crear cola nombrada: %d\n", returnCode);
        exit(1);
    }

    printf("Esperando por proceso escritor...\n");

    /* Abrir cola nombrada. Se bloquea hasta que otro proceso la abra para escribir */
    if ((fd = open(FIFO_NAME, O_RDONLY)) < 0)
    {
        printf("Error al abrir archivo de cola nombrada: %d\n", fd);
        exit(1);
    }

    /* Syscall open sin error -> otro proceso se encuentra asociado a la cola nombrada */
    printf("Se obtuvo un proceso escritor\n");

    /* Apertura de archivo log.txt. Retorna puntero si se pudo abrir correctamente, si no retorna NULL */
    if ((plog = fopen("log.txt", "a")) == NULL)
    {
        perror("Error al abrir el archivo log.txt");
        exit(1);
    }

    /* Apertura de archivo signals.txt. Retorna puntero si se pudo abrir correctamente, si no retorna NULL */
    if ((psignals = fopen("signals.txt", "a")) == NULL)
    {
        perror("Error al abrir el archivo signals.txt");
        exit(1);
    }

    /* Loop hasta que la syscall de read retorne un valor <= 0 */
    do
    {
        /* Se lee datos de la cola y se los guarda en inputBuffer. Retorna el número leido o -1 para error, o 0 para EOF.*/
        bytesRead = read(fd, inputBuffer, BUFFER_SIZE); //NOTE: a diferencia del write, read es bloqueante
        if (bytesRead == -1)
        {
            perror("Error de lectura de cola nombrada");
        }
        else
        {
            if (strncmp(inputBuffer, "DATA", CMP_SIZE) == 0)
            {
                if (fwrite(inputBuffer, sizeof(char), bytesRead, plog) != bytesRead)
                {
                    perror("Error de escritura en archivo log.txt");
                }
            }
            else if (strncmp(inputBuffer, "SIGN", CMP_SIZE) == 0)
            {
                if (fwrite(inputBuffer, sizeof(char), bytesRead, psignals) != bytesRead)
                {
                    perror("Error de escritura en archivo signals.txt");
                }
            }
            else
            {
                printf("Encabezado incorrecto\n");
            }
        }
    } while (bytesRead > 0);

    if (fclose(plog) == -1 && fclose(psignals) == -1)
    {
        printf("No se pudieron cerrar todos los archivos\n");
    }
    else
    {
        printf("Se cerraron todos los archivos\n");
    }

    return 0;
}
