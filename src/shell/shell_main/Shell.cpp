#include "shell/Shell.h"
#include "shell/console_colors.h"
#include <stdio.h>
#include <string.h>


#undef count_of
#define count_of(x)     (sizeof(x) / sizeof(x[0]))

#define CONTROL_ARROW_UP        "\x1B[A"
#define CONTROL_ARROW_DOWN      "\x1B[B"
#define CONTROL_ARROW_RIGHT     "\x1B[C"
#define CONTROL_ARROW_LEFT      "\x1B[D"
#define CONTROL_HOME            "\x1B[H"
#define CONTROL_HOME_ALT        "\x1B[1~"
#define CONTROL_END             "\x1B[F"
#define CONTROL_END_ALT         "\x1B[4~"
#define CONTROL_PAGE_UP         "\x1B[5~"
#define CONTROL_PAGE_DOWN       "\x1B[6~"
#define CONTROL_DELETE          "\x1B[3~"

static void print_eol() {
    printf("\r\n");
}

Shell::Shell(const Handler *handlers) : handlers(handlers) {
    history = new History(8);
    control_sequence.position = 0;
    control_sequence.buffer[0] = 0;
}

Shell::~Shell() {
    if (history) {
        delete history;
        history = nullptr;
    }
}

void Shell::reset() {
}

void Shell::start() {
    printf("%s ", ">");
}

int Shell::handle_input() {
    static const char *argv[32];
    int argc = 0;

    char *ptr = input.buffer;
    char *end = input.buffer + input.size;

    while (ptr < end && argc < count_of(argv) - 1) {
        while (ptr < end && *ptr == ' ') ptr++;
        if (ptr >= end) break;

        if (*ptr == '"') {
            ptr++;
            argv[argc++] = ptr;
            while (ptr < end && *ptr != '"') ptr++;
            *ptr++ = '\0';
        } else {
            argv[argc++] = ptr;
            while (ptr < end && *ptr != ' ') ptr++;
            *ptr++ = '\0';
        }
    }
    argv[argc] = nullptr;

    if (argc < 1) return static_cast<unsigned char>(-1);

    const char *command = argv[0];

    for (int i = 0;; i++) {
        if (!handlers[i].name || !handlers[i].handler) break;

        const Handler *h = &handlers[i];
        if (strcmp(command, h->name) == 0) {
            if (history) history->add(argc, argv);
            return h->handler(argc, argv);
        }
    }

    printf(COLOR_RED("Command %s not handled\r\n"), command);
    return static_cast<unsigned char>(-2);
}

//------------------------------------------------------------------------------

void Shell::update(int c) {
    if (is_control_sequence(c)) return;

    if (c == '\t') {
        if (autocomplete_streak < 2) {
            this->autocomplete();
            autocomplete_streak++;
        }
        return;
    }

    autocomplete_streak = 0;

    if (c == '\r') c = '\n';

    if (c == '\x7F') {
        // backspace
        if (input.cursor > input.buffer) {
            printf("\b \b");
            input.remove_left();
        }

        return;
    }

    if (c == '\x03' || c == '\x04') {
        // Ctrl + C | Ctrl + D
        print_eol();

        input.reset();
        this->start();

        return;
    }

    if (c == '\n') {
        input.end();

        print_eol();

        if (!input.is_empty()) {
            int status = handle_input();
        }

        input.reset();
        this->start();

        return;
    }

    putchar(c);
    input.put(c);
}

int Shell::ControlSequence::detect(int c) {
    if (c == '\x1B') {
        // this is the beginning of control sequence
        position = 0;
        buffer[position++] = c;

        return IN_SEQUENCE;
    }

    if (position == 1) {
        if (c == '[') {
            buffer[position++] = c;
        } else {
            // probably incorrect
            // ignore current sequence
            position = 0;
        }

        return IN_SEQUENCE;
    }

    if (position > 1) {
        buffer[position++] = c;

        if (c >= 'A' && c <= 'Z') {
            // end of control sequence
            buffer[position] = 0;
            position = 0;
        } else if (c == '~') {
            // end of control sequence [F1-F12]
            buffer[position] = 0;
            position = 0;
        } else if (position >= count_of(buffer)) {
            // buffer overflow
            position = 0;
        }

        if (position == 0) {
            return END_SEQUENCE;
        }

        return IN_SEQUENCE;
    }

    return NO_SEQUENCE;
}

