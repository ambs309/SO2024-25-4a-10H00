#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#define BUFFER_SIZE 256
#define PIPE_NAME_SIZE 100
#define MESSAGE_SIZE 256

int main(int argc, char *argv[]) {
    int nstud, aluno_inicial, num_alunos;
    char pipeAluno[PIPE_NAME_SIZE];
    char mensagem[MESSAGE_SIZE];
    char resposta[BUFFER_SIZE];
    int support_fd, student_fd;
    ssize_t bytesLidos;




    if (argc != 5) {
        fprintf(stderr, "Utilização: %s <nstud> <aluno_inicial> <num_alunos> <pipe_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }





    // Obter argumentos
    nstud = atoi(argv[1]);
    aluno_inicial = atoi(argv[2]);
    num_alunos = atoi(argv[3]);
    const char *support_pipe = argv[4];





    // Criar o nome do pipe do aluno
    snprintf(pipeAluno, PIPE_NAME_SIZE, "/tmp/student_%d", nstud);





    // Criar o named pipe do aluno
    if (mkfifo(pipeAluno, 0666) == -1) {
        perror("Erro ao criar o named pipe do estudante");
        exit(EXIT_FAILURE);
    }





    // Mensagem inicial
    printf("student %d: aluno inicial=%d, número de alunos=%d\n", nstud, aluno_inicial, num_alunos);





    // Abrir o named pipe do suporte para escrita (OPEN WRITE ONLY)
    support_fd = open(support_pipe, O_WRONLY);
    if (support_fd == -1) {
        perror("Erro ao abrir o named pipe do suporte");
        unlink(pipeAluno);
        exit(EXIT_FAILURE);
    }





    // Aqui a mensagem que é suposto ser enviada 
    snprintf(mensagem, MESSAGE_SIZE, "%d %d %s", aluno_inicial, num_alunos, pipeAluno);





    // Enviar a mensagem para o support_agent
    if (write(support_fd, mensagem, strlen(mensagem) + 1) == -1) {
        perror("Erro ao escrever no named pipe do suporte");
        close(support_fd);
        unlink(pipeAluno);
        exit(EXIT_FAILURE);
    }

    close(support_fd);





    // Abrir o named pipe do aluno para leitura
    student_fd = open(pipeAluno, O_RDONLY);
    if (student_fd == -1) {
        perror("Erro ao abrir o named pipe do aluno");
        unlink(pipeAluno);
        exit(EXIT_FAILURE);
    }






    // Ler a resposta do support_agent
    bytesLidos = read(student_fd, resposta, BUFFER_SIZE);
    if (bytesLidos == -1) {
        perror("Erro ao ler do named pipe do aluno");
        close(student_fd);
        unlink(pipeAluno);
        exit(EXIT_FAILURE);
    }

    resposta[bytesLidos] = '\0';

    int numeroAlunosInscritos = atoi(resposta);







    // Mensagem final
    printf("student %d: alunos inscritos=%d\n", nstud, numeroAlunosInscritos);

    close(student_fd);
    unlink(pipeAluno); // Remover o named pipe do aluno

    return 0;
}
