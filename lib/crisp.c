#include <stdio.h>
#include <stdlib.h>

#include "str_builder.h"
#include "mpc.h"

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

enum {
  ERROR,
  NUMBER,
  QEXPR,
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

str_builder_t* to_string(Value* v);
Value* eval(Value* v);
Value* pop(Value* v, int i);
Value* builtin(Value* a, char* func);
Value* builtin_eval(Value* a);

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

Value* qexpr(void) {
  Value* v = malloc(sizeof(Value));
  v->type = QEXPR;
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

  if (strstr(t->tag, "qexpr")) x = qexpr();

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
    case QEXPR:
      for (int i = 0; i < v->count; ++i)
        delete(v->cell[i]);
      free(v->cell);
      break;
    case SYMBOL: free(v->symbol); break;
  }

  free(v);
}

Value* join(Value* x, Value* y) {
  while (y->count) x = add(x, pop(y, 0));
  delete(y);
  return x;
}

str_builder_t* to_string_helper(Value* v, char open, char close) {
  str_builder_t* output = str_builder_create();

  str_builder_add_char(output, open);

  for (int i = 0; i < v->count; ++i) {
    str_builder_add_builder(output, to_string(v->cell[i]), 0);
    if (i != (v->count - 1)) str_builder_add_char(output, ' ');
  }

  str_builder_add_char(output, close);

  return output;
}

str_builder_t* to_string(Value* v) {
  str_builder_t* output = str_builder_create();

  switch (v->type) {
    case ERROR:
      str_builder_add_str(output, "error: ", 0);
      str_builder_add_str(output, v->error, 0);
      break;
    case NUMBER:
      str_builder_add_int(output, v->number);
      break;
    case SEXPR:
      str_builder_add_builder(output, to_string_helper(v, '(', ')'), 0);
      break;
    case QEXPR:
      str_builder_add_builder(output, to_string_helper(v, '{', '}'), 0);
      break;
    case SYMBOL:
      str_builder_add_str(output, v->symbol, 0);
      break;
  }

  return output;
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
    if (strcmp(op, "%") == 0) x->number %= y->number;

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

  Value* result = builtin(v, f->symbol);

  delete(f);

  return result;
}

Value* eval(Value* v) {
  return v->type == SEXPR ? eval_sexpr(v) : v;
}

#define LASSERT(args, cond, err) \
  if (!(cond)) { \
    delete(args); \
    return error(err); \
  }

Value* builtin_head(Value* a) {
  LASSERT(a, a->count == 1, "Function 'head' passed too many arguments");
  LASSERT(a, a->cell[0]->type == QEXPR, "Function 'head' passed incorrect type");
  LASSERT(a, a->cell[0]->count != 0, "Function 'head' passed {}");

  Value* v = take(a, 0);

  while (v->count > 1) delete(pop(v, 1));

  return v;
}

Value* builtin_tail(Value* a) {
  LASSERT(a, a->count == 1, "Function 'tail' passed too many arguments");
  LASSERT(a, a->cell[0]->type == QEXPR, "Function 'tail' passed incorrect type");
  LASSERT(a, a->cell[0]->count != 0, "Function 'tail' passed {}");

  Value* v = take(a, 0);

  delete(pop(v, 0));

  return v;
}

Value* builtin_list(Value* a) {
  a->type = QEXPR;
  return a;
}

Value* builtin_join(Value* a) {
  for (int i = 0; i < a->count; ++i)
    LASSERT(a, a->cell[i]->type == QEXPR, "Function 'join' passed incorrect type");

  Value* x = pop(a, 0);

  while (a->count) x = add(x, pop(a, 0));

  delete(a);

  return x;
}

Value* builtin(Value* a, char* func) {
  if (strcmp("list", func) == 0) return builtin_list(a);
  if (strcmp("head", func) == 0) return builtin_head(a);
  if (strcmp("tail", func) == 0) return builtin_tail(a);
  if (strcmp("eval", func) == 0) return builtin_eval(a);
  if (strcmp("join", func) == 0) return builtin_join(a);
  if (strstr("+-/*", func)) return eval_op(a, func);
  delete(a);
  return error("Unknown function");
}

Value* builtin_eval(Value* a) {
  LASSERT(a, a->count == 1, "Function 'eval' passed too many arguments");
  LASSERT(a, a->cell[0]->type == QEXPR, "Function 'eval' passed incorrect type");

  Value* x = take(a, 0);
  x->type = SEXPR;

  return eval(x);
}

#ifdef EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
char* run(char* input) {
  char* output;

  str_builder_t* sb = str_builder_create();

  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Qexpr = mpc_new("qexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Program = mpc_new("program");

  mpca_lang(
    MPCA_LANG_DEFAULT,
    " \
      number : /-?[0-9]+/ ; \
      symbol : \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\" | '+' | '-' | '*' | '/' | '%' ; \
      sexpr : '(' <expr>* ')' ; \
      qexpr : '{' <expr>* '}' ; \
      expr : <number> | <symbol> | <sexpr> | <qexpr> ; \
      program : /^/ <expr>* /$/ ; \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Program
  );

  mpc_result_t result;

  if (mpc_parse("<stdin>", input, Program, &result)) {
    Value* x = eval(read(result.output));
    str_builder_add_builder(sb, to_string(x), 0);
    delete(x);
    mpc_ast_delete(result.output);
  } else {
    str_builder_add_str(sb, mpc_err_string(result.error), 0);
    mpc_err_delete(result.error);
  }

  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Program);

  output = str_builder_dump(sb, NULL);
  str_builder_destroy(sb);

  return output;
}
