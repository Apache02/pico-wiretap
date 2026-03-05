#include "shell/History.h"
#include <string.h>


#undef MIN
#undef MAX
#define MIN(a, b)       (a > b ? b : a)
#define MAX(a, b)       (a < b ? b : a)

History::History(uint8_t depth) : depth(depth) {
    size = 0;
    index = 0;
    tokens = nullptr;
    if (depth > 0) {
        tokens = new char *[depth];
        for (auto i = 0; i < depth; i++) {
            tokens[i] = nullptr;
        }
    }
}

History::~History() {
    for (auto i = 0; i < size; i++) {
        delete[] tokens[i];
        tokens[i] = nullptr;
    }
    delete[] tokens;
    tokens = nullptr;
}

void History::add(const char *token) {
    index = -1;

    if (size > 0 && strcmp(token, tokens[0]) == 0) {
        return;
    }

    if (size == depth) {
        delete[] tokens[size - 1];
        tokens[size - 1] = nullptr;
    }
    for (auto i = size; i > 0; i--) {
        if (i < depth) tokens[i] = tokens[i - 1];
    }
    tokens[0] = new char[strlen(token) + 1];
    strcpy(tokens[0], token);

    size = MIN(size + 1, depth);
}

void History::add(int argc, const char *argv[]) {
    if (argc <= 0) return;

    size_t total_len = 0;
    for (auto i = 0; i < argc; i++) {
        total_len += strlen(argv[i]);
        if (i < argc - 1) total_len++;
    }

    char *token = new char[total_len + 1];
    token[0] = '\0';
    for (auto i = 0; i < argc; i++) {
        strcat(token, argv[i]);
        if (i < argc - 1) strcat(token, " ");
    }

    add(token);
    delete[] token;
}

const char *History::prev() {
    index = MIN(index + 1, size - 1);
    if (index < 0) return nullptr;
    return tokens[index];
}

const char *History::next() {
    index = MAX(index - 1, -1);
    if (index < 0) return nullptr;
    return tokens[index];
}

