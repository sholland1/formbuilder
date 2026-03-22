#include "regex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libregexp/libregexp.h"

typedef struct {
    int dummy;
} RegexContext;

// Minimal required callbacks (stubs)
int lre_check_stack_overflow(void *opaque, size_t alloca_size) {
    (void)opaque; (void)alloca_size;
    return 0;
}

int lre_check_timeout(void *opaque) {
    (void)opaque;
    return 0;
}

void *lre_realloc(void *opaque, void *ptr, size_t size) {
    (void)opaque;
    return realloc(ptr, size);
}

CompiledRegex compile_regex(const char *pattern, int flags) {
    char error_msg[128] = {0};
    int bytecode_len = 0;
    RegexContext ctx = {0};

    uint8_t *bytecode = lre_compile(&bytecode_len,
        error_msg, sizeof(error_msg),
        pattern, strlen(pattern),
        flags, &ctx);

    if (!bytecode) {
        fprintf(stderr, "Regex compile error: %s\n", error_msg);
        exit(EXIT_FAILURE);
    }

    return (CompiledRegex){
        .bytecode     = bytecode,
        .bytecode_len = bytecode_len,
    };
}

void free_compiled_regex(CompiledRegex *regex) {
    if (regex && regex->bytecode) {
        free(regex->bytecode);
        regex->bytecode = NULL;
        regex->bytecode_len = 0;
    }
}

bool is_sv_match(const CompiledRegex *regex, Nob_String_View sv) {
    if (!regex || !regex->bytecode) return true;

    RegexContext ctx = {0};

    // We don't need captures in this simple use-case
    uint8_t *ignore_captures[32] = {0};

    int ret = lre_exec(ignore_captures,
        regex->bytecode,
        (const uint8_t *)sv.data,
        0, sv.count,
        0, &ctx);

    return ret > 0;
}

bool is_match(const CompiledRegex *regex, const char *str) {
    if (!regex || !regex->bytecode || !str) return true;
    return is_sv_match(regex, nob_sv_from_cstr(str));
}
