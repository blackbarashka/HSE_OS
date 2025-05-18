#!/bin/bash

if [ $# -lt 3 ]; then
    echo "Использование: $0 <количество_стай> <IP> <PORT>"
    exit 1
fi

NUM_BEES=$1
IP=$2
PORT=$3

# Запускаем наблюдателя в отдельном терминале, если возможно
if command -v xterm &>/dev/null; then
    xterm -title "Наблюдатель" -e "./observer $IP $PORT" &
elif command -v gnome-terminal &>/dev/null; then
    gnome-terminal -- ./observer $IP $PORT &
else
    echo "ВНИМАНИЕ: Не найден подходящий терминал для запуска наблюдателя в отдельном окне."
    echo "Запускаем наблюдателя в фоне."
    ./observer $IP $PORT &
fi

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
