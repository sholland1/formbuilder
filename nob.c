#define NOB_IMPLEMENTATION
#include "cmdline/src/nob.h"

#define BUILD_FOLDER "cmdline/build/"
#define SRC_FOLDER   "cmdline/src/"

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    const char *program_name = nob_shift_args(&argc, &argv);

    bool release = false;
    bool run = false;
    while (argc > 0) {
        const char *arg = nob_shift_args(&argc, &argv);
        if (strcmp(arg, "release") == 0) {
            release = true;
        }
        else if (strcmp(arg, "run") == 0) {
            run = true;
        }
        else {
            nob_log(NOB_ERROR, "Unknown arg %s", arg);
            return 1;
        }
    }

    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    Nob_Cmd cmd = {0};

    nob_cmd_append(&cmd, "cc", "-lm", "-Wall", "-Wextra", "-o", BUILD_FOLDER"form", SRC_FOLDER"form.c");
    if (release) {
        nob_cmd_append(&cmd, "-s", "-O2");
    }

    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;

    if (run) {
        nob_cmd_append(&cmd, BUILD_FOLDER"form");
        if (!nob_cmd_run(&cmd)) return 1;
    }

    return 0;
}
