#!/bin/bash

if [ $# -lt 3 ]; then
    echo "Использование: $0 <количество_стай> <IP> <PORT> [количество_наблюдателей=1]"
    exit 1
fi

NUM_BEES=$1
IP=$2
PORT=$3
NUM_OBSERVERS=${4:-1}

echo "Запускаем $NUM_OBSERVERS наблюдателей..."

for ((i=1; i<=$NUM_OBSERVERS; i++))
do
    if command -v xterm &>/dev/null; then
        xterm -title "Наблюдатель #$i" -e "./observer $IP $PORT" &
    elif command -v gnome-terminal &>/dev/null; then
        gnome-terminal -- bash -c "./observer $IP $PORT" &
    else
        echo "ВНИМАНИЕ: Не найден подходящий терминальный эмулятор."
        echo "Запускаем наблюдателя #$i в фоновом режиме."
        ./observer $IP $PORT &
    fi
    echo "Запущен наблюдатель #$i"
    sleep 0.5
done

echo "Запускаем $NUM_BEES стай пчёл..."

for ((i=1; i<=$NUM_BEES; i++))
do
    echo "Запуск стаи #$i"
    ./client $i $IP $PORT &
    sleep 0.5
done

echo -e "\nВсе стаи запущены!"
echo -e "- Для отключения стаи нажмите Ctrl+C в окне стаи"
echo -e "- Можно запустить новые стаи командой: ./client <id> $IP $PORT"
echo -e "- Или использовать менеджер стай: ./bee_manager $IP $PORT"

echo -e "\nЖдем завершения всех клиентских процессов..."
wait
echo "Все стаи вернулись в улей."
