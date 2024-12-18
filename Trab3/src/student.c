#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#define BUFFER_SIZE 256
#define PIPE_NAME_SIZE 100

char pipeAluno[PIPE_NAME_SIZE];

void cleanup(int signum) {
    unlink(pipeAluno);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <nstud> <aluno_inicial> <num_alunos> <pipe_suporte>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int nstud = atoi(argv[1]);
    int alunoInicial = atoi(argv[2]);
    int numAlunos = atoi(argv[3]);
    const char *pipeSuporte = argv[4];

    snprintf(pipeAluno, PIPE_NAME_SIZE, "/tmp/student_%d", nstud);
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    unlink(pipeAluno);

    if (mkfifo(pipeAluno, 0666) == -1) {
        perror("Erro ao criar pipe aluno");
        exit(EXIT_FAILURE);
    }

    printf("student %d: aluno inicial=%d, número de alunos=%d\n", nstud, alunoInicial, numAlunos);
    srand(time(NULL));

    int suporteFd = open(pipeSuporte, O_WRONLY);
    if (suporteFd == -1) {
        perror("Erro ao abrir pipe suporte");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < numAlunos; i++) {
        int aluno = alunoInicial + i;
        char respostaFinal[BUFFER_SIZE] = "";

        for (int j = 0; j < 5; j++) { // Cada aluno inscreve-se em 5 disciplinas
            int disciplina = rand() % 10; // 10 disciplinas: 0 a 9
            char mensagem[BUFFER_SIZE];

            snprintf(mensagem, BUFFER_SIZE, "%d,%d,%s", aluno, disciplina, pipeAluno);
            write(suporteFd, mensagem, strlen(mensagem) + 1);

            // Receber resposta
            int alunoFd = open(pipeAluno, O_RDONLY);
            if (alunoFd == -1) {
                perror("Erro ao abrir pipe aluno");
                continue;
            }

            char resposta[BUFFER_SIZE];
            read(alunoFd, resposta, BUFFER_SIZE);
            close(alunoFd);

            // Adicionar disciplina/horário ao buffer
            char temp[50];
            snprintf(temp, sizeof(temp), "%s%d/%s", (j > 0 ? ", " : ""), disciplina, resposta);
            strncat(respostaFinal, temp, sizeof(respostaFinal) - strlen(respostaFinal) - 1);
        }

        // Imprime resultado acumulado para o aluno
        printf("student %d, aluno %d: %s\n", nstud, aluno, respostaFinal);
    }

    close(suporteFd);
    unlink(pipeAluno);
    return 0;
}
