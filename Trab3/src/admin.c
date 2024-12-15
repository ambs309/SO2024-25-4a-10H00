#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define BUFFER_SIZE 256

void enviarPedido(const char *pipeAdmin, const char *mensagem, const char *pipeResposta) {
    int adminFd = open(pipeAdmin, O_WRONLY);
    if (adminFd == -1) {
        perror("Erro ao abrir pipe admin");
        exit(EXIT_FAILURE);
    }

    write(adminFd, mensagem, strlen(mensagem) + 1);
    close(adminFd);

    int respostaFd = open(pipeResposta, O_RDONLY);
    if (respostaFd == -1) {
        perror("Erro ao abrir pipe resposta");
        exit(EXIT_FAILURE);
    }

    char resposta[BUFFER_SIZE];
    read(respostaFd, resposta, BUFFER_SIZE);
    printf("Resposta: %s\n", resposta);
    close(respostaFd);
}

int main() {
    const char *pipeAdmin = "/tmp/admin";
    const char *pipeResposta = "/tmp/admin_resp";
    unlink(pipeResposta);
    mkfifo(pipeResposta, 0666);

    while (1) {
        printf("\nMenu:\n1. Pedir Horários\n2. Gravar em Ficheiro\n3. Terminar\nOpção: ");
        int opcao;
        scanf("%d", &opcao);

        char mensagem[BUFFER_SIZE];
        if (opcao == 1) {
            int numAluno;
            printf("Número do aluno: ");
            scanf("%d", &numAluno);
            snprintf(mensagem, BUFFER_SIZE, "1,%d,%s", numAluno, pipeResposta);
        } else if (opcao == 2) {
            char ficheiro[BUFFER_SIZE];
            printf("Nome do ficheiro: ");
            scanf("%s", ficheiro);
            snprintf(mensagem, BUFFER_SIZE, "2,%s,%s", ficheiro, pipeResposta);
        } else if (opcao == 3) {
            snprintf(mensagem, BUFFER_SIZE, "3,%s", pipeResposta);
            enviarPedido(pipeAdmin, mensagem, pipeResposta);
            printf("Admin: Terminado.\n");
            break;
        } else {
            printf("Opção inválida!\n");
            continue;
        }

        enviarPedido(pipeAdmin, mensagem, pipeResposta);
    }

    unlink(pipeResposta);
    return 0;
}
