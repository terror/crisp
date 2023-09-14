#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>
#include "lib/mpc.h"

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

void add_history(char* unused) {}
#elif __APPLE__
#include <editline/readline.h>
#elif __linux
#include <editline/readline.h>
#include <editline/history.h>
#endif

long eval_op(long x, char* op, long y) {
  if (strcmp(op, "+") == 0) { return x + y; }
  if (strcmp(op, "-") == 0) { return x - y; }
  if (strcmp(op, "*") == 0) { return x * y; }
  if (strcmp(op, "/") == 0) { return x / y; }
  return 0;
}

long eval(mpc_ast_t* t) {
  if (strstr(t->tag, "number"))
    return atoi(t->contents);

  char* op = t->children[1]->contents;
  long x = eval(t->children[2]);

  int i = 3;

  while (strstr(t->children[i]->tag, "expression")) {
    x = eval_op(x, op, eval(t->children[i]));
    ++i;
  }

  return x;
}

int main() {
  puts(":: crisp ::");

  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expression = mpc_new("expression");
  mpc_parser_t* Program = mpc_new("program");

  mpca_lang(
    MPCA_LANG_DEFAULT,
    " \
      number : /-?[0-9]+/ ; \
      operator: '+' | '-' | '*' | '/' ; \
      expression: <number> | '(' <operator> <expression>+ ')' ; \
      program: /^/ <operator> <expression>+ /$/ ; \
    ",
    Number, Operator, Expression, Program
  );

  while (1) {
    char *input = readline("> ");

    add_history(input);

    mpc_result_t result;

    if (mpc_parse("<stdin>", input, Program, &result)) {
      long output = eval(result.output);
      printf("%li\n", output);
      mpc_ast_delete(result.output);
    } else {
      mpc_err_print(result.error);
      mpc_err_delete(result.error);
    }

    free(input);
  }

  mpc_cleanup(4, Number, Operator, Expression, Program);

  return 0;
}