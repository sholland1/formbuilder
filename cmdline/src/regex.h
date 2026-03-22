#ifndef REGEX_H
#define REGEX_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "nob.h"

struct RegexContext;

typedef struct {
    int bytecode_len;
    uint8_t *bytecode;
} CompiledRegex;

CompiledRegex compile_regex(const char *pattern, int flags);
void free_compiled_regex(CompiledRegex *regex);
bool is_match(const CompiledRegex *regex, const char *str);
bool is_sv_match(const CompiledRegex *regex, Nob_String_View sv);

// int match_with_captures(....);

#endif
