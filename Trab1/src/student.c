#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> 
#include <unistd.h> 


//As variáveis pipe_name e a message vão agora ser os argumentos passados
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <nome do pipe> <mensagem>\n", argv[0]);
       exit(EXIT_FAILURE);
    }
    //este if serve para perceber se se introduziram de facto os argumentos em condições,caso
    //tal aconteça o programa termina (EXIT_FAILURE), antes disso imprime ainda a mensagem que ali está
    //escrita no fprintf...

    char *pipe_name = argv[1];
    char *message = argv[2];

    int pipe_fd = open(pipe_name, O_WRONLY);//Aqui abre-se o pipe para escrita
    if (pipe_fd == -1) {//++
        perror("Erro ao abrir o pipe");//este erro basicamente nao acontece mas olha fica aqui
        exit(EXIT_FAILURE);//++
    }

    // Escreve a mensagem no pipe
    if (write(pipe_fd, message, strlen(message)) == -1) {
        perror("Erro ao escrever no pipe");
        close(pipe_fd);
        exit(EXIT_FAILURE);
    }
    printf("Mensagem '%s' enviada ao pipe '%s'.\n", message, pipe_name);
    //finalmente recebemos a impressão da mensagem na qual está identificado
    // quem a mandou e que pipe a recebeu
    // Fecha o pipe
    close(pipe_fd);

    return 0;
}