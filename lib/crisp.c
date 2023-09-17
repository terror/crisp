#include <stdio.h>
#include <stdlib.h>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

#include "crisp.h"
#include "mpc.h"
#include "str_builder.h"

struct Value;
typedef struct Value Value;

Value* builtin(Value* a, char* func);
Value* env_get(Env* e, Value* k);
Value* eval(Env* e, Value* v);
Value* pop(Value* v, int i);
str_builder_t* to_string(Value* v);
void env_add_builtins(Env* e);

#define LASSERT(args, cond, err) if (!(cond)) { delete(args); return error(err); }

enum {
  ERROR,
  FUNCTION,
  NUMBER,
  QEXPR,
  SEXPR,
  SYMBOL
};

typedef Value* (*Builtin)(Env*, Value*);

struct Value {
  Builtin fun;
  char *error;
  char *symbol;
  int count;
  int type;
  long number;
  struct Value** cell;
};

struct Env {
  int count;
  char **symbols;
  Value **values;
};

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

Value* fun(Builtin func) {
  Value* v = malloc(sizeof(Value));
  v->type = FUNCTION;
  v->fun = func;
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
    case FUNCTION: break;
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
    case FUNCTION:
      str_builder_add_str(output, "<function>", 0);
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

Value* copy(Value* v) {
  Value* x = malloc(sizeof(Value));

  x->type = v->type;

  switch (v->type) {
    case ERROR:
      x->error = malloc(strlen(v->error) + 1);
      strcpy(x->error, v->error);
      break;
    case FUNCTION:
      x->fun = v->fun;
      break;
    case NUMBER:
      x->number = v->number;
      break;
    case SYMBOL:
      x->symbol = malloc(strlen(v->symbol) + 1);
      strcpy(x->symbol, v->symbol);
      break;
    case SEXPR:
    case QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(Value*) * x->count);
      for (int i = 0; i < x->count; ++i)
        x->cell[i] = copy(v->cell[i]);
      break;
  }

  return x;
}

