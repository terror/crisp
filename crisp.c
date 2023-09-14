#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>

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

int main() {
  puts(":: crisp ::");

  while (1) {
    char *input = readline("> ");
    add_history(input);
    printf(":: %s\n", input);
    free(input);
  }

  return 0;
}
