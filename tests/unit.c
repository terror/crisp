#include <criterion/criterion.h>
#include <criterion/new/assert.h>

#include "../lib/crisp.h"

Test(unit, math) {
  cr_assert(eq(str, run("(+ (% 3 2) (* 5 5 (+ 1 (/ 10 5))))", NULL), "76"));
}

Test(unit, cons) {
  cr_assert(eq(str,
               run("(cons 1 (cons 2 (cons 3 (cons 4 (cons 5 (cons 6 (cons 7 "
                   "(cons 8 (cons 9 (cons 10 {}))))))))))",
                   NULL),
               "{1 2 3 4 5 6 7 8 9 10}"));
}

Test(unit, head) {
  cr_assert(eq(str, run("(head {1 2 3 4 5 6 7 8 9 10})", NULL), "{1}"));
}

Test(unit, tail) {
  cr_assert(eq(str, run("(tail {1 2 3 4 5 6 7 8 9 10})", NULL),
               "{2 3 4 5 6 7 8 9 10}"));
}

Test(unit, init) {
  cr_assert(eq(str, run("(init {1 2 3 4 5 6 7 8 9 10})", NULL),
               "{1 2 3 4 5 6 7 8 9}"));
}

Test(unit, list) {
  cr_assert(eq(str, run("(list 1 2 3 4 5 6 7 8 9 10)", NULL),
               "{1 2 3 4 5 6 7 8 9 10}"));
}

Test(unit, len) {
  cr_assert(eq(str, run("(len {1 2 3 4 5 6 7 8 9 10})", NULL), "10"));
}

Test(unit, join) {
  cr_assert(eq(str, run("(join {1 2 3 4 5} {6 7 8 9 10})", NULL),
               "{1 2 3 4 5 6 7 8 9 10}"));
}

Test(unit, eval) {
  cr_assert(eq(str, run("(eval (cons + (cons 1 (cons 2 {}))))", NULL), "3"));
}

Test(unit, partial_evaluation) {
  Env* env = env_new();

  cr_assert(eq(str, run("def {add} (\\ {x y} {+ x y})", env), "()"));
  cr_assert(eq(str, run("add 1 2", env), "3"));
  cr_assert(eq(str, run("add 1", env), "(\\ {y} {+ x y})"));
  cr_assert(eq(str, run("def {add-one} (add 1)", env), "()"));
  cr_assert(eq(str, run("add-one 1", env), "2"));
}

Test(unit, comparison) {
  cr_assert(eq(str, run("(== 1 1)", NULL), "1"));
  cr_assert(eq(str, run("(== 1 2)", NULL), "0"));
  cr_assert(eq(str, run("(< 1 2)", NULL), "1"));
  cr_assert(eq(str, run("(< 2 1)", NULL), "0"));
  cr_assert(eq(str, run("(> 1 2)", NULL), "0"));
  cr_assert(eq(str, run("(> 2 1)", NULL), "1"));
  cr_assert(eq(str, run("(<= 1 2)", NULL), "1"));
  cr_assert(eq(str, run("(<= 2 1)", NULL), "0"));
  cr_assert(eq(str, run("(>= 1 2)", NULL), "0"));
  cr_assert(eq(str, run("(>= 2 1)", NULL), "1"));
}

Test(unit, if_statement) {
  cr_assert(eq(str, run("(if (== 1 1) {1} {0})", NULL), "1"));
  cr_assert(eq(str, run("(if (== 1 2) {1} {0})", NULL), "0"));
}
