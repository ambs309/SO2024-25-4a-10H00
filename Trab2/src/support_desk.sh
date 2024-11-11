#!/bin/bash

# Verificar o número de argumentos
if [[ $# -lt 4 ]]; then
    echo "Uso: $0 NALUN NDISCIP NLUG NSTUD [PIPE_NAME]"
    exit 1
fi

# Obter argumentos
NALUN=$1
NDISCIP=$2
NLUG=$3
NSTUD=$4

# O Nome do named pipe (pode ser passado como argumento OPCIONAL!)
if [[ $# -ge 5 ]]; then
    PIPE_NAME=$5
else
    PIPE_NAME="/tmp/suporte"
fi







verificaNumeroPositivoInt() {
    [[ $1 =~ ^[1-9][0-9]*$ ]]
}




# Verificar se NALUN, NDISCIP, NLUG e NSTUD são inteiros positivos
if ! verificaNumeroPositivoInt "$NALUN"; 
then
    echo "Erro: NALUN ('$NALUN') deve ser um inteiro positivo."
    exit 1
fi

if ! verificaNumeroPositivoInt "$NDISCIP"; 
then
    echo "Erro: NDISCIP ('$NDISCIP') deve ser um inteiro positivo."
    exit 1
fi

if ! verificaNumeroPositivoInt "$NLUG"; 
then
    echo "Erro: NLUG ('$NLUG') deve ser um inteiro positivo."
    exit 1
fi

if ! verificaNumeroPositivoInt "$NSTUD"; 
then
    echo "Erro: NSTUD ('$NSTUD') deve ser um inteiro positivo."
    exit 1
fi




# Verificar se NALUN é maior ou igual ao número total de lugares disponíveis
TOTAL_CAPACIDADE=$(( NDISCIP * NLUG ))
if (( NALUN > TOTAL_CAPACIDADE )); 
then
    echo "Aviso: O número total de alunos (NALUN=$NALUN) excede a capacidade total disponível ($TOTAL_CAPACIDADE)."
    echo "Alguns alunos não poderão ser inscritos."
fi



# Calcular o número mínimo de horários (NHOR)
NHOR=$(( (NALUN + NDISCIP * NLUG - 1) / (NDISCIP * NLUG) ))
if (( NHOR < 1 )); 
then
    NHOR=1
fi



# Imprimir informações iniciais
echo "Número de alunos (NALUN): $NALUN"
echo "Número de disciplinas (NDISCIP): $NDISCIP"
echo "Número de lugares por sala (NLUG): $NLUG"
echo "Número de horários calculado (NHOR): $NHOR"
echo "Número de processos student (NSTUD): $NSTUD"
echo "Named pipe: $PIPE_NAME"




# Criar o named pipe apenas se ainda não existir
if [[ ! -p $PIPE_NAME ]]; 
then
    mkfifo $PIPE_NAME
    echo "Named pipe $PIPE_NAME criado."
else
    echo "Named pipe $PIPE_NAME já existe."
fi




# Executar o support_agent em background para além disso passamos NALUN e o nome do pipe como argumentos
./support_agent $NALUN $NDISCIP $NHOR $NLUG $PIPE_NAME &
AGENT_PID=$!
echo "support_agent iniciado com PID $AGENT_PID."




# Calcular o número de alunos por processo student
ALUNOS_POR_STUDENT=$(( NALUN / NSTUD ))
RESTO=$(( NALUN % NSTUD ))



# Iniciar os processos student
ALUNO_INICIAL=0
declare -a STUDENT_PIDS



for (( i=1; i<=NSTUD; i++ ))
do
    NUM_ALUNOS=$ALUNOS_POR_STUDENT
    if [[ $i -le $RESTO ]]; 
    then
        NUM_ALUNOS=$(( NUM_ALUNOS + 1 ))
    fi

    # Executar student passando: número do student, aluno inicial, número de alunos, e o nome do pipe
    ./student $i $ALUNO_INICIAL $NUM_ALUNOS $PIPE_NAME &
    STUDENT_PIDS[$i]=$!

    echo "Student $i iniciado com PID ${STUDENT_PIDS[$i]} (aluno inicial=$ALUNO_INICIAL, num_alunos=$NUM_ALUNOS)."

    ALUNO_INICIAL=$(( ALUNO_INICIAL + NUM_ALUNOS ))
done




# Esperar que todos os processos terminem
wait $AGENT_PID

for PID in "${STUDENT_PIDS[@]}"
do
    wait $PID
done
echo "Todos os processos terminaram."




# Remover o named pipe
rm $PIPE_NAME
echo "Named pipe $PIPE_NAME removido."

exit 0
