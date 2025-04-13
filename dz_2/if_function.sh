#!/bin/bash

# Пример использования условия if
# Скрипт проверяет, существует ли файл с именем example.txt в текущем каталоге

if [ -e example.txt ]; then
    echo "Файл example.txt существует."
else
    echo "Файл example.txt не найден."
fi
