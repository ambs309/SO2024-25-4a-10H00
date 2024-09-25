#!/usr/bin/bash

#scrpt0: echo, variáveis, execução decomandos

#0.1 echo
echo "Hello World!" # Hello World!

s="Hellow World 2!"
echo $s

# 0.2 2 execução de comandos
pwd

cmd="ls"
$cmd

#0.3 Built-in Variables

echo "Number of arguments passed to Script: $#"
echo "All arguments passed to script: $s"
echo "Scrpt's arguments separated info different variables: $1 $2..."
echo "Last Program's return value: $?"
echo "Script's PID: $$"

#0.4 Reading a value from input:

echo "What's your name?"
read name
echo "Hello, $name!"

echo "comando a executar ?"
read command
$command

#0.5 mkdir

echo "Diretório a criar"
read dir
mkdir temp
mkdir temp/$dir
ls temp

#0.6 mv

echo "Novo nome do diretório"
read novodir
mv temp/$dir temp/$novodir
ls temp


