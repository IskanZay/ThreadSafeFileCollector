# Конфигурация
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Сборка для Visual Studio
cmake --build build --config Release

# Запуск тестов 
ctest --test-dir build -C Release --output-on-failure
