#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>

#define BUFFER_SIZE 256
#define ADMIN_PIPE "/tmp/admin"

typedef struct {
    int disciplina;
    int horario;
    int numLugares;
    int vagas;
    int *alunosInscritos;
    int numInscritos;
} HorarioP;

typedef struct {
    int alunoID;
    int disciplinas[10]; // -1 se não inscrito, caso contrário horário
} AlunoInfo;

static HorarioP *horarioP;
static AlunoInfo *alunos;

static pthread_mutex_t horarioP_mutex = PTHREAD_MUTEX_INITIALIZER;

static int NDISCIP, NHOR, NLUG;
static int totalAlunos;
static char SUPPORT_PIPE[BUFFER_SIZE];
static int running = 1; // usado para terminar o loop quando pedido terminar

// Função para inicializar a estrutura de horários
void inicializarHorario() {
    for (int i = 0; i < NDISCIP * NHOR; i++) {
        horarioP[i].disciplina = i / NHOR;
        horarioP[i].horario = i % NHOR;
        horarioP[i].numLugares = NLUG;
        horarioP[i].vagas = NLUG;
        horarioP[i].numInscritos = 0;
        horarioP[i].alunosInscritos = malloc(sizeof(int)*NLUG);
        for (int v = 0; v < NLUG; v++) {
            horarioP[i].alunosInscritos[v] = -1;
        }
    }
}

// Função auxiliar para inscrever aluno numa disciplina
// Retorna o horário onde inscreveu ou -1 se não há vagas
int inscrever_aluno(int alunoID, int disciplina) {
    // procurar um horário da disciplina com vaga
    for (int i = 0; i < NDISCIP * NHOR; i++) {
        if (horarioP[i].disciplina == disciplina && horarioP[i].vagas > 0) {
            // Inscrever neste horário
            horarioP[i].vagas--;
            // Encontrar posição livre
            for (int p = 0; p < horarioP[i].numLugares; p++) {
                if (horarioP[i].alunosInscritos[p] == -1) {
                    horarioP[i].alunosInscritos[p] = alunoID;
                    horarioP[i].numInscritos++;
                    break;
                }
            }
            return horarioP[i].horario;
        }
    }
    return -1;
}

int gravarEmFicheiro(const char *nome_ficheiro) {
    FILE *f = fopen(nome_ficheiro, "w");
    if (!f) return -1;

    fprintf(f, "#aluno,d0,d1,d2,d3,d4,d5,d6,d7,d8,d9\n");
    int count = 0;
    pthread_mutex_lock(&horarioP_mutex);
    for (int aluno = 0; aluno < totalAlunos; aluno++) {
        // verificar se inscrito em pelo menos 1 disciplina
        int inscrito = 0;
        for (int d = 0; d < 10; d++) {
            if (alunos[aluno].disciplinas[d] != -1) {
                inscrito = 1;
                break;
            }
        }
        if (!inscrito) continue;

        fprintf(f, "%d", aluno);
        for (int d = 0; d < 10; d++) {
            if (alunos[aluno].disciplinas[d] != -1)
                fprintf(f, ",%d", alunos[aluno].disciplinas[d]);
            else
                fprintf(f, ",");
        }
        fprintf(f, "\n");
        count++;
    }
    pthread_mutex_unlock(&horarioP_mutex);

    fclose(f);
    return count;
}

