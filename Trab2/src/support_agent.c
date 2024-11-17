#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>

#define BUFFER_SIZE 256

typedef struct {
    int disciplina;
    int horario;
    int numLugares;
    int vagas;
} HorarioP;

typedef struct {
    int alunoInicial;
    int numAlunos;
    char studentPipe[BUFFER_SIZE];
} Pedido;

HorarioP *horarioP;
pthread_mutex_t horarioP_mutex = PTHREAD_MUTEX_INITIALIZER;
int NDISCIP, NHOR, NLUG, totalAlunosInscritos = 0, totalAlunos;
char SUPPORT_PIPE[BUFFER_SIZE];

typedef struct ThreadNode {
    pthread_t thread;
    struct ThreadNode *next;
} ThreadNode;

ThreadNode *threadList = NULL;
pthread_mutex_t threadList_mutex = PTHREAD_MUTEX_INITIALIZER;

void inicializarHorario() {
    for (int i = 0; i < NDISCIP * NHOR; i++) {
        horarioP[i].disciplina = i / NHOR;
        horarioP[i].horario = i % NHOR;
        horarioP[i].numLugares = NLUG;
        horarioP[i].vagas = NLUG;
    }
}

void *processarPedido(void *arg) {
    Pedido *req = (Pedido *)arg;
    int alunosInscritos = 0;

    
    pthread_mutex_lock(&horarioP_mutex);
    for (int i = 0; i < NDISCIP * NHOR && req->numAlunos > 0; i++) {
        if (horarioP[i].vagas > 0) {
            int vagasDisponiveis = horarioP[i].vagas;
            int vagasAInscrever = (req->numAlunos < vagasDisponiveis)
                                  ? req->numAlunos : vagasDisponiveis;
            horarioP[i].vagas -= vagasAInscrever;
            alunosInscritos += vagasAInscrever;
            req->numAlunos -= vagasAInscrever;
        }
    }
    totalAlunosInscritos += alunosInscritos;
    pthread_mutex_unlock(&horarioP_mutex);

    // abrir o pipe do student
    int studentFd;
    int attempts = 0;
    while ((studentFd = open(req->studentPipe,
                             O_WRONLY | O_NONBLOCK)) == -1) {
        if (errno == ENXIO || errno == ENOENT) {
        
            attempts++;
            if (attempts >= 5) {
                perror("Erro ao abrir o named pipe do student");
                free(req);
                pthread_exit(NULL);
            }
            usleep(100000); 
        } else {
            perror("Erro ao abrir o named pipe do student");
            free(req);
            pthread_exit(NULL);
        }
    }

    // Escrever a resposta no pipe
    char resposta[BUFFER_SIZE];
    snprintf(resposta, BUFFER_SIZE, "%d", alunosInscritos);
    write(studentFd, resposta, strlen(resposta) + 1);
    close(studentFd);
    free(req);

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr,
                "Uso: %s <nalun> <NDISCIP> <NHOR> <NLUG> <PIPE_NAME>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    totalAlunos = atoi(argv[1]);
    NDISCIP = atoi(argv[2]);
    NHOR = atoi(argv[3]);
    NLUG = atoi(argv[4]);
    strncpy(SUPPORT_PIPE, argv[5], BUFFER_SIZE);

    horarioP = malloc(sizeof(HorarioP) * NDISCIP * NHOR);
    if (!horarioP) {
        perror("Erro ao alocar memória");
        exit(EXIT_FAILURE);
    }
    inicializarHorario();

    if (mkfifo(SUPPORT_PIPE, 0666) == -1 && errno != EEXIST) {
        perror("Erro ao criar o named pipe do suporte");
        exit(EXIT_FAILURE);
    }

    
    int supportFd = open(SUPPORT_PIPE, O_RDONLY | O_NONBLOCK);
    if (supportFd == -1) {
        perror("Erro ao abrir o named pipe do suporte");
        unlink(SUPPORT_PIPE);
        exit(EXIT_FAILURE);
    }

    printf("support_agent: Iniciado e pronto para receber pedidos.\n");

    ssize_t n;
    while (1) {
        char buffer[BUFFER_SIZE];
        int index = 0;

        // Read data from the pipe
        while ((n = read(supportFd, &buffer[index], 1)) > 0) {
            if (buffer[index] == '\0')
                break;
            index++;
        }

        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            
            usleep(100000); 
            continue;
        } else if (n == 0) {
            
            usleep(100000);
            continue;
        } else if (n == -1) {
            perror("Erro ao ler do named pipe do suporte");
            break;
        }

        buffer[index] = '\0';

        if (strcmp(buffer, "EXIT") == 0) {
            printf("support_agent: Sinal de término recebido. Encerrando.\n");
            break;
        }

        // processar a mensagem
        Pedido *req = malloc(sizeof(Pedido));
        if (sscanf(buffer, "%d %d %s",
                   &req->alunoInicial, &req->numAlunos,
                   req->studentPipe) != 3) {
            fprintf(stderr, "Mensagem inválida recebida: %s\n", buffer);
            free(req);
            continue;
        }

        pthread_t thread;
        pthread_create(&thread, NULL, processarPedido, req);

       
        pthread_mutex_lock(&threadList_mutex);
        ThreadNode *newNode = malloc(sizeof(ThreadNode));
        newNode->thread = thread;
        newNode->next = threadList;
        threadList = newNode;
        pthread_mutex_unlock(&threadList_mutex);
    }

    // Juntar as Therads antes de sair
    pthread_mutex_lock(&threadList_mutex);
    ThreadNode *current = threadList;
    while (current != NULL) {
        pthread_join(current->thread, NULL);
        ThreadNode *temp = current;
        current = current->next;
        free(temp);
    }
    pthread_mutex_unlock(&threadList_mutex);

    printf("Todos os alunos foram processados ou não há mais vagas.\n");
    printf("Estado final dos horários:\n");
    for (int i = 0; i < NDISCIP * NHOR; i++) {
        printf("Disciplina %d, Horário %d: Vagas restantes = %d\n",
               horarioP[i].disciplina, horarioP[i].horario,
               horarioP[i].vagas);
    }
    printf("Total de alunos inscritos: %d\n", totalAlunosInscritos);
    printf("Total de alunos não inscritos: %d\n",
           totalAlunos - totalAlunosInscritos);

    close(supportFd);
    unlink(SUPPORT_PIPE); // MANDAR O SUPPORT PIPE IR DAR UMA VOLTA AO BILHAR GRANDE
    free(horarioP);

    return 0;
}
