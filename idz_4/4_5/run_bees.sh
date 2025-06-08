#!/bin/bash

# Параметры по умолчанию
SERVER_IP="127.0.0.1"
SERVER_PORT="8080"
NUM_SWARMS=5
CLIENT_PROGRAM="client"

# Проверка аргументов командной строки
if [ "$#" -ge 1 ]; then
    NUM_SWARMS=$1
fi

if [ "$#" -ge 2 ]; then
    SERVER_IP=$2
fi

if [ "$#" -ge 3 ]; then
    SERVER_PORT=$3
fi

echo "===== Запуск $NUM_SWARMS стай пчел для программы на 4-5 баллов ====="
echo "Подключение к серверу: $SERVER_IP:$SERVER_PORT"
echo "--------------------------------------"

# Массив для хранения PID клиентов
declare -a BEE_PIDS

# Запуск клиентов
for (( i=1; i<=$NUM_SWARMS; i++ ))
do
    echo "Запуск стаи #$i"
    ./$CLIENT_PROGRAM $SERVER_IP $SERVER_PORT $i &
    BEE_PIDS[$i]=$!
    sleep 0.5 # Небольшая задержка между запусками
done

echo "--------------------------------------"
echo "Все $NUM_SWARMS стай запущены."
echo "PID процессов:"
for (( i=1; i<=$NUM_SWARMS; i++ ))
do
    echo "Стая #$i: PID=${BEE_PIDS[$i]}"
done
echo "--------------------------------------"

# Функция для завершения всех клиентов
cleanup() {
    echo -e "\nЗавершение всех стай..."
    for pid in ${BEE_PIDS[@]}
    do
        if ps -p $pid > /dev/null; then
            echo "Завершение стаи с PID $pid"
            kill -9 $pid 2>/dev/null
        fi
    done
    echo "Все стаи завершены."
    exit 0
}

# Установка обработчика сигнала для Ctrl+C
trap cleanup SIGINT SIGTERM

echo "Стаи работают. Нажмите Ctrl+C для завершения всех стай."

# Ждем сигнала завершения
while true; do
    # Проверяем, все ли клиенты еще работают
    all_finished=true
    for pid in ${BEE_PIDS[@]}
    do
        if ps -p $pid > /dev/null; then
            all_finished=false
            break
        fi
    done

    # Если все клиенты завершились, выходим из цикла
    if $all_finished; then
        echo "Все стаи завершили работу."
        break
    fi

    sleep 1
done

exit 0
