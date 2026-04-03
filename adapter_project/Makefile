# Makefile для проекта обработки бинарных файлов сетевых адаптеров
# Вариант 22

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra

# Целевые файлы
CREATE_TARGET = create_adapter
PROCESS_TARGET = process_adapter

# Исходные файлы
CREATE_SRC = create_adapter.cpp
PROCESS_SRC = process_adapter.cpp

# Все цели
all: $(CREATE_TARGET) $(PROCESS_TARGET)

# Компиляция программы создания файла
$(CREATE_TARGET): $(CREATE_SRC)
	$(CXX) $(CXXFLAGS) -o $(CREATE_TARGET) $(CREATE_SRC)

# Компиляция программы обработки файла
$(PROCESS_TARGET): $(PROCESS_SRC)
	$(CXX) $(CXXFLAGS) -o $(PROCESS_TARGET) $(PROCESS_SRC)

# Запуск программы создания
run_create: $(CREATE_TARGET)
	./$(CREATE_TARGET)

# Запуск программы обработки
run_process: $(PROCESS_TARGET)
	./$(PROCESS_TARGET)

# Очистка скомпилированных файлов
clean:
	rm -f $(CREATE_TARGET) $(PROCESS_TARGET) adapter.dat

# Пересборка
rebuild: clean all

.PHONY: all clean rebuild run_create run_process
