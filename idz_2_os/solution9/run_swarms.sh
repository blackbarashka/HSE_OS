#!/bin/bash
NUM_SWARMS=${1:-5}
echo "Запуск ${NUM_SWARMS} стай пчел..."
for i in $(seq 1 ${NUM_SWARMS}); do
    ./bee_swarm $i &
    sleep 0.5
done
echo "Все стаи запущены"
