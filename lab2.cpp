#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>
using namespace std;

// Класс Квадрат
class Kvadrat {
protected:
    double a; // длина стороны
public:
    Kvadrat(double side = 0.0) : a(side) {}

    double diagonal() const { return a * sqrt(2.0); } // диагональ
    double perimeter() const { return 4.0 * a; }      // периметр
    virtual double area() const { return a * a; }     // площадь

    virtual void printInfo() const {
        cout << "Квадрат: сторона=" << a
             << "  диагональ=" << diagonal()
             << "  периметр=" << perimeter()
             << "  площадь=" << area() << "\n";
    }

    double getSide() const { return a; }
};

// Класс Призма (наследует от Квадрат)
class Prizma : public Kvadrat {
    double h; // высота призмы
public:
    Prizma(double side = 0.0, double height = 0.0) : Kvadrat(side), h(height) {}

    double volume() const { return Kvadrat::area() * h; }                 // объём
    double area() const override { return 2.0 * a * a + 4.0 * a * h; }     // площадь поверхности

    void printInfo() const override {
        cout << "Призма: сторона основания=" << a
             << "  высота=" << h
             << "  диагональ основания=" << diagonal()
             << "  площадь поверхности=" << area()
             << "  объём=" << volume() << "\n";
    }

    double getHeight() const { return h; }
};

// Вспомогательная функция: безопасный ввод целого >= minVal
int readInt(const char* prompt, int minVal) {
    int x;
    while (true) {
        cout << prompt;
        if (cin >> x && x >= minVal) {
            return x;
        }
        cout << "  Некорректный ввод. Введите целое число >= " << minVal << ".\n";
        cin.clear();
        while (cin.get() != '\n') ; // сброс буфера
    }
}

// Вспомогательная функция: безопасный ввод double >= minVal
double readDouble(const char* prompt, double minVal) {
    double x;
    while (true) {
        cout << prompt;
        if (cin >> x && x >= minVal) {
            return x;
        }
        cout << "  Некорректный ввод. Введите число >= " << minVal << ".\n";
        cin.clear();
        while (cin.get() != '\n') ;
    }
}

// Случайное double в [minv, maxv]
double randDouble(double minv, double maxv) {
    double r = (double)rand() / (double)RAND_MAX; // [0,1]
    return minv + r * (maxv - minv);
}

int main() {
    srand((unsigned)time(nullptr));

    cout << "=== Лабораторная работа: квадраты и квадратные призмы ===\n\n";

    int mode = readInt("Выберите режим: 1 - ввод вручную, 2 - случайные данные: ", 1);
    while (mode != 1 && mode != 2) {
        cout << "  Введите 1 или 2.\n";
        mode = readInt("Выберите режим: 1 - ввод вручную, 2 - случайные данные: ", 1);
    }

    int N = readInt("Введите количество квадратов N (>=0): ", 0);
    int M = readInt("Введите количество призм M (>=0): ", 0);

    Kvadrat* squares = nullptr;
    Prizma* prisms = nullptr;

    if (N > 0) squares = new Kvadrat[N];
    if (M > 0) prisms = new Prizma[M];

    if (mode == 1) {
        // Ввод вручную
        cout << "\nВвод данных вручную:\n";
        for (int i = 0; i < N; ++i) {
            double side = readDouble(("Сторона квадрата " + to_string(i+1) + ": ").c_str(), 0.0);
            squares[i] = Kvadrat(side);
        }
        for (int i = 0; i < M; ++i) {
            double side = readDouble(("Сторона основания призмы " + to_string(i+1) + ": ").c_str(), 0.0);
            double height = readDouble(("Высота призмы " + to_string(i+1) + ": ").c_str(), 0.0);
            prisms[i] = Prizma(side, height);
        }
    } else {
        // Случайные данные
        cout << "\nГенерация случайных данных.\n";
        double sideMin = readDouble("Минимальная сторона (>=0): ", 0.0);
        double sideMax = readDouble("Максимальная сторона (> min): ", sideMin + 1e-12);
        double heightMin = readDouble("Минимальная высота (>=0): ", 0.0);
        double heightMax = readDouble("Максимальная высота (> min): ", heightMin + 1e-12);

        cout << "\nГенерируем данные...\n";
        for (int i = 0; i < N; ++i) {
            double side = randDouble(sideMin, sideMax);
            squares[i] = Kvadrat(side);
        }
        for (int i = 0; i < M; ++i) {
            double side = randDouble(sideMin, sideMax);
            double height = randDouble(heightMin, heightMax);
            prisms[i] = Prizma(side, height);
        }
    }

    // Опционально вывести все объекты перед поиском
    int showAll = readInt("\nПоказать все созданные объекты? 1-Да 0-Нет: ", 0);
    while (showAll != 0 && showAll != 1) {
        cout << "  Введите 1 или 0.\n";
        showAll = readInt("Показать все созданные объекты? 1-Да 0-Нет: ", 0);
    }

    if (showAll == 1) {
        cout << "\nВсе квадраты:\n";
        if (N == 0) cout << "  Квадратов нет.\n";
        for (int i = 0; i < N; ++i) {
            cout << "  [" << i+1 << "] ";
            squares[i].printInfo();
        }

        cout << "\nВсе призмы:\n";
        if (M == 0) cout << "  Призм нет.\n";
        for (int i = 0; i < M; ++i) {
            cout << "  [" << i+1 << "] ";
            prisms[i].printInfo();
        }
    }

    // Нахождение квадрата с максимальной площадью
    if (N == 0) {
        cout << "\nКвадратов нет для поиска максимальной площади.\n";
    } else {
        int idxMax = 0;
        for (int i = 1; i < N; ++i) {
            if (squares[i].area() > squares[idxMax].area())
                idxMax = i;
        }
        cout << "\nКвадрат с максимальной площадью (номер " << idxMax+1 << "):\n";
        squares[idxMax].printInfo();
    }

    // Нахождение призмы с максимальной диагональю основания
    if (M == 0) {
        cout << "\nПризм нет для поиска максимальной диагонали основания.\n";
    } else {
        int idxMaxP = 0;
        for (int i = 1; i < M; ++i) {
            if (prisms[i].diagonal() > prisms[idxMaxP].diagonal())
                idxMaxP = i;
        }
        cout << "\nПризма с максимальной диагональю основания (номер " << idxMaxP+1 << "):\n";
        prisms[idxMaxP].printInfo();
    }

    // Очистка памяти
    delete[] squares;
    delete[] prisms;

    cout << "\nГотово.\n";
    return 0;
}