Value* eval_op(Env* _e, Value* a, char* op) {
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

Value* eval_sexpr(Env* e, Value* v) {
  for (int i = 0; i < v->count; ++i)
    v->cell[i] = eval(e, v->cell[i]);

  for (int i = 0; i < v->count; ++i)
    if (v->cell[i]->type == ERROR) return take(v, i);

  if (v->count == 0) return v;
  if (v->count == 1) return take(v, 0);

  Value* f = pop(v, 0);

  if (f->type != FUNCTION) {
    delete(f);
    delete(v);
    return error("s-expression does not start with symbol");
  }

  Value* result = f->fun(e, v);

  delete(f);

  return result;
}

Value* eval(Env* e, Value* v) {
  if (v->type == SYMBOL) {
    Value* x = env_get(e, v);
    delete(v);
    return x;
  }

  return v->type == SEXPR ? eval_sexpr(e, v) : v;
}

Value* builtin_add(Env* e, Value* a) {
  return eval_op(e, a, "+");
}

Value* builtin_sub(Env* e, Value* a) {
  return eval_op(e, a, "-");
}

Value* builtin_mul(Env* e, Value* a) {
  return eval_op(e, a, "*");
}

Value* builtin_div(Env* e, Value* a) {
  return eval_op(e, a, "/");
}

Value* builtin_mod(Env* e, Value* a) {
  return eval_op(e, a, "%");
}

Value* builtin_cons(Env* e, Value *a) {
  LASSERT(a, a->count == 2, "Function 'cons' passed incorrect number of arguments");
  LASSERT(a, a->cell[1]->type == QEXPR, "Function 'cons' passed incorrect type");

  Value* x = pop(a, 0);
  Value* y = pop(a, 0);

  y->count++;

  y->cell = realloc(y->cell, sizeof(Value*) * y->count);

  memmove(&y->cell[1], &y->cell[0], sizeof(Value*) * (y->count - 1));

  y->cell[0] = x;

  return y;
}

Value* builtin_eval(Env* e, Value* a) {
  LASSERT(a, a->count == 1, "Function 'eval' passed too many arguments");
  LASSERT(a, a->cell[0]->type == QEXPR, "Function 'eval' passed incorrect type");

  Value* x = take(a, 0);
  x->type = SEXPR;

  return eval(e, x);
}

Value* builtin_head(Env* e, Value* a) {
  LASSERT(a, a->count == 1, "Function 'head' passed too many arguments");
  LASSERT(a, a->cell[0]->type == QEXPR, "Function 'head' passed incorrect type");
  LASSERT(a, a->cell[0]->count != 0, "Function 'head' passed {}");

  Value* v = take(a, 0);

  while (v->count > 1) delete(pop(v, 1));

  return v;
}

Value* builtin_init(Env* e, Value *a) {
  LASSERT(a, a->count == 1, "Function 'init' passed too many arguments");
  LASSERT(a, a->cell[0]->type == QEXPR, "Function 'init' passed incorrect type");
  LASSERT(a, a->cell[0]->count != 0, "Function 'init' passed {}");

  Value* x = take(a, 0);

  delete(pop(x, x->count - 1));

  return x;
}

Value* builtin_join(Env* e, Value* a) {
  for (int i = 0; i < a->count; ++i)
    LASSERT(a, a->cell[i]->type == QEXPR, "Function 'join' passed incorrect type");

  Value* x = pop(a, 0);

  while (a->count) x = add(x, pop(a, 0));

  delete(a);

  return x;
}

Value* builtin_len(Env* e, Value *a) {
  LASSERT(a, a->count == 1, "Function 'len' passed too many arguments");
  LASSERT(a, a->cell[0]->type == QEXPR, "Function 'len' passed incorrect type");

  Value* x = number(a->cell[0]->count);
  delete(a);

  return x;
}

Value* builtin_list(Env* e, Value* a) {
  a->type = QEXPR;
  return a;
}

Value* builtin_tail(Env* e, Value* a) {
  LASSERT(a, a->count == 1, "Function 'tail' passed too many arguments");
  LASSERT(a, a->cell[0]->type == QEXPR, "Function 'tail' passed incorrect type");
  LASSERT(a, a->cell[0]->count != 0, "Function 'tail' passed {}");

  Value* v = take(a, 0);

  delete(pop(v, 0));

  return v;
}

Env* env_new(void) {
  Env* e = malloc(sizeof(Env));
  e->count = 0;
  e->symbols = NULL;
  e->values = NULL;
  env_add_builtins(e);
  return e;
}

void env_delete(Env* e) {
  for (int i = 0; i < e->count; ++i) {
    free(e->symbols[i]);
    delete(e->values[i]);
  }
  free(e->symbols);
  free(e->values);
  free(e);
}

Value* env_get(Env* e, Value* k) {
  for (int i = 0; i < e->count; ++i)
    if (strcmp(e->symbols[i], k->symbol) == 0)
      return copy(e->values[i]);
  return error("Unbound symbol");
}

void env_put(Env *e, Value* k, Value* v) {
  for (int i = 0; i < e->count; ++i) {
    if (strcmp(e->symbols[i], k->symbol) == 0) {
      delete(e->values[i]);
      e->values[i] = copy(v);
      return;
    }
  }

  e->count++;
  e->values = realloc(e->values, sizeof(Value*) * e->count);
  e->symbols = realloc(e->symbols, sizeof(char*) * e->count);

  e->values[e->count - 1] = copy(v);
  e->symbols[e->count - 1] = malloc(strlen(k->symbol) + 1);
  strcpy(e->symbols[e->count - 1], k->symbol);
}

void env_add_builtin(Env* e, char* name, Builtin func) {
  Value* k = symbol(name);
  Value* v = fun(func);
  env_put(e, k, v);
  delete(k);
  delete(v);
}

void env_add_builtins(Env* e) {
  env_add_builtin(e, "%", builtin_mod);
  env_add_builtin(e, "*", builtin_mul);
  env_add_builtin(e, "+", builtin_add);
  env_add_builtin(e, "-", builtin_sub);
  env_add_builtin(e, "/", builtin_div);
  env_add_builtin(e, "cons", builtin_cons);
  env_add_builtin(e, "eval", builtin_eval);
  env_add_builtin(e, "head", builtin_head);
  env_add_builtin(e, "init", builtin_init);
  env_add_builtin(e, "join", builtin_join);
  env_add_builtin(e, "len", builtin_len);
  env_add_builtin(e, "list", builtin_list);
  env_add_builtin(e, "tail", builtin_tail);
}

#ifdef EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
char* run(char* input, Env* e) {
  if (e == NULL) e = env_new();

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
      symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&%]+/ ;  \
      sexpr : '(' <expr>* ')' ; \
      qexpr : '{' <expr>* '}' ; \
      expr : <number> | <symbol> | <sexpr> | <qexpr> ; \
      program : /^/ <expr>* /$/ ; \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Program
  );

  mpc_result_t result;

  if (mpc_parse("<stdin>", input, Program, &result)) {
    Value* x = eval(e, read(result.output));
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
