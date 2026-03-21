#ifndef FORM_APP_H
#define FORM_APP_H

#include "form_cli.h"

bool load_form_from_file(const char *file_path, Form *form);
void display_form(const Form *form, Answers *answers);
void output_answers(const Answers *answers, int pretty_print, FILE *stream);
void output_form(const Form *form, int pretty_print, FILE *stream);

#endif
