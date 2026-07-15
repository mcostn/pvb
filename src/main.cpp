#include <iostream>

#include "ui/ui.hpp"

int main()
{
    Error err = StartApp();
    if (err != Error::Ok) {
        GlobalLogger.Error("Unexpected error {}", static_cast<int>(err));
        return EXIT_FAILURE;
    }
}
