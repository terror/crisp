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

enum {
  ERROR,
  NUMBER,
  SEXPR,
  SYMBOL
};

typedef struct Value {
  char *error;
  char *symbol;
  int count;
  int type;
  long number;
  struct Value** cell;
} Value;

void print(Value* v);
Value* eval(Value* v);

Value* error(char *message) {
  Value* v = malloc(sizeof(Value));
  v->type = ERROR;
  v->error = malloc(strlen(message) + 1);
  strcpy(v->error, message);
  return v;
}

Value* number(long x) {
  Value* v = malloc(sizeof(Value));
  v->type = NUMBER;
  v->number = x;
  return v;
}

Value* parse_number(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? number(x) : error("Invalid number");
}

Value* symbol(char *s) {
  Value* v = malloc(sizeof(Value));
  v->type = SYMBOL;
  v->symbol = malloc(strlen(s) + 1);
  strcpy(v->symbol, s);
  return v;
}

Value* sexpr(void) {
  Value* v = malloc(sizeof(Value));
  v->type = SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

Value* add(Value *a, Value *b) {
  a->count++;
  a->cell = realloc(a->cell, sizeof(Value*) * a->count);
  a->cell[a->count - 1] = b;
  return a;
}

Value* read(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) return parse_number(t);
  if (strstr(t->tag, "symbol")) return symbol(t->contents);

  Value* x = NULL;

  if (strcmp(t->tag, ">") == 0 || strstr(t->tag, "sexpr"))
    x = sexpr();

  for (int i = 0; i < t->children_num; ++i) {
    if (
      strcmp(t->children[i]->contents, "(") == 0
        || strcmp(t->children[i]->contents, ")") == 0
        || strcmp(t->children[i]->contents, "}") == 0
        || strcmp(t->children[i]->contents, "{") == 0
        || strcmp(t->children[i]->tag, "regex") == 0
    ) continue;
    x = add(x, read(t->children[i]));
  }

  return x;
}

void delete(Value* v) {
  switch (v->type) {
    case ERROR: free(v->error); break;
    case NUMBER: break;
    case SEXPR:
      for (int i = 0; i < v->count; ++i)
        delete(v->cell[i]);
      free(v->cell);
      break;
    case SYMBOL: free(v->symbol); break;
  }

  free(v);
}

void print_expr(Value* v, char open, char close) {
  putchar(open);

  for (int i = 0; i < v->count; ++i) {
    print(v->cell[i]);
    if (i != (v->count - 1)) putchar(' ');
  }

  putchar(close);
}

void print(Value* v) {
  switch (v->type) {
    case ERROR: printf("error: %s", v->error); break;
    case NUMBER: printf("%li", v->number); break;
    case SEXPR: print_expr(v, '(', ')'); break;
    case SYMBOL: printf("%s", v->symbol); break;
  }
}

void println(Value* v) {
  print(v);
  putchar('\n');
}

Value* pop(Value* v, int i) {
  Value* x = v->cell[i];
  memmove(&v->cell[i], &v->cell[i + 1], sizeof(Value*) * (v->count - i - 1));
  v->count--;
  v->cell = realloc(v->cell, sizeof(Value*) * v->count);
  return x;
}

Value* take(Value* v, int i) {
  Value* x = pop(v, i);
  delete(v);
  return x;
}

Value* eval_op(Value* a, char* op) {
  for (int i = 0; i < a->count; ++i)
    if (a->cell[i]->type != NUMBER)
      return error("Cannot operate on non-number");

  Value* x = pop(a, 0);

  if (strcmp(op, "-") == 0 && a->count == 0)
    x->number = -x->number;

  while (a->count > 0) {
    Value* y = pop(a, 0);

    if (strcmp(op, "+") == 0) x->number += y->number;
    if (strcmp(op, "-") == 0) x->number -= y->number;
    if (strcmp(op, "*") == 0) x->number *= y->number;
    if (strcmp(op, "/") == 0) {
      if (y->number == 0) {
        delete(x);
        delete(y);
        x = error("Division by zero");
        break;
      }
      x->number /= y->number;
    }

    delete(y);
  }

  delete(a);

  return x;
}

Value* eval_sexpr(Value* v) {
  for (int i = 0; i < v->count; ++i)
    v->cell[i] = eval(v->cell[i]);

  for (int i = 0; i < v->count; ++i)
    if (v->cell[i]->type == ERROR) return take(v, i);

  if (v->count == 0) return v;
  if (v->count == 1) return take(v, 0);

  Value* f = pop(v, 0);

  if (f->type != SYMBOL) {
    delete(f);
    delete(v);
    return error("s-expression does not start with symbol");
  }

  Value* result = eval_op(v, f->symbol);

  delete(f);

  return result;
}

Value* eval(Value* v) {
  return v->type == SEXPR ? eval_sexpr(v) : v;
}

int main() {
  puts(":: crisp ::");

  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Program = mpc_new("program");

  mpca_lang(
    MPCA_LANG_DEFAULT,
    " \
      number : /-?[0-9]+/ ; \
      symbol : '+' | '-' | '*' | '/' ; \
      sexpr : '(' <expr>* ')' ; \
      expr : <number> | <symbol> | <sexpr> ; \
      program : /^/ <expr>* /$/ ; \
    ",
    Number, Symbol, Sexpr, Expr, Program
  );

  while (1) {
    char *input = readline("> ");

    add_history(input);

    mpc_result_t result;

    if (mpc_parse("<stdin>", input, Program, &result)) {
      Value* x = eval(read(result.output));
      println(x);
      delete(x);
      mpc_ast_delete(result.output);
    } else {
      mpc_err_print(result.error);
      mpc_err_delete(result.error);
    }

    free(input);
  }

  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Program);

  return 0;
}
