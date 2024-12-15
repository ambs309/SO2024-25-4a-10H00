#!/bin/bash

if [[ $# -lt 4 ]]; then
    echo "Uso: $0 NALUN NDISCIP NLUG NSTUD [PIPE_NAME]"
    exit 1
fi

NALUN=$1
NDISCIP=$2
NLUG=$3
NSTUD=$4
PIPE_NAME=${5:-/tmp/suporte}

rm -f /tmp/student_* $PIPE_NAME
pkill -f './student' 2>/dev/null
pkill -f './support_agent' 2>/dev/null

mkfifo $PIPE_NAME
./support_agent $NALUN $NDISCIP 1 $NLUG $PIPE_NAME &
AGENT_PID=$!

ALUNO_INICIAL=0
declare -a STUDENT_PIDS

for (( i=1; i<=NSTUD; i++ )); do
    NUM_ALUNOS=$((NALUN / NSTUD))
    if (( i <= NALUN % NSTUD )); then
        NUM_ALUNOS=$((NUM_ALUNOS + 1))
    fi
    ./student $i $ALUNO_INICIAL $NUM_ALUNOS $PIPE_NAME &
    STUDENT_PIDS[$i]=$! 
    ALUNO_INICIAL=$((ALUNO_INICIAL + NUM_ALUNOS))
done


for pid in "${STUDENT_PIDS[@]}"; do
    if kill -0 $pid 2>/dev/null; then
        wait $pid 2>/dev/null || true
    fi
done

# mandar o sinal de trmino ao suporte agente
printf "EXIT\0" > $PIPE_NAME

wait $AGENT_PID # esperamos que o nosso amigo suporte agente acabe

rm -f $PIPE_NAME