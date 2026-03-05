#pragma once

#include <stdint.h>


class History {
private:
    uint8_t depth;
    int8_t index;
    uint8_t size;
    char **tokens;

public:
    History(uint8_t depth);

    ~History();

    void add(const char *token);

    void add(int argc, const char *argv[]);

    const char *prev();

    const char *next();
};
