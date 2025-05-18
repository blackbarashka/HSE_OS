# Компилятор и флаги
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
LDFLAGS = -pthread

# Исполняемые файлы
SERVER = server
CLIENT = client
OBSERVER = observer

# Исходные файлы
SERVER_SRC = server.cpp
CLIENT_SRC = client.cpp
OBSERVER_SRC = observer.cpp

# Цели по умолчанию
all: $(SERVER) $(CLIENT) $(OBSERVER)

# Правила компиляции
$(SERVER): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

$(CLIENT): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

$(OBSERVER): $(OBSERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

# Очистка сгенерированных файлов
clean:
	rm -f $(SERVER) $(CLIENT) $(OBSERVER)

# IP и порт для запуска (можно изменить)
IP = 127.0.0.1
PORT = 8080
NUM_BEES = 5
DELAY = 0.5

# Запуск сервера
run-server: $(SERVER)
	./$(SERVER) $(IP) $(PORT)

# Запуск одного клиента
run-client: $(CLIENT)
	./$(CLIENT) 1 $(IP) $(PORT)

# Запуск наблюдателя
run-observer: $(OBSERVER)
	./$(OBSERVER) $(IP) $(PORT)

# Запуск множества клиентов
run-bees: $(CLIENT)
	@if [ -f run_clients.sh ]; then \
		chmod +x run_clients.sh && ./run_clients.sh $(NUM_BEES) $(IP) $(PORT) $(DELAY); \
	else \
		echo "Скрипт run_clients.sh не найден. Запускаем клиентов вручную."; \
		for i in `seq 1 $(NUM_BEES)`; do \
			echo "Запуск стаи #$$i..."; \
			./$(CLIENT) $$i $(IP) $(PORT) & \
			sleep $(DELAY); \
		done; \
		echo "Все стаи запущены!"; \
	fi

# Запуск полной демонстрации
demo: $(SERVER) $(CLIENT) $(OBSERVER)
	@echo "Запуск демонстрации..."
	@echo "1. Сначала запустите сервер: make run-server"
	@echo "2. В другом терминале запустите наблюдателя: make run-observer"
	@echo "3. В третьем терминале запустите стаи пчел: make run-bees"

# Маркер для указания фиктивных целей
.PHONY: all clean run-server run-client run-observer run-bees demo
