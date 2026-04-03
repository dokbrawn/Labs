#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>

// Структура записи о сетевом адаптере
struct Adapter {
    char adapterType[50];      // тип сетевого адаптера
    char interface[30];        // интерфейс подключения
    int speed;                 // скорость передачи данных (Мбит/с)
    int ports;                 // количество портов
    char chip[50];             // чип
    char usbStandard[20];      // стандарт USB
};

// Функция для заполнения строки с нулевым завершением
void fillString(char* dest, const std::string& src, int size) {
    std::memset(dest, 0, size);
    std::strncpy(dest, src.c_str(), size - 1);
}

int main() {
    setlocale(LC_ALL, "Russian");
    
    // Создаем вектор с 20 записями о сетевых адаптерах
    std::vector<Adapter> adapters(20);
    
    // Заполняем данные
    // Запись 1
    fillString(adapters[0].adapterType, "Ethernet Adapter", 50);
    fillString(adapters[0].interface, "PCI Express", 30);
    adapters[0].speed = 1000;
    adapters[0].ports = 2;
    fillString(adapters[0].chip, "Intel I210", 50);
    fillString(adapters[0].usbStandard, "N/A", 20);
    
    // Запись 2
    fillString(adapters[1].adapterType, "Wi-Fi Adapter", 50);
    fillString(adapters[1].interface, "USB", 30);
    adapters[1].speed = 867;
    adapters[1].ports = 1;
    fillString(adapters[1].chip, "Realtek RTL8812AU", 50);
    fillString(adapters[1].usbStandard, "USB 3.0", 20);
    
    // Запись 3
    fillString(adapters[2].adapterType, "Bluetooth Adapter", 50);
    fillString(adapters[2].interface, "USB", 30);
    adapters[2].speed = 3;
    adapters[2].ports = 1;
    fillString(adapters[2].chip, "CSR 4.0", 50);
    fillString(adapters[2].usbStandard, "USB 2.0", 20);
    
    // Запись 4
    fillString(adapters[3].adapterType, "Ethernet Adapter", 50);
    fillString(adapters[3].interface, "PCI", 30);
    adapters[3].speed = 100;
    adapters[3].ports = 4;
    fillString(adapters[3].chip, "Realtek RTL8139", 50);
    fillString(adapters[3].usbStandard, "N/A", 20);
    
    // Запись 5
    fillString(adapters[4].adapterType, "Wi-Fi Adapter", 50);
    fillString(adapters[4].interface, "PCI Express", 30);
    adapters[4].speed = 2400;
    adapters[4].ports = 1;
    fillString(adapters[4].chip, "Intel AX200", 50);
    fillString(adapters[4].usbStandard, "N/A", 20);
    
    // Запись 6
    fillString(adapters[5].adapterType, "Gigabit Ethernet", 50);
    fillString(adapters[5].interface, "PCI Express", 30);
    adapters[5].speed = 2500;
    adapters[5].ports = 1;
    fillString(adapters[5].chip, "Intel I225-V", 50);
    fillString(adapters[5].usbStandard, "N/A", 20);
    
    // Запись 7
    fillString(adapters[6].adapterType, "Wi-Fi Adapter", 50);
    fillString(adapters[6].interface, "M.2", 30);
    adapters[6].speed = 3000;
    adapters[6].ports = 1;
    fillString(adapters[6].chip, "MediaTek MT7921", 50);
    fillString(adapters[6].usbStandard, "N/A", 20);
    
    // Запись 8
    fillString(adapters[7].adapterType, "Ethernet Adapter", 50);
    fillString(adapters[7].interface, "USB", 30);
    adapters[7].speed = 1000;
    adapters[7].ports = 1;
    fillString(adapters[7].chip, "ASIX AX88179", 50);
    fillString(adapters[7].usbStandard, "USB 3.0", 20);
    
    // Запись 9
    fillString(adapters[8].adapterType, "Bluetooth Adapter", 50);
    fillString(adapters[8].interface, "PCI Express", 30);
    adapters[8].speed = 5;
    adapters[8].ports = 1;
    fillString(adapters[8].chip, "Intel AX200 BT", 50);
    fillString(adapters[8].usbStandard, "N/A", 20);
    
    // Запись 10
    fillString(adapters[9].adapterType, "Wi-Fi Adapter", 50);
    fillString(adapters[9].interface, "USB", 30);
    adapters[9].speed = 600;
    adapters[9].ports = 1;
    fillString(adapters[9].chip, "TP-Link Archer T4U", 50);
    fillString(adapters[9].usbStandard, "USB 3.0", 20);
    
    // Запись 11
    fillString(adapters[10].adapterType, "Ethernet Adapter", 50);
    fillString(adapters[10].interface, "PCI Express", 30);
    adapters[10].speed = 10000;
    adapters[10].ports = 1;
    fillString(adapters[10].chip, "Intel X520", 50);
    fillString(adapters[10].usbStandard, "N/A", 20);
    
    // Запись 12
    fillString(adapters[11].adapterType, "Wi-Fi Adapter", 50);
    fillString(adapters[11].interface, "PCI Express", 30);
    adapters[11].speed = 1733;
    adapters[11].ports = 1;
    fillString(adapters[11].chip, "Broadcom BCM94360", 50);
    fillString(adapters[11].usbStandard, "N/A", 20);
    
    // Запись 13
    fillString(adapters[12].adapterType, "Bluetooth Adapter", 50);
    fillString(adapters[12].interface, "USB", 30);
    adapters[12].speed = 2;
    adapters[12].ports = 1;
    fillString(adapters[12].chip, "Cambridge Silicon Radio", 50);
    fillString(adapters[12].usbStandard, "USB 2.0", 20);
    
    // Запись 14
    fillString(adapters[13].adapterType, "Ethernet Adapter", 50);
    fillString(adapters[13].interface, "USB", 30);
    adapters[13].speed = 2500;
    adapters[13].ports = 1;
    fillString(adapters[13].chip, "Realtek RTL8156", 50);
    fillString(adapters[13].usbStandard, "USB 3.2", 20);
    
    // Запись 15
    fillString(adapters[14].adapterType, "Wi-Fi Adapter", 50);
    fillString(adapters[14].interface, "USB", 30);
    adapters[14].speed = 1200;
    adapters[14].ports = 1;
    fillString(adapters[14].chip, "Netgear A6100", 50);
    fillString(adapters[14].usbStandard, "USB 2.0", 20);
    
    // Запись 16
    fillString(adapters[15].adapterType, "Gigabit Ethernet", 50);
    fillString(adapters[15].interface, "PCI", 30);
    adapters[15].speed = 1000;
    adapters[15].ports = 2;
    fillString(adapters[15].chip, "Intel I350-T2", 50);
    fillString(adapters[15].usbStandard, "N/A", 20);
    
    // Запись 17
    fillString(adapters[16].adapterType, "Wi-Fi Adapter", 50);
    fillString(adapters[16].interface, "M.2", 30);
    adapters[16].speed = 2400;
    adapters[16].ports = 1;
    fillString(adapters[16].chip, "Qualcomm QCA6174", 50);
    fillString(adapters[16].usbStandard, "N/A", 20);
    
    // Запись 18
    fillString(adapters[17].adapterType, "Bluetooth Adapter", 50);
    fillString(adapters[17].interface, "M.2", 30);
    adapters[17].speed = 5;
    adapters[17].ports = 1;
    fillString(adapters[17].chip, "Intel CNVi", 50);
    fillString(adapters[17].usbStandard, "N/A", 20);
    
    // Запись 19
    fillString(adapters[18].adapterType, "Ethernet Adapter", 50);
    fillString(adapters[18].interface, "PCI Express", 30);
    adapters[18].speed = 1000;
    adapters[18].ports = 4;
    fillString(adapters[18].chip, "Intel I350-T4", 50);
    fillString(adapters[18].usbStandard, "N/A", 20);
    
    // Запись 20
    fillString(adapters[19].adapterType, "Wi-Fi Adapter", 50);
    fillString(adapters[19].interface, "USB", 30);
    adapters[19].speed = 1900;
    adapters[19].ports = 1;
    fillString(adapters[19].chip, "Asus USB-AC68", 50);
    fillString(adapters[19].usbStandard, "USB 3.0", 20);
    
    // Открываем файл для записи в бинарном режиме
    std::ofstream outFile("adapter.dat", std::ios::binary);
    
    if (!outFile.is_open()) {
        std::cerr << "Ошибка: не удалось создать файл adapter.dat" << std::endl;
        return 1;
    }
    
    // Записываем все 20 записей в файл
    for (int i = 0; i < 20; i++) {
        outFile.write(reinterpret_cast<const char*>(&adapters[i]), sizeof(Adapter));
        if (!outFile) {
            std::cerr << "Ошибка записи в файл!" << std::endl;
            outFile.close();
            return 1;
        }
    }
    
    outFile.close();
    
    std::cout << "Файл adapter.dat успешно создан!" << std::endl;
    std::cout << "Количество записей: 20" << std::endl;
    std::cout << "Размер одной записи: " << sizeof(Adapter) << " байт" << std::endl;
    std::cout << "Общий размер файла: " << 20 * sizeof(Adapter) << " байт" << std::endl;
    
    // Выводим информацию о созданных записях
    std::cout << "\nСозданные записи:" << std::endl;
    std::cout << "=============================================================" << std::endl;
    for (int i = 0; i < 20; i++) {
        std::cout << "Запись " << (i + 1) << ": " << adapters[i].adapterType 
                  << " | " << adapters[i].interface 
                  << " | " << adapters[i].speed << " Мбит/с"
                  << " | Портов: " << adapters[i].ports
                  << " | Чип: " << adapters[i].chip
                  << " | USB: " << adapters[i].usbStandard << std::endl;
    }
    
    return 0;
}
