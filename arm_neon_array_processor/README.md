ARM NEON Array Processor
========================

Проект для демонстрации возможностей ARM NEON SIMD архитектуры.

Требования
----------
- ARM платформа с поддержкой NEON (Raspberry Pi 4, Raspberry Pi 3, etc.)
- Компилятор g++ с поддержкой NEON
- Архитектура: aarch64 или armhf

Компиляция на Raspberry Pi 4 (aarch64)
--------------------------------------
g++ -O3 -march=armv8-a+simd -o test_neon main.cpp

Или для armhf:
g++ -O3 -mfpu=neon -mfloat-abi=hard -o test_neon main.cpp

Для кросс-компиляции с x86_64:
aarch64-linux-gnu-g++ -O3 -march=armv8-a+simd -o test_neon_arm main.cpp

Запуск
------
./test_neon

Функции
-------
1. process_array_scalar - скалярная версия (эталон)
2. process_array_neon - векторная версия с NEON (4 элемента за итерацию)
3. process_array_neon_unrolled - развернутая версия (8 элементов за итерацию)

Особенности реализации
---------------------
- Множественная загрузка через vld1q_s32
- Безветвевое вычисление модуля через баррельный шифтер (vshrq_n_s32)
- Условное выполнение через маски (vcgtq_s32, vandq_s32, vorrq_s32)
- Горизонтальное сложение через vaddvq_s32
- Предзагрузка данных (__builtin_prefetch)
- Обработка ошибок при передаче nullptr

Структура проекта
-----------------
arm_neon_processor.h - заголовочный файл с реализацией функций
main.cpp - тесты и бенчмарки
README.md - документация
