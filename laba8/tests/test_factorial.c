#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h> // Подключаем CMocka

#include "../src/factorial.h" // Подключаем тестируемый код

// Тестовая функция для случая 0!
static void test_factorial_zero_input(void **state) {
    (void) state; // Не используется в этом тесте
    assert_int_equal(1, factorial(0));
}

// Тестовая функция для положительных значений
static void test_factorial_positive_input(void **state) {
    (void) state; // Не используется в этом тесте
    assert_int_equal(1, factorial(1));
    assert_int_equal(2, factorial(2));
    assert_int_equal(6, factorial(3));
    assert_int_equal(120, factorial(5));
    // assert_int_equal(3628800, factorial(factorial(10))); // Это слишком большое число для int, если factorial возвращает long long, то можно оставить
}

// Тестовая функция для отрицательных значений
static void test_factorial_negative_input(void **state) {
    (void) state; // Не используется в этом тесте
    assert_int_equal(-1, factorial(-5)); // Предполагаем, что -1 возвращается для ошибки
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_factorial_zero_input),
        cmocka_unit_test(test_factorial_positive_input),
        cmocka_unit_test(test_factorial_negative_input),
    };

    // Запуск всех тестов и возвращение результата
    return cmocka_run_group_tests(tests, NULL, NULL);
}