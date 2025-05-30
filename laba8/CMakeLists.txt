cmake_minimum_required(VERSION 3.10) # Убедитесь, что у вас установлен CMake 3.10 или новее в WSL
project(MyCProjectTest C) # Объявляем проект на языке C

# Ищем пакет CMocka.
# REQUIRED означает, что если CMocka не будет найден, конфигурация CMake завершится с ошибкой.
# Если CMocka установлен через apt-get install libcmocka-dev, CMake найдет его автоматически.
find_package(Cmocka REQUIRED)

# Добавляем ваш тестируемый C-код как статическую библиотеку.
# Это хорошая практика, чтобы отделить тестируемый код от исполняемого файла тестов.
add_library(factorial_lib STATIC src/factorial.c)

# Указываем, где искать заголовочные файлы для 'factorial_lib'.
# PUBLIC означает, что эти инклюды будут доступны для всех, кто линкуется с factorial_lib.
target_include_directories(factorial_lib PUBLIC src)

# Создаем исполняемый файл для тестов.
# Это будет основной файл, который запускает ваши CMocka тесты.
add_executable(test_factorial tests/test_factorial.c)

# Линкуем тестовый исполняемый файл с CMocka и вашей библиотекой.
# 'cmocka' - это имя библиотеки, которую CMake находит через find_package.
# Это более надежный способ, чем 'Cmocka::cmocka-static', если find_package не создает импортированные цели.
target_link_libraries(test_factorial factorial_lib cmocka)

# Включаем модуль CTest, чтобы CMake мог обнаруживать и запускать тесты.
include(CTest)

# Добавляем наш тестовый исполняемый файл как тест для CTest.
# CTest будет запускать 'test_factorial' и анализировать его выход.
add_test(NAME FactorialTestCommand COMMAND test_factorial)