set dotenv-load

export EDITOR := 'nvim'

alias r := run

default:
  just --list

clean:
  rm -rf a.out

dev-deps:
  brew install emscripten criterion ripgrep

fmt:
  clang-format -i -style=file:.clang-format main.c lib/*.c
  prettier --write .

fmt-check:
  clang-format --dry-run --Werror main.c lib/*.c
  prettier --check .

forbid:
  ./bin/forbid

run:
  gcc -std=c99 -Wall main.c lib/*.c -lreadline -lm && ./a.out

serve:
  python3 -m http.server 8000 --directory ./www

test:
  gcc tests/unit.c lib/*.c -I/opt/homebrew/include -L/opt/homebrew/lib -lcriterion && ./a.out

wasm:
  emcc lib/*.c \
    -s EXPORTED_FUNCTIONS="['_malloc', '_free']" \
    -s EXPORTED_RUNTIME_METHODS="['ccall', 'stringToUTF8', 'UTF8ToString']" \
    -s WASM=1 \
    -o www/index.js
