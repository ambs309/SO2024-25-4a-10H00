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
    int disciplina;    // ID da disciplina
    int horario;       // ID do horário
    int num_lugares;   // Número total de lugares
    int vagas;         // Número de vagas disponíveis
} HorarioP;






typedef struct {
    int aluno_inicial;
    int num_alunos;
    char student_pipe[BUFFER_SIZE];
} Pedido;





HorarioP *horárioP; // Vai ser alocado de maneira dinâmica
pthread_mutex_t horarioP_mutex = PTHREAD_MUTEX_INITIALIZER;
int numeroDeAlunosInscritos = 0;
int numeroMaxAlunos;
int nalun;  // Número de alunos que têm de reservar horário
int NDISCIP, NHOR, NLUG;
char SUPPORT_PIPE[BUFFER_SIZE];






void inicializarHorario() {
    int i = 0;
    for (int i = 0; i < NDISCIP; i++) {
        for (int j = 0; j < NHOR; j++) {
            horárioP[i].disciplina = i;
            horárioP[i].horario = j;
            horárioP[i].num_lugares = NLUG;
            horárioP[i].vagas = NLUG;
            i++;
        }
    }
}





void *processarPedido(void *arg) {//req = pedid
    Pedido *req = (Pedido *)arg;
    int alunos_a_inscrever = req->num_alunos;
    int alunos_inscritos = 0;

    pthread_mutex_lock(&horarioP_mutex);

    // Verificação das vagas disponíveis
    for (int i = 0; i < NDISCIP * NHOR && alunos_a_inscrever > 0; i++) {
        if (horárioP[i].vagas > 0) {
            int vagas_disponiveis = horárioP[i].vagas;
            int vagas_a_preencher = (alunos_a_inscrever < vagas_disponiveis) ? alunos_a_inscrever : vagas_disponiveis;

            horárioP[i].vagas -= vagas_a_preencher;
            alunos_inscritos += vagas_a_preencher;
            alunos_a_inscrever -= vagas_a_preencher;
            numeroDeAlunosInscritos += vagas_a_preencher;
        }
    }

    pthread_mutex_unlock(&horarioP_mutex);






    // Enviar a resposta para o Student
    int student_fd = open(req->student_pipe, O_WRONLY);
    if (student_fd == -1) {
        perror("Erro ao abrir o named pipe do student");
        free(req);
        pthread_exit(NULL);
    }

    char resposta[BUFFER_SIZE];
    snprintf(resposta, BUFFER_SIZE, "%d", alunos_inscritos);










    // só para garantir que o '\0' é mandado
    if (write(student_fd, resposta, strlen(resposta) + 1) == -1) {
        perror("Erro ao escrever no named pipe do student");
    }

    close(student_fd);
    free(req);

    pthread_exit(NULL);
}





int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Uso: %s <nalun> <NDISCIP> <NHOR> <NLUG> <PIPE_NAME>\n", argv[0]);
        exit(EXIT_FAILURE);
    }





    nalun = atoi(argv[1]);
    NDISCIP = atoi(argv[2]);
    NHOR = atoi(argv[3]);
    NLUG = atoi(argv[4]);
    strncpy(SUPPORT_PIPE, argv[5], BUFFER_SIZE);

    numeroMaxAlunos = NDISCIP * NHOR * NLUG;






    // Alocar memória para Horários
    horárioP = malloc(sizeof(HorarioP) * NDISCIP * NHOR);
    if (horárioP == NULL) {
        perror("Erro ao alocar memória para os Horários");
        exit(EXIT_FAILURE);
    }





    // Inicializar as estruturas de dados
    inicializarHorario();






    // Verificar se o named pipe já existe se não existir criamo-lo
    if (access(SUPPORT_PIPE, F_OK) == -1) {
        if (mkfifo(SUPPORT_PIPE, 0666) == -1) {
            perror("Erro ao criar o named pipe do suporte");
            exit(EXIT_FAILURE);
        }
    }







    // Abrir o named pipe para leitura (OPEN READ ONLY)
    int support_fd = open(SUPPORT_PIPE, O_RDONLY);
    if (support_fd == -1) {
        perror("Erro ao abrir o named pipe do suporte");
        unlink(SUPPORT_PIPE);
        exit(EXIT_FAILURE);
    }






    printf("support_agent: Iniciado e pronto para receber pedidos.\n");







    while (1) {
        // Aqui verificamos as condições para terminar
        pthread_mutex_lock(&horarioP_mutex);
        if (numeroDeAlunosInscritos >= nalun || numeroDeAlunosInscritos >= numeroMaxAlunos) {
            pthread_mutex_unlock(&horarioP_mutex);
            break;
        }
        pthread_mutex_unlock(&horarioP_mutex);

        // Ler a mensagem do named pipe
        char buffer[BUFFER_SIZE];
        int j = 0;
        ssize_t bytesLidos;

        // ler carateres até chegar ao '\0'
        pthread_mutex_lock(&horarioP_mutex); // Excluir outras threads de ler do pipe ao mesmo tempo
        while ((bytesLidos = read(support_fd, &buffer[j], 1)) > 0) {
            if (buffer[j] == '\0') {
                break;
            }
            j++;
            if (j >= BUFFER_SIZE - 1) {
                fprintf(stderr, "A mensagem é demasiado comprida.\n");
                break;
            }
        }
        pthread_mutex_unlock(&horarioP_mutex);

        if (bytesLidos == -1) {
            perror("Erro ao ler do named pipe do suporte");
            continue;
        } else if (bytesLidos == 0) {
    // Não há mais dados e não há mais writers, fechar o pipe e SAIR!
    close(support_fd);
    break;
}


        buffer[j] = '\0'; // Só para tera a certeza que a String acabou

        // Criar uma nova estrutura de pedido
        Pedido *req = malloc(sizeof(Pedido));
        if (req == NULL) {
            perror("Erro ao alocar memória para o pedido");
            continue;
        }

        // Analisar a mensagem
        if (sscanf(buffer, "%d %d %s", &req->aluno_inicial, &req->num_alunos, req->student_pipe) != 3) {
            fprintf(stderr, "Mensagem inválida recebida: %s\n", buffer);
            free(req);
            continue;
        }





        // Criar uma thread para processar o pedido
        pthread_t thread;
        if (pthread_create(&thread, NULL, processarPedido, req) != 0) {
            perror("Erro ao criar a thread");
            free(req);
            continue;
        }




        // Desvincular a thread para que os seus recursos sejam liberados automaticamente
        pthread_detach(thread);
    }

    close(support_fd);
    unlink(SUPPORT_PIPE);

    printf("support_agent: Todos os alunos foram inscritos ou não há mais vagas.\n");






    // Imprimir o estado final dos horários
    printf("Estado final dos horários:\n");
    for (int i = 0; i < NDISCIP * NHOR; i++) {
        printf("Disciplina %d, Horário %d: Vagas restantes = %d\n",
               horárioP[i].disciplina,
               horárioP[i].horario,
               horárioP[i].vagas);
    }






    // AQUI SIM! "Limpamos a casa"
    free(horárioP);
    pthread_mutex_destroy(&horarioP_mutex);

    return 0;
}
