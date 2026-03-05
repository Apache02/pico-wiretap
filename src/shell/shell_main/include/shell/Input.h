#pragma once

#include <string.h>


struct Input {
    int sentinel1 = 0xDEADBEEF;
    char buffer[16384]{};
    int sentinel2 = 0xF00DCAFE;
    int size = 0;
    bool error = false;
    char *cursor = buffer;

    void reset() {
        memset(buffer, '\0', sizeof(buffer));
        size = 0;
        error = false;
        cursor = buffer;
    }

    void put(char c) {
        if (size >= sizeof(buffer) - 1) {
            error = true;
        }
        *cursor++ = c;
        size++;
    }

    void end() {
        *cursor = '\0';
    }

    void set(const char *s) {
        reset();
        while (*s) put(*s++);
    }

    void put_strn(const char *s, int n) {
        while (*s && n-- > 0) put(*s++);
    }

    bool is_empty() {
        return buffer[0] == '\0';
    }

    // ------------------------------

    bool remove_left() {
        if (cursor > buffer) {
            *--cursor = '\0';
            size--;
            return true;
        }
        return false;
    }

    bool remove_right() {
        if (cursor < buffer + size) {
            memmove(cursor, cursor + 1, size - (cursor - buffer));
            size--;
            return true;
        }
        return false;
    }

    // ------------------------------

    bool cursor_left() {
        if (cursor > buffer) {
            cursor--;
            return true;
        }
        return false;
    }

    bool cursor_right() {
        if (cursor < buffer + size) {
            cursor++;
            return true;
        }
        return false;
    }

    // ------------------------------

    int get_offset() {
        return cursor - buffer;
    }

    void set_offset(int offset) {
        cursor = buffer + offset;
    }
};
