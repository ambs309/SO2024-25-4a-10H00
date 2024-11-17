#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#define BUFFER_SIZE 256
#define PIPE_NAME_SIZE 100
#define MESSAGE_SIZE 256

char pipeAluno[PIPE_NAME_SIZE]; // Nome do named pipe do aluno

void cleanup(int signum) {
    unlink(pipeAluno); // Remover o named pipe do aluno
    exit(0);           // Sair do programa
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <nstud> <aluno_inicial> <num_alunos> <pipe_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int nstud = atoi(argv[1]);
    int alunoInicial = atoi(argv[2]);
    int numAlunos = atoi(argv[3]);
    const char *supportPipe = argv[4];

    snprintf(pipeAluno, PIPE_NAME_SIZE, "/tmp/student_%d", nstud);

    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    unlink(pipeAluno);

    if (mkfifo(pipeAluno, 0666) == -1 && errno != EEXIST) {
        perror("Erro ao criar o named pipe do estudante");
        exit(EXIT_FAILURE);
    }

    printf("student %d: aluno inicial=%d, número de alunos=%d\n", nstud, alunoInicial, numAlunos);

    int supportFd = open(supportPipe, O_WRONLY);
    if (supportFd == -1) {
        perror("Erro ao abrir o named pipe do suporte");
        unlink(pipeAluno);
        exit(EXIT_FAILURE);
    }

    char mensagem[MESSAGE_SIZE];
    snprintf(mensagem, MESSAGE_SIZE, "%d %d %s", alunoInicial, numAlunos, pipeAluno);

    if (write(supportFd, mensagem, strlen(mensagem) + 1) == -1) {
        perror("Erro ao escrever no named pipe do suporte");
        close(supportFd);
        unlink(pipeAluno);
        exit(EXIT_FAILURE);
    }

    close(supportFd);

    int studentFd = open(pipeAluno, O_RDONLY);
    if (studentFd == -1) {
        perror("Erro ao abrir o named pipe do aluno");
        unlink(pipeAluno);
        exit(EXIT_FAILURE);
    }

    char resposta[BUFFER_SIZE];
    ssize_t bytesLidos = read(studentFd, resposta, BUFFER_SIZE - 1);
    if (bytesLidos == -1) {
        perror("Erro ao ler do named pipe do aluno");
        close(studentFd);
        unlink(pipeAluno);
        exit(EXIT_FAILURE);
    }

    resposta[bytesLidos] = '\0';

    int alunosInscritos = atoi(resposta);
    printf("student %d: alunos inscritos=%d\n", nstud, alunosInscritos);

    close(studentFd);
    unlink(pipeAluno);

    return 0;
}

