#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>
using namespace std;

// Суперкласс Квадрат
class Kvadrat {
protected:
    double a; // длина стороны
public:
    Kvadrat(double side = 0.0) : a(side) {}
    virtual double diagonal() const { return a * sqrt(2.0); }
    virtual double perimeter() const { return 4.0 * a; }
    virtual double area() const { return a * a; }

    virtual void printInfo() const {
        cout << "Квадрат: сторона=" << a
             << "  диагональ=" << diagonal()
             << "  периметр=" << perimeter()
             << "  площадь=" << area() << endl;
    }
};

// Подкласс Призма
class Prizma : public Kvadrat {
    double h; // высота
public:
    Prizma(double side = 0.0, double height = 0.0) : Kvadrat(side), h(height) {}
    // площадь поверхности (переопределяем)
    double area() const override { return 2.0 * a * a + 4.0 * a * h; }
    double volume() const { return Kvadrat::area() * h; }

    void printInfo() const override {
        cout << "Призма: сторона=" << a
             << "  высота=" << h
             << "  диагональ основания=" << diagonal()
             << "  площадь поверхности=" << area()
             << "  объём=" << volume() << endl;
    }
};

int main() {
    srand((unsigned)time(NULL));

    cout << "Выберите режим: 1 - ввод вручную, 2 - случайные данные: ";
    int mode;
    cin >> mode;

    cout << "Введите количество квадратов N: ";
    int N; cin >> N;
    if (N < 0) N = 0;
    cout << "Введите количество призм M: ";
    int M; cin >> M;
    if (M < 0) M = 0;

    Kvadrat* squares = NULL;
    Prizma* prisms = NULL;
    if (N > 0) squares = new Kvadrat[N];
    if (M > 0) prisms = new Prizma[M];

    if (mode == 1) {
        // ручной ввод
        for (int i = 0; i < N; ++i) {
            double s;
            cout << "Сторона квадрата " << i+1 << ": ";
            cin >> s;
            squares[i] = Kvadrat(s);
        }
        for (int i = 0; i < M; ++i) {
            double s, h;
            cout << "Сторона основания призмы " << i+1 << ": ";
            cin >> s;
            cout << "Высота призмы " << i+1 << ": ";
            cin >> h;
            prisms[i] = Prizma(s, h);
        }
    } else {
        // генерация простых случайных данных (1..10)
        for (int i = 0; i < N; ++i) {
            double s = (rand() % 100 + 1) / 10.0; // 0.1 .. 10.0
            squares[i] = Kvadrat(s);
        }
        for (int i = 0; i < M; ++i) {
            double s = (rand() % 100 + 1) / 10.0;
            double h = (rand() % 100 + 1) / 10.0;
            prisms[i] = Prizma(s, h);
        }
    }

    // Покажем все объекты (коротко)
    cout << "\nВсе квадраты:\n";
    if (N == 0) cout << "  нет\n";
    for (int i = 0; i < N; ++i) {
        cout << "[" << i+1 << "] "; squares[i].printInfo();
    }

    cout << "\nВсе призмы:\n";
    if (M == 0) cout << "  нет\n";
    for (int i = 0; i < M; ++i) {
        cout << "[" << i+1 << "] "; prisms[i].printInfo();
    }

    // Найти квадрат с максимальной площадью
    if (N > 0) {
        int idx = 0;
        for (int i = 1; i < N; ++i) {
            if (squares[i].area() > squares[idx].area()) idx = i;
        }
        cout << "\nКвадрат с максимальной площадью (номер " << idx+1 << "):\n";
        squares[idx].printInfo();
    } else {
        cout << "\nКвадратов нет для поиска.\n";
    }

    // Найти призму с максимальной диагональю основания
    if (M > 0) {
        int idx = 0;
        for (int i = 1; i < M; ++i) {
            if (prisms[i].diagonal() > prisms[idx].diagonal()) idx = i;
        }
        cout << "\nПризма с максимальной диагональю основания (номер " << idx+1 << "):\n";
        prisms[idx].printInfo();
    } else {
        cout << "\nПризм нет для поиска.\n";
    }

    // очистка
    delete[] squares;
    delete[] prisms;

    return 0;
}
. Что такое подкласс?
Подкласс — это класс, который наследует свойства и методы другого класса (суперкласса). Он может добавлять новые поля и методы или переопределять унаследованные.
2. Как используется конструктор суперкласса?
Конструктор суперкласса вызывается из конструктора подкласса с помощью списка инициализации:
Копировать код
Cpp
Prizma(double s, double h) : Kvadrat(s), h(h) {}
Это позволяет корректно передать значения полей, объявленных в базовом классе.
3. Что такое наследование методов суперкласса?
Наследование методов — это механизм, при котором подкласс получает доступ ко всем открытым (public) и защищённым (protected) методам суперкласса. Подкласс может:
использовать их без изменений;
переопределить с помощью override;
вызывать явно через SuperClassName::method().
