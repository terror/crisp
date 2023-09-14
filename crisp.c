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

typedef struct {
  int err;
  int type;
  long num;
} lval;

enum { LVAL_NUM, LVAL_ERR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

void lval_print(lval v) {
  switch (v.type) {
    case LVAL_NUM:
      printf("%li", v.num);
      break;
    case LVAL_ERR:
      if (v.err == LERR_DIV_ZERO)
        printf("error: Division by zero");
      if (v.err == LERR_BAD_OP)
        printf("error: Invalid operator");
      if (v.err == LERR_BAD_NUM)
        printf("error: Invalid number");
      break;
  }
}

void lval_println(lval v) {
  lval_print(v);
  putchar('\n');
}

lval eval_op(lval x, char *op, lval y) {
  if (x.type == LVAL_ERR) return x;
  if (y.type == LVAL_ERR) return y;

  if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
  if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
  if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }

  if (strcmp(op, "/") == 0)
    return y.num == 0 ?
      lval_err(LERR_DIV_ZERO) :
      lval_num(x.num / y.num);

  return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  char* op = t->children[1]->contents;
  lval x = eval(t->children[2]);

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
      lval output = eval(result.output);
      lval_println(output);
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
