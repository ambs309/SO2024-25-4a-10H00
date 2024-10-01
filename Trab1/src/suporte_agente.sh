#!/bin/bash

# será que passaram o argumento do nome do pipe??
if [ "$#" -lt 1 ]; then
  echo "Uso: $0 <nome do named pipe>"
  exit 1
fi
PIPE_NAME=$1

# Espera o named pipe ser criado, isto claro caso não exista um de antemão
while [ ! -p "$PIPE_NAME" ]; do
  echo "Aguardando a criação do named pipe '$PIPE_NAME'..."
  sleep 1
done

echo "Named pipe '$PIPE_NAME' encontrado. Ativar o suporte agente!"

# Ciclo para processar pedidos do named pipe
while true; do
  # Lê do pipe as mensagens recebidas, quando não é quit começamos a tratar do pedido,
  #se for quit, acaba o suporte_agente shell.
  if read MESSAGE < "$PIPE_NAME"; then
    if [ "$MESSAGE" == "quit" ]; then
      echo "Suporte de agente vai terminar!"
      break
    else
      echo "A tratar do pedido: $MESSAGE"
    fi
    # agora, antes do próximo pedido vai esperar entre 1 a 5 segundos,
    # os valores são obtidos de modo COMPLETAMENTE arbitrário devido ao random ali em baixo
    SLEEP_TIME=$((1 + RANDOM % 5))
    sleep "$SLEEP_TIME"
  fi
done

echo "Suporte agente terminado"