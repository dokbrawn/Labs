#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <iomanip>
#include <clocale>
#include <windows.h>

// Структура записи о сетевом адаптере
struct Adapter {
    char adapterType[50];      // тип сетевого адаптера
    char interface[30];        // интерфейс подключения
    int speed;                 // скорость передачи данных (Мбит/с)
    int ports;                 // количество портов
    char chip[50];             // чип
    char usbStandard[20];      // стандарт USB
};

// Функция для отображения информации о разработчике
void showDeveloperInfo() {
    std::cout << "\n=============================================================" << std::endl;
    std::cout << "Разработчик: Студент группы [Ваша группа]" << std::endl;
    std::cout << "Вариант: 22" << std::endl;
    std::cout << "Задание: Обработка бинарного файла adapter.dat" << std::endl;
    std::cout << "Год: 2025" << std::endl;
    std::cout << "=============================================================\n" << std::endl;
}

// Функция для отображения одной записи
void displayAdapter(const Adapter& adapter, int index) {
    std::cout << "Запись #" << index << std::endl;
    std::cout << "  Тип адаптера: " << adapter.adapterType << std::endl;
    std::cout << "  Интерфейс: " << adapter.interface << std::endl;
    std::cout << "  Скорость: " << adapter.speed << " Мбит/с" << std::endl;
    std::cout << "  Количество портов: " << adapter.ports << std::endl;
    std::cout << "  Чип: " << adapter.chip << std::endl;
    std::cout << "  Стандарт USB: " << adapter.usbStandard << std::endl;
    std::cout << "-------------------------------------------------------------" << std::endl;
}

// Функция для отображения всех записей
void displayAllAdapters(const std::vector<Adapter>& adapters) {
    std::cout << "\n=================== ВСЕ ЗАПИСИ В ФАЙЛЕ ====================" << std::endl;
    for (size_t i = 0; i < adapters.size(); i++) {
        displayAdapter(adapters[i], i + 1);
    }
    std::cout << "Всего записей: " << adapters.size() << std::endl;
}

// Функция поиска по типу адаптера
void searchByType(const std::vector<Adapter>& adapters, const std::string& searchType) {
    std::cout << "\n=================== РЕЗУЛЬТАТЫ ПОИСКА ====================" << std::endl;
    bool found = false;
    
    for (size_t i = 0; i < adapters.size(); i++) {
        // Сравниваем тип адаптера (регистронезависимо)
        std::string adapterType(adapters[i].adapterType);
        
        // Преобразуем в нижний регистр для сравнения
        std::string searchTypeLower = searchType;
        std::string adapterTypeLower = adapterType;
        
        for (auto& c : searchTypeLower) c = tolower(c);
        for (auto& c : adapterTypeLower) c = tolower(c);
        
        if (adapterTypeLower.find(searchTypeLower) != std::string::npos) {
            displayAdapter(adapters[i], i + 1);
            found = true;
        }
    }
    
    if (!found) {
        std::cout << "\n*** Модель с типом \"" << searchType << "\" не найдена! ***" << std::endl;
    } else {
        std::cout << "Поиск завершен." << std::endl;
    }
}

// Функция для отображения информации о файле
void displayFileInfo(const std::string& filename, size_t recordCount) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удалось открыть файл для получения информации!" << std::endl;
        return;
    }
    
    std::streamsize fileSize = file.tellg();
    file.close();
    
    std::cout << "\n=================== ИНФОРМАЦИЯ О ФАЙЛЕ ====================" << std::endl;
    std::cout << "Имя файла: " << filename << std::endl;
    std::cout << "Размер файла: " << fileSize << " байт" << std::endl;
    std::cout << "Количество записей: " << recordCount << std::endl;
    std::cout << "Размер одной записи: " << sizeof(Adapter) << " байт" << std::endl;
    std::cout << "=============================================================\n" << std::endl;
}

