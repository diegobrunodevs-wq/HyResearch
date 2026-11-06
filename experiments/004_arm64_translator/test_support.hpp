#pragma once

#include <iostream>
#include <cstdlib>

#define CHECK(expression)                                      \
    do                                                         \
    {                                                          \
        if (!(expression))                                     \
        {                                                      \
            std::cerr << "FALHA: " << #expression              \
                      << " | Arquivo: " << __FILE__            \
                      << " | Linha: " << __LINE__ << '\n';     \
            std::exit(EXIT_FAILURE);                           \
        }                                                      \
    } while (false)