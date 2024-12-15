#!/bin/bash

# Verificação dos argumentos
if [[ $# -lt 4 ]]; then
    echo "Uso: $0 <NALUN> <NDISCIP> <NLUG> <NSTUD> [PIPE_NAME]"
    exit 1
fi

# Parâmetros iniciais
NALUN=$1                # Número total de alunos
NDISCIP=$2              # Número de disciplinas
NLUG=$3                 # Número de lugares por horário
NSTUD=$4                # Número de processos student
PIPE_NAME=${5:-/tmp/suporte}   # Pipe principal para comunicação com support_agent

# Limpeza de pipes antigos e processos anteriores
rm -f /tmp/student_* /tmp/admin /tmp/admin_resp $PIPE_NAME
pkill -f './student' 2>/dev/null
pkill -f './support_agent' 2>/dev/null

# Criação dos pipes necessários
mkfifo $PIPE_NAME
mkfifo /tmp/admin

# Iniciar o support_agent
echo "Iniciando o support_agent..."
./support_agent $NALUN $NDISCIP 1 $NLUG $PIPE_NAME &
AGENT_PID=$!

# Abrir terminal separado para o admin
echo "Iniciando o admin em um novo terminal..."
gnome-terminal --title="Admin" -- ./admin &

# Inicializar os processos student
ALUNO_INICIAL=0
declare -a STUDENT_PIDS

for (( i=1; i<=NSTUD; i++ )); do
    # Determinar o número de alunos por processo student
    NUM_ALUNOS=$((NALUN / NSTUD))
    if (( i <= NALUN % NSTUD )); then
        NUM_ALUNOS=$((NUM_ALUNOS + 1))
    fi

    echo "Iniciando student $i com $NUM_ALUNOS alunos a partir do aluno $ALUNO_INICIAL..."
    ./student $i $ALUNO_INICIAL $NUM_ALUNOS $PIPE_NAME &
    STUDENT_PIDS[$i]=$!
    ALUNO_INICIAL=$((ALUNO_INICIAL + NUM_ALUNOS))
done

# Aguardar a conclusão dos processos student
for pid in "${STUDENT_PIDS[@]}"; do
    if kill -0 $pid 2>/dev/null; then
        wait $pid 2>/dev/null || true
    fi
done

# Enviar sinal de término ao support_agent
echo "Enviando sinal de término ao support_agent..."
printf "3,/tmp/admin_resp" > /tmp/admin

# Aguardar a finalização do support_agent
wait $AGENT_PID

# Limpeza dos pipes após a execução
rm -f $PIPE_NAME /tmp/admin /tmp/admin_resp /tmp/student_*

echo "Execução concluída. Todos os recursos foram limpos."
