#!/bin/bash

# Verifica se o número correto de argumentos foi passado
if [ "$#" -lt 2 ]; then
  echo "Uso: $0 <número de students> <nome do named pipe>"
  exit 1
fi

NUM_STUDENTS=$1
PIPE_NAME=$2

# Verifica se o named pipe já existe,
# mais uma vez tal como no outro ficheiro shell caso este já não exista é criado um
if [ ! -p "$PIPE_NAME" ]; then
  mkfifo "$PIPE_NAME" #criacao desse pipe nesta linha com o comando mkfifo(make FIFO(1st in 1st out, como manda a lei))
  echo "criou-se '$PIPE_NAME'"
else
  echo "Pelos vistos este named pipe já existia... '$PIPE_NAME' "
fi

# Aqui é que executamos o suporte_agente em background, passamos como argumentos o nome do pipe
./suporte_agente.sh "$PIPE_NAME" &
AGENT_PID=$!

# Executa os students em background
for (( i=1; i<=$NUM_STUDENTS; i++ )); do
  MESSAGE="Mensagem do estudante $i"
  ./student "$PIPE_NAME" "$MESSAGE" &
done

# Espera 1 segundo antes de enviar o comando de saída
sleep 1
echo "quit" > "$PIPE_NAME"

# Espera todos os processos terminarem
wait

# por fim fazemos rm(remove) ao named pipe
rm "$PIPE_NAME"
echo "Este named pipe foi removido'$PIPE_NAME'"
