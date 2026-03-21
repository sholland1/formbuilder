#define NOB_IMPLEMENTATION
#include "cmdline/src/nob.h"

#define BUILD_FOLDER "cmdline/build/"
#define SRC_FOLDER   "cmdline/src/"

static void append_common_cli_sources(Nob_Cmd *cmd) {
    nob_cmd_append(cmd,
        SRC_FOLDER"form_answers.c",
        SRC_FOLDER"form_terminal.c",
        SRC_FOLDER"form_fields.c",
        SRC_FOLDER"form_timer.c",
        SRC_FOLDER"form_app.c");
}

static void append_compile_flags(Nob_Cmd *cmd, bool release) {
    nob_cmd_append(cmd, "cc", "-Wall", "-Wextra", "-lm", "-pthread");
    if (release) {
        nob_cmd_append(cmd, "-s", "-O2");
    }
    else {
        nob_cmd_append(cmd, "-DDEBUG=1");
    }
}

static bool is_port_arg(const char *arg) {
    if (arg == NULL || *arg == '\0') return false;
    for (const char *p = arg; *p; ++p) {
        if (!isdigit((unsigned char)*p)) return false;
    }
    return true;
}

static bool open_in_browser(const char *url) {
    const char *browser = getenv("BROWSER");
    Nob_Cmd cmd = {0};

    if (browser && *browser) {
        nob_cmd_append(&cmd, "sh", "-c", "exec \"$BROWSER\" \"$1\"", "sh", url);
        return nob_cmd_run(&cmd);
    }

#ifdef _WIN32
    nob_cmd_append(&cmd, "cmd", "/c", "start", "", url);
#elif defined(__APPLE__)
    nob_cmd_append(&cmd, "open", url);
#else
    nob_cmd_append(&cmd, "xdg-open", url);
#endif
    return nob_cmd_run(&cmd);
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    const char *program_name = nob_shift_args(&argc, &argv);

    bool release = false;
    bool run = false;
    bool serve = false;
    bool test = false;
    bool gen_test_form = false;
    const char *serve_port = "8000";
    while (argc > 0) {
        const char *arg = nob_shift_args(&argc, &argv);
        if (strcmp(arg, "release") == 0) {
            release = true;
        }
        else if (strcmp(arg, "run") == 0) {
            run = true;
        }
        else if (strcmp(arg, "serve") == 0) {
            serve = true;
            if (argc > 0 && is_port_arg(argv[0])) {
                serve_port = nob_shift_args(&argc, &argv);
            }
        }
        else if (strcmp(arg, "test") == 0) {
            test = true;
        }
        else if (strcmp(arg, "gen-test-form") == 0) {
            gen_test_form = true;
        }
        else {
            nob_log(NOB_ERROR, "Unknown arg %s", arg);
            nob_log(NOB_INFO, "Usage: %s [release] [run] [serve [port]] [test] [gen-test-form]", program_name);
            return 1;
        }
    }

    bool should_build_form = (!serve && !test && !gen_test_form) || run || release;
    if (should_build_form) {
        if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;

        Nob_Cmd cmd = {0};

        append_compile_flags(&cmd, release);
        nob_cmd_append(&cmd, "-o", BUILD_FOLDER"form");
        nob_cmd_append(&cmd, SRC_FOLDER"form.c");
        append_common_cli_sources(&cmd);

        if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;

        if (run) {
            nob_cmd_append(&cmd, BUILD_FOLDER"form");
            if (!nob_cmd_run(&cmd)) return 1;
        }
    }

    if (test || gen_test_form) {
        if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;
    }

    if (test) {
        Nob_Cmd cmd = {0};
        append_compile_flags(&cmd, false);
        nob_cmd_append(&cmd, "-o", BUILD_FOLDER"test");
        nob_cmd_append(&cmd, SRC_FOLDER"test.c");
        append_common_cli_sources(&cmd);

        if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;

        nob_cmd_append(&cmd, BUILD_FOLDER"test");
        if (!nob_cmd_run(&cmd)) return 1;
    }

    if (gen_test_form) {
        Nob_Cmd cmd = {0};
        append_compile_flags(&cmd, false);
        nob_cmd_append(&cmd, "-o", BUILD_FOLDER"generate_test_form");
        nob_cmd_append(&cmd, SRC_FOLDER"generate_test_form.c");

        if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;

        nob_cmd_append(&cmd, BUILD_FOLDER"generate_test_form");
        if (!nob_cmd_run(&cmd)) return 1;
    }

    if (serve) {
        const char *url = nob_temp_sprintf("http://localhost:%s/form.html?preview=true", serve_port);
        Nob_Cmd server_cmd = {0};
        Nob_Procs procs = {0};

        nob_log(NOB_INFO, "Serving repository root at %s", url);
        nob_cmd_append(&server_cmd, "python3", "-m", "http.server", serve_port);
        if (!nob_cmd_run(&server_cmd, .async = &procs)) return 1;

        if (!open_in_browser(url)) {
            nob_log(NOB_WARNING, "Failed to open browser automatically. Open %s manually.", url);
        }

        if (!nob_procs_wait(procs)) return 1;
    }

    return 0;
}
