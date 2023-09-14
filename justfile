set dotenv-load

export EDITOR := 'nvim'

default:
  just --list

clean:
  rm -rf a.out

forbid:
  ./bin/forbid

run:
  gcc -std=c99 -Wall crisp.c -lreadline && ./a.out