bool Shell::is_control_sequence(int c) {
    switch (control_sequence.detect(c)) {
        case ControlSequence::END_SEQUENCE:
            handle_control_sequence(control_sequence.buffer);
        case ControlSequence::IN_SEQUENCE:
            return true;
        default:
            return false;
    }
}

void Shell::handle_control_sequence(const char *control) {
    if (strcmp(control, CONTROL_ARROW_UP) == 0) {
        this->replace_command(history->prev());
    } else if (strcmp(control, CONTROL_ARROW_DOWN) == 0) {
        this->replace_command(history->next());
    } else if (strcmp(control, CONTROL_ARROW_LEFT) == 0) {
        if (input.cursor_left()) {
            printf(CONTROL_ARROW_LEFT);
        }
    } else if (strcmp(control, CONTROL_ARROW_RIGHT) == 0) {
        if (input.cursor_right()) {
            printf(CONTROL_ARROW_RIGHT);
        }
    } else if (strcmp(control, CONTROL_PAGE_UP) == 0) {
    } else if (strcmp(control, CONTROL_PAGE_DOWN) == 0) {
    } else if (strcmp(control, CONTROL_HOME) == 0 || strcmp(control, CONTROL_HOME_ALT) == 0) {
        int length = input.get_offset();
        for (int i = 0; i < length; i++) {
            printf(CONTROL_ARROW_LEFT);
        }
        input.set_offset(0);
    } else if (strcmp(control, CONTROL_END) == 0 || strcmp(control, CONTROL_END_ALT) == 0) {
    } else if (strcmp(control, CONTROL_DELETE) == 0) {
    } else {
        putchar('\r');
        printf("Unhandled control sequence [" COLOR_YELLOW("\\x%02X%s") "]", control[0], &control[1]);
        print_eol();
    }
}

void Shell::replace_command(const char *command) {
    size_t length = strlen(input.buffer);
    putchar('\r');
    for (size_t i = 0; i < length + 4; i++) {
        putchar(' ');
    }
    putchar('\r');
    this->start();

    if (command) {
        printf("%s", command);
        input.set(command);
    } else {
        input.reset();
    }
}

static unsigned int prefix_match(const char *s1, const char *s2) {
    size_t i = 0;
    while (*s1 != '\0' && *s2 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
        i++;
    }
    return i;
}

void Shell::autocomplete() {
    size_t length = strlen(input.buffer);
    if (length == 0 || input.buffer[length - 1] == ' ') {
        return;
    }

    const char *candidates[16] = {nullptr};

    size_t found_count = 0;
    for (int i = 0;; i++) {
        if (!handlers[i].name || !handlers[i].handler) break;

        size_t match_count = prefix_match(input.buffer, handlers[i].name);
        if (match_count < length) {
            continue;
        }

        if (found_count < count_of(candidates)) {
            candidates[found_count] = handlers[i].name;
        }

        found_count++;
    }

    if (found_count == 0) return;

    if (found_count > 1) {
        // print candidates
        putchar('\r');
        for (size_t i = 0; i < found_count && i < count_of(candidates); i++) {
            if (i > 0 && (i & 0b11) == 0b11) {
                print_eol();
            }
            printf("%-16s", candidates[i]);
        }
        print_eol();

        // find how many common symbols
        int common_count = 0;
        for (;; common_count++) {
            for (size_t i = 1; i < found_count; i++) {
                if (candidates[0][common_count] != candidates[i][common_count]) {
                    goto break_2;
                }
            }
        }
    break_2:

        input.reset();
        input.put_strn(candidates[0], common_count);
        this->start();
        printf("%s", input.buffer);

        return;
    }

    // found_count == 1

    auto i = length;
    if (candidates[0][i] == '\0') {
        putchar(' ');
        input.put(' ');
    } else {
        for (;; i++) {
            char c = candidates[0][i];
            if (c == '\0') break;
            putchar(c);
            input.put(c);
        }
    }
}
