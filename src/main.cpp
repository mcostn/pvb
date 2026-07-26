#include <iostream>

#include "ui/ui.hpp"

int main()
{
    Error err = StartApp();
    if (err != Error::Ok) {
        GlobalLogger.Error("Unexpected error: {}", ErrorDetail::Message);
        return EXIT_FAILURE;
    }
}
