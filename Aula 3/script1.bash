#1/usr/bin/bash

#script1: if's

echo "Nome"
read nome
if ([ "$nome" == "$1" ]); then
    echo "OK: $nome = $1"
    else
    echo "NOK: $nome != $1"
    fi

echo "Nome"
read nome
echo "Idade"
read idade

if ([ "$nome" == "$1" ]) && ([ "$idade" == "$1" ]); then
    echo "OK: $nome = $1 $idade = $1"
    else
    echo "NOK: $nome != $1 $idade != $1"
    fi


