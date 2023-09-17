#include <criterion/criterion.h>
#include <criterion/new/assert.h>

#include "../lib/crisp.h"

Test(unit, math) {
  cr_assert(eq(str, run("(+ (% 3 2) (* 5 5 (+ 1 (/ 10 5))))", NULL), "76"));
}
