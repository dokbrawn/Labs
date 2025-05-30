import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# Загрузка данных
try:
    df = pd.read_csv('matrix_mult_results.txt')
except FileNotFoundError:
    print("Ошибка: файл 'matrix_mult_results.txt' не найден.")
    exit()

# Переименование колонок на русский
df.rename(columns={
    'N': 'Размер',
    'Threads': 'Потоки',
    'Time_ms': 'Время_мс'
}, inplace=True)

# Убедиться, что "Потоки" — строка для разделения цветов
df['Потоки'] = df['Потоки'].astype(str)

# Настройка стиля
sns.set_theme(style="whitegrid")

# Создание графика
plt.figure(figsize=(12, 8))
sns.lineplot(
    data=df,
    x='Размер',
    y='Время_мс',
    hue='Потоки',
    marker='o',
    palette='viridis',
    linewidth=2.5
)

# Подписи и оформление
plt.title('Производительность перемножения матриц', fontsize=16)
plt.xlabel('Размер матрицы (N)', fontsize=14)
plt.ylabel('Время выполнения (мс)', fontsize=14)
plt.legend(title='Количество потоков', bbox_to_anchor=(1.05, 1), loc='upper left')
plt.grid(True, linestyle='--', alpha=0.7)
plt.tight_layout()

# Отображение
plt.show()
