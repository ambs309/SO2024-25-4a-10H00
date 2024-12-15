#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>

#define BUFFER_SIZE 256

typedef struct {
    int aluno;
    int disciplina;
    int horario;
} Inscricao;

Inscricao horarios[1000];
int totalHorarios = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *adminHandler(void *arg) {
    int adminFd = open("/tmp/admin", O_RDONLY);
    if (adminFd == -1) {
        perror("Erro ao abrir pipe admin");
        pthread_exit(NULL);
    }

    char buffer[BUFFER_SIZE];
    while (read(adminFd, buffer, BUFFER_SIZE) > 0) {
        int codop;
        sscanf(buffer, "%d", &codop);

        if (codop == 1) {
            int aluno;
            sscanf(buffer, "1,%d", &aluno);
            printf("Admin pediu horários do aluno %d\n", aluno);
        } else if (codop == 3) {
            printf("Admin: Pedido de término recebido.\n");
            pthread_exit(NULL);
        }
    }
    close(adminFd);
    return NULL;
}

void *studentHandler(void *arg) {
    int suporteFd = *((int *)arg);
    char buffer[BUFFER_SIZE];

    while (read(suporteFd, buffer, BUFFER_SIZE) > 0) {
        int aluno, disciplina;
        char respostaPipe[BUFFER_SIZE];
        sscanf(buffer, "%d,%d,%s", &aluno, &disciplina, respostaPipe);

        pthread_mutex_lock(&mutex);
        int horario = rand() % 3; // Exemplo: aleatório
        pthread_mutex_unlock(&mutex);

        char resposta[BUFFER_SIZE];
        snprintf(resposta, BUFFER_SIZE, "%d", horario);

        int respostaFd = open(respostaPipe, O_WRONLY);
        write(respostaFd, resposta, strlen(resposta) + 1);
        close(respostaFd);
    }
    return NULL;
}

int main() {
    mkfifo("/tmp/suporte", 0666);
    mkfifo("/tmp/admin", 0666);

    int suporteFd = open("/tmp/suporte", O_RDONLY | O_NONBLOCK);
    pthread_t adminThread;
    pthread_create(&adminThread, NULL, adminHandler, NULL);

    pthread_t studentThread;
    pthread_create(&studentThread, NULL, studentHandler, &suporteFd);

    pthread_join(adminThread, NULL);
    pthread_join(studentThread, NULL);

    close(suporteFd);
    unlink("/tmp/suporte");
    unlink("/tmp/admin");
    return 0;
}
