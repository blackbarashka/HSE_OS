CC = g++
CFLAGS = -Wall -std=c++11
TARGETS = hive_server bee_swarm

all: $(TARGETS) run_script

hive_server: hive_server.cpp
	$(CC) $(CFLAGS) -o $@ $<

bee_swarm: bee_swarm.cpp
	$(CC) $(CFLAGS) -o $@ $<

run_script:
	echo '#!/bin/bash' > run_swarms.sh
	echo 'NUM_SWARMS=$${1:-5}' >> run_swarms.sh
	echo 'echo "Запуск $${NUM_SWARMS} стай пчел..."' >> run_swarms.sh
	echo 'for i in $$(seq 1 $${NUM_SWARMS}); do' >> run_swarms.sh
	echo '    ./bee_swarm $$i &' >> run_swarms.sh
	echo '    sleep 0.5' >> run_swarms.sh
	echo 'done' >> run_swarms.sh
	echo 'echo "Все стаи запущены"' >> run_swarms.sh
	chmod +x run_swarms.sh

clean:
	rm -f $(TARGETS) run_swarms.sh