// Thread para ler pedidos de students (no pipe SUPPORT_PIPE)
void* student_thread_func(void *arg) {
    int supportFd = open(SUPPORT_PIPE, O_RDONLY | O_NONBLOCK);
    if (supportFd == -1) {
        perror("Erro ao abrir o named pipe do suporte");
        running = 0;
        return NULL;
    }

    while (running) {
        char buffer[BUFFER_SIZE];
        ssize_t n;
        int index = 0;

        // Ler uma mensagem completa (até '\0')
        while ((n = read(supportFd, &buffer[index], 1)) > 0) {
            if (buffer[index] == '\0')
                break;
            index++;
            if (index >= BUFFER_SIZE-1) break; // evitar overflow
        }

        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // nada para ler
            usleep(100000);
            continue;
        } else if (n == 0) {
            // pipe sem dados
            usleep(100000);
            continue;
        } else if (n == -1) {
            perror("Erro ao ler do pipe suporte");
            break;
        }

        buffer[index] = '\0';

        // Formato do pedido do student: "num_aluno,disciplina,pipe_resposta"
        // Se for "EXIT" ignorar aqui, pois EXIT é admin
        if (strcmp(buffer, "EXIT") == 0) {
            // não faz sentido aqui
            continue;
        }

        int num_aluno, disciplina;
        char pipe_resp[BUFFER_SIZE];

        if (sscanf(buffer, "%d,%d,%s", &num_aluno, &disciplina, pipe_resp) != 3) {
            fprintf(stderr, "Mensagem inválida: %s\n", buffer);
            continue;
        }

        pthread_mutex_lock(&horarioP_mutex);
        // Inscrever aluno na disciplina
        int horario_insc = inscrever_aluno(num_aluno, disciplina);
        // guardar no array global de alunos
        if (horario_insc != -1) {
            alunos[num_aluno].disciplinas[disciplina] = horario_insc;
        } else {
            alunos[num_aluno].disciplinas[disciplina] = -1;
        }
        pthread_mutex_unlock(&horarioP_mutex);

        // enviar resposta
        int fd = open(pipe_resp, O_WRONLY | O_NONBLOCK);
        if (fd == -1) {
            int attempts = 0;
            while ((fd = open(pipe_resp, O_WRONLY | O_NONBLOCK)) == -1) {
                if (errno == ENXIO || errno == ENOENT) {
                    attempts++;
                    if (attempts >= 5) {
                        perror("Erro ao abrir pipe de resposta do student");
                        break;
                    }
                    usleep(100000);
                } else {
                    perror("Erro ao abrir pipe de resposta do student");
                    break;
                }
            }
        }

        if (fd != -1) {
            char resposta[50];
            snprintf(resposta, 50, "%d", horario_insc);
            write(fd, resposta, strlen(resposta)+1);
            close(fd);
        }
    }

    close(supportFd);
    return NULL;
}

