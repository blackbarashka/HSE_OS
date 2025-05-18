CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra
LDFLAGS = -pthread

all: server client observer bee_manager

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp $(LDFLAGS)

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp $(LDFLAGS)

observer: observer.cpp
	$(CXX) $(CXXFLAGS) -o observer observer.cpp $(LDFLAGS)

bee_manager: bee_manager.cpp
	$(CXX) $(CXXFLAGS) -o bee_manager bee_manager.cpp $(LDFLAGS)

clean:
	rm -f server client observer bee_manager

run-server:
	./server 127.0.0.1 8080

run-client:
	./client 1 127.0.0.1 8080

run-observer:
	./observer 127.0.0.1 8080

run-manager:
	./bee_manager 127.0.0.1 8080

# Запуск демо с различными компонентами
run-demo:
	@echo "Запуск демо-системы (сервер, наблюдатель и менеджер стай в отдельных окнах)..."
	@echo "Для демонстрации корректного завершения работы, нажмите Ctrl+C в окне сервера."
	@if command -v xterm >/dev/null 2>&1; then \
		xterm -title "Сервер" -e "./server 127.0.0.1 8080" & \
		sleep 1; \
		xterm -title "Наблюдатель" -e "./observer 127.0.0.1 8080" & \
		sleep 1; \
		xterm -title "Менеджер стай" -e "./bee_manager 127.0.0.1 8080"; \
	elif command -v gnome-terminal >/dev/null 2>&1; then \
		gnome-terminal -- ./server 127.0.0.1 8080 & \
		sleep 1; \
		gnome-terminal -- ./observer 127.0.0.1 8080 & \
		sleep 1; \
		gnome-terminal -- ./bee_manager 127.0.0.1 8080; \
	else \
		echo "Не найден подходящий терминальный эмулятор для демо."; \
		echo "Запустите компоненты вручную в разных терминалах:"; \
		echo "  ./server 127.0.0.1 8080"; \
		echo "  ./observer 127.0.0.1 8080"; \
		echo "  ./bee_manager 127.0.0.1 8080"; \
	fi

.PHONY: all clean run-server run-client run-observer run-manager run-demo