// Функция для редактирования записи
void editRecord(std::vector<Adapter>& adapters) {
    int recordNum;
    std::cout << "\nВведите номер записи для редактирования (1-20): ";
    std::cin >> recordNum;
    
    if (recordNum < 1 || recordNum > static_cast<int>(adapters.size())) {
        std::cout << "Неверный номер записи!" << std::endl;
        return;
    }
    
    int idx = recordNum - 1;
    Adapter& adapter = adapters[idx];
    
    std::cout << "\nРедактирование записи #" << recordNum << std::endl;
    std::cout << "Текущие данные:" << std::endl;
    displayAdapter(adapter, recordNum);
    
    std::cout << "\nВведите новые данные (оставьте пустым, чтобы не менять):" << std::endl;
    
    std::string input;
    std::cout << "Тип адаптера [" << adapter.adapterType << "]: ";
    std::getline(std::cin >> std::ws, input);
    if (!input.empty()) {
        std::memset(adapter.adapterType, 0, 50);
        std::strncpy(adapter.adapterType, input.c_str(), 49);
    }
    
    std::cout << "Интерфейс [" << adapter.interface << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        std::memset(adapter.interface, 0, 30);
        std::strncpy(adapter.interface, input.c_str(), 29);
    }
    
    std::cout << "Скорость [" << adapter.speed << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        adapter.speed = std::stoi(input);
    }
    
    std::cout << "Количество портов [" << adapter.ports << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        adapter.ports = std::stoi(input);
    }
    
    std::cout << "Чип [" << adapter.chip << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        std::memset(adapter.chip, 0, 50);
        std::strncpy(adapter.chip, input.c_str(), 49);
    }
    
    std::cout << "Стандарт USB [" << adapter.usbStandard << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        std::memset(adapter.usbStandard, 0, 20);
        std::strncpy(adapter.usbStandard, input.c_str(), 19);
    }
    
    std::cout << "\nЗапись обновлена!" << std::endl;
}

// Функция для сохранения изменений в файл
void saveToFile(const std::vector<Adapter>& adapters, const std::string& filename) {
    std::ofstream outFile(filename, std::ios::binary);
    
    if (!outFile.is_open()) {
        std::cerr << "Ошибка: не удалось открыть файл для записи!" << std::endl;
        return;
    }
    
    for (const auto& adapter : adapters) {
        outFile.write(reinterpret_cast<const char*>(&adapter), sizeof(Adapter));
    }
    
    outFile.close();
    std::cout << "\nИзменения успешно сохранены в файл " << filename << std::endl;
}

int main() {
    // Устанавливаем кодировку для корректного отображения русского текста в Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    setlocale(LC_ALL, "ru_RU.UTF-8");
    
    showDeveloperInfo();
    
    std::string filename = "adapter.dat";
    std::vector<Adapter> adapters;
    
    // Открываем файл и читаем все записи
    std::ifstream inFile(filename, std::ios::binary);
    
    if (!inFile.is_open()) {
        std::cerr << "Ошибка: не удалось открыть файл " << filename << std::endl;
        std::cerr << "Убедитесь, что файл существует (запустите сначала create_adapter.exe)" << std::endl;
        return 1;
    }
    
    // Читаем все записи из файла
    Adapter adapter;
    while (inFile.read(reinterpret_cast<char*>(&adapter), sizeof(Adapter))) {
        adapters.push_back(adapter);
    }
    
    inFile.close();
    
    std::cout << "Файл успешно загружен!" << std::endl;
    displayFileInfo(filename, adapters.size());
    
    // Главное меню программы
    int choice;
    do {
        std::cout << "\n=================== МЕНЮ ПРОГРАММЫ ===================" << std::endl;
        std::cout << "1. Показать все записи" << std::endl;
        std::cout << "2. Поиск по типу адаптера" << std::endl;
        std::cout << "3. Редактировать запись" << std::endl;
        std::cout << "4. Сохранить изменения" << std::endl;
        std::cout << "5. Информация о файле" << std::endl;
        std::cout << "6. Информация о разработчике" << std::endl;
        std::cout << "0. Выход" << std::endl;
        std::cout << "===============================================" << std::endl;
        std::cout << "Выберите действие: ";
        std::cin >> choice;
        
        switch (choice) {
            case 1:
                displayAllAdapters(adapters);
                break;
                
            case 2: {
                std::string searchType;
                std::cout << "\nВведите тип адаптера для поиска: ";
                std::getline(std::cin >> std::ws, searchType);
                searchByType(adapters, searchType);
                break;
            }
            
            case 3:
                editRecord(adapters);
                break;
                
            case 4:
                saveToFile(adapters, filename);
                break;
                
            case 5:
                displayFileInfo(filename, adapters.size());
                break;
                
            case 6:
                showDeveloperInfo();
                break;
                
            case 0:
                std::cout << "\nПрограмма завершена." << std::endl;
                break;
                
            default:
                std::cout << "\nНеверный выбор! Попробуйте снова." << std::endl;
        }
        
    } while (choice != 0);
    
    return 0;
}
