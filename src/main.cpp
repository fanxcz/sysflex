// main.cpp — точка входа приложения sysflex.
// Разбирает аргументы командной строки, создаёт объект App и запускает его.
#include "sysflex/config.hpp"
#include "sysflex/sysflex.hpp"
#include <iostream>

int main(int argc, char** argv) {
    sysflex::Config config;

    if (!sysflex::Config::parse(argc, argv, config)) {
        return 1;
    }

    sysflex::App app(config);
    return app.run();
}
