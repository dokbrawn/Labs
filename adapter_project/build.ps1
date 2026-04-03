# Скрипт быстрой компиляции для PowerShell
# Запуск: .\build.ps1

Write-Host "=============================================================" -ForegroundColor Cyan
Write-Host "Сборка проекта adapter_project (Вариант 22)" -ForegroundColor Cyan
Write-Host "=============================================================" -ForegroundColor Cyan
Write-Host ""

# Проверяем наличие компилятора cl.exe
$clPath = Get-Command cl.exe -ErrorAction SilentlyContinue

if (-not $clPath) {
    Write-Host "ОШИБКА: Компилятор cl.exe не найден!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Необходимо запустить этот скрипт из Developer Command Prompt for Visual Studio" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Как открыть Developer Command Prompt:" -ForegroundColor Cyan
    Write-Host "1. Нажмите Пуск" -ForegroundColor White
    Write-Host "2. Найдите 'Developer Command Prompt for VS 2022' (или ваша версия)" -ForegroundColor White
    Write-Host "3. Запустите и перейдите в папку с проектом" -ForegroundColor White
    Write-Host "4. Запустите: .\build.ps1" -ForegroundColor White
    exit 1
}

Write-Host "Компилятор найден: $($clPath.Source)" -ForegroundColor Green
Write-Host ""

# Компиляция create_adapter.cpp
Write-Host "[1/2] Компиляция create_adapter.cpp..." -ForegroundColor Cyan
cl /EHsc /O2 /utf-8 create_adapter.cpp /Fe:create_adapter.exe 2>&1 | Select-String -Pattern "error|warning" | Foreach-Object {
    if ($_ -match "error") {
        Write-Host $_ -ForegroundColor Red
    } elseif ($_ -match "warning") {
        Write-Host $_ -ForegroundColor Yellow
    }
}

if (Test-Path "create_adapter.exe") {
    Write-Host "Успешно создан: create_adapter.exe" -ForegroundColor Green
} else {
    Write-Host "ОШИБКА: Не удалось создать create_adapter.exe" -ForegroundColor Red
    exit 1
}

Write-Host ""

# Компиляция process_adapter.cpp
Write-Host "[2/2] Компиляция process_adapter.cpp..." -ForegroundColor Cyan
cl /EHsc /O2 /utf-8 process_adapter.cpp /Fe:process_adapter.exe 2>&1 | Select-String -Pattern "error|warning" | Foreach-Object {
    if ($_ -match "error") {
        Write-Host $_ -ForegroundColor Red
    } elseif ($_ -match "warning") {
        Write-Host $_ -ForegroundColor Yellow
    }
}

if (Test-Path "process_adapter.exe") {
    Write-Host "Успешно создан: process_adapter.exe" -ForegroundColor Green
} else {
    Write-Host "ОШИБКА: Не удалось создать process_adapter.exe" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "=============================================================" -ForegroundColor Cyan
Write-Host "СБОРКА ЗАВЕРШЕНА УСПЕШНО!" -ForegroundColor Green
Write-Host "=============================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Следующие шаги:" -ForegroundColor Yellow
Write-Host "1. Запустите .\create_adapter.exe для создания файла adapter.dat" -ForegroundColor White
Write-Host "2. Запустите .\process_adapter.exe для обработки файла" -ForegroundColor White
Write-Host ""
Write-Host "Вариант 22 - Поиск по типу адаптера" -ForegroundColor Cyan
Write-Host ""
