#include "shell/Shell.h"
#include "shell/commands_common.h"
#include "commands/commands.h"
#include <stdio.h>


static int help(int, const char *[]);

extern const Shell::Handler handlers[];

const Shell::Handler handlers[] = {
    {"help", help},
    {"pinout", command_pinout},
    {"echo", command_echo},
    {"dump", command_dump},
    {"dump32", command_dump32},
    {"sensors", command_sensors},
    {"clocks", command_clocks},
    {"i2c_scan", command_i2c_scan},
    {"chip_id", command_chip_id},
    {"tasks", command_tasks},
    // required at the end
    {nullptr, nullptr},
};


static int help(int, const char *[]) {
    printf("Commands:\r\n");
    for (int i = 0;; i++) {
        if (!handlers[i].name || !handlers[i].handler) {
            break;
        }

        printf("  %s\r\n", handlers[i].name);
    }

    return 0;
}