// Thread para ler pedidos do admin (no pipe ADMIN_PIPE)
void* admin_thread_func(void *arg) {
    // Criar o pipe admin se não existir
    unlink(ADMIN_PIPE);
    if (mkfifo(ADMIN_PIPE, 0666) == -1 && errno != EEXIST) {
        perror("Erro ao criar /tmp/admin");
        return NULL;
    }

    int adminFd = open(ADMIN_PIPE, O_RDONLY | O_NONBLOCK);
    if (adminFd == -1) {
        perror("Erro ao abrir /tmp/admin");
        return NULL;
    }

    while (running) {
        char buffer[BUFFER_SIZE];
        ssize_t n;
        int index = 0;

        while ((n = read(adminFd, &buffer[index], 1)) > 0) {
            if (buffer[index] == '\0')
                break;
            index++;
            if (index >= BUFFER_SIZE -1) break;
        }

        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            usleep(100000);
            continue;
        } else if (n == 0) {
            usleep(100000);
            continue;
        } else if (n == -1) {
            perror("Erro ao ler do /tmp/admin");
            break;
        }

        buffer[index] = '\0';

        // Pedidos do admin:
        // codop=1: "1,<num_aluno>,<pipe_resposta>"
        // codop=2: "2,<nome_ficheiro>,<pipe_resposta>"
        // codop=3: "3,<pipe_resposta>"
        

        char *token = strtok(buffer, ",");
        if (!token) continue;
        int codop = atoi(token);

        if (codop == 1) {
            token = strtok(NULL, ",");
            if (!token) continue;
            int num_aluno = atoi(token);
            token = strtok(NULL, ",");
            if (!token) continue;
            char *pipe_resp = token;

            pthread_mutex_lock(&horarioP_mutex);
            char resposta[BUFFER_SIZE];
            snprintf(resposta, BUFFER_SIZE, "%d", num_aluno);
            for (int d = 0; d < 10; d++) {
                if (alunos[num_aluno].disciplinas[d] != -1) {
                    char temp[50];
                    snprintf(temp, 50, ",%d/%d", d, alunos[num_aluno].disciplinas[d]);
                    strcat(resposta, temp);
                }
            }
            pthread_mutex_unlock(&horarioP_mutex);

            int fd = open(pipe_resp, O_WRONLY);
            if (fd != -1) {
                write(fd, resposta, strlen(resposta)+1);
                close(fd);
            }

        } else if (codop == 2) {
            token = strtok(NULL, ",");
            if (!token) continue;
            char *nome_ficheiro = token;
            token = strtok(NULL, ",");
            if (!token) continue;
            char *pipe_resp = token;

            int res = gravarEmFicheiro(nome_ficheiro);

            int fd = open(pipe_resp, O_WRONLY);
            if (fd != -1) {
                char resposta[50];
                snprintf(resposta, 50, "%d", res);
                write(fd, resposta, strlen(resposta)+1);
                close(fd);
            }

        } else if (codop == 3) {
            token = strtok(NULL, ",");
            if (!token) continue;
            char *pipe_resp = token;

            int fd = open(pipe_resp, O_WRONLY);
            if (fd != -1) {
                write(fd, "Ok", 3);
                close(fd);
            }

            // Terminar o agent
            running = 0;
        }
    }

    close(adminFd);
    unlink(ADMIN_PIPE);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr,"Uso: %s <NALUN> <NDISCIP> <NHOR> <NLUG> <PIPE_NAME>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    totalAlunos = atoi(argv[1]);
    NDISCIP = atoi(argv[2]);
    NHOR = atoi(argv[3]);
    NLUG = atoi(argv[4]);
    strncpy(SUPPORT_PIPE, argv[5], BUFFER_SIZE);

    // Alocar estrutura de horarios
    horarioP = malloc(sizeof(HorarioP)*NDISCIP*NHOR);
    if (!horarioP) {
        perror("Erro ao alocar memória para horarios");
        exit(EXIT_FAILURE);
    }

    // Alocar estrutura para alunos
    alunos = malloc(sizeof(AlunoInfo)*totalAlunos);
    if (!alunos) {
        perror("Erro ao alocar memória para alunos");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < totalAlunos; i++) {
        alunos[i].alunoID = i;
        for (int d = 0; d < 10; d++) {
            alunos[i].disciplinas[d] = -1;
        }
    }

    inicializarHorario();

    unlink(SUPPORT_PIPE);
    if (mkfifo(SUPPORT_PIPE, 0666) == -1 && errno != EEXIST) {
        perror("Erro ao criar pipe do suporte");
        exit(EXIT_FAILURE);
    }

    printf("support_agent: Iniciado e pronto para receber pedidos.\n");

    
    pthread_t th_student, th_admin;
    pthread_create(&th_student, NULL, student_thread_func, NULL);
    pthread_create(&th_admin, NULL, admin_thread_func, NULL);

    // Esperar terminar
    pthread_join(th_student, NULL);
    pthread_join(th_admin, NULL);

    // Imprimir estado final
    printf("Estado final dos horários:\n");
    for (int i = 0; i < NDISCIP * NHOR; i++) {
        printf("Disciplina %d, Horário %d: Vagas restantes = %d, Inscritos = %d\n",
               horarioP[i].disciplina, horarioP[i].horario,
               horarioP[i].vagas, horarioP[i].numInscritos);
    }

    // Calcular quantos alunos se inscreveram em pelo menos uma disciplina
    int inscritos = 0;
    for (int a = 0; a < totalAlunos; a++) {
        for (int d = 0; d < 10; d++) {
            if (alunos[a].disciplinas[d] != -1) {
                inscritos++;
                break;
            }
        }
    }

    printf("Total de alunos inscritos: %d\n", inscritos);
    printf("Total de alunos não inscritos: %d\n", totalAlunos - inscritos);

    // Cleanup
    for (int i = 0; i < NDISCIP * NHOR; i++) {
        free(horarioP[i].alunosInscritos);
    }
    free(horarioP);
    free(alunos);
    unlink(SUPPORT_PIPE);

    return 0;
}
