set dotenv-load

export EDITOR := 'nvim'

alias r := run

default:
  just --list

clean:
  rm -rf a.out

dev-deps:
  brew tap oven-sh/bun
  brew install bun emscripten

fmt:
  prettier --write .

forbid:
  ./bin/forbid

run:
  gcc -std=c99 -Wall main.c lib/*.c -lreadline -lm && ./a.out

serve:
	python3 -m http.server 8000 --directory ./www

wasm:
  emcc lib/crisp.c lib/str_builder.c lib/mpc.c \
    -s EXPORTED_FUNCTIONS="['_malloc', '_free']" \
    -s EXPORTED_RUNTIME_METHODS="['ccall', 'stringToUTF8', 'UTF8ToString']" \
    -s WASM=1 \
    -o www/index.js
