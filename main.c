#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/crisp.h"

#ifdef _WIN32
static char buffer[2048];

char *readline(char *prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\0';
  return cpy;
}

void add_history(char *unused) {}
#elif __APPLE__
#include <editline/readline.h>
#elif __linux
#include <editline/history.h>
#include <editline/readline.h>
#endif

int main() {
  Env *env = env_new();

  for (;;) {
    char *input = readline("> ");
    if (strcmp(input, "exit") == 0)
      break;
    add_history(input);
    printf("%s\n", run(input, env));
    free(input);
  }

  env_delete(env);

  return 0;
}
