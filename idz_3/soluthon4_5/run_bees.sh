#!/bin/bash

if [ $# -lt 3 ]; then
    echo "Использование: $0 <количество_стай> <IP> <PORT>"
    exit 1
fi

NUM_BEES=$1
IP=$2
PORT=$3

echo "Запускаем $NUM_BEES стай пчёл..."

for ((i=1; i<=$NUM_BEES; i++))
do
    echo "Запуск стаи #$i"
    ./client $i $IP $PORT &
    sleep 0.5
done

echo "Все стаи запущены! Ждём завершения работы..."
wait
echo "Все стаи вернулись в улей."
