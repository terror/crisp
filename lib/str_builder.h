#ifndef str_builder_h
#define str_builder_h

#include <stddef.h>

struct str_builder;
typedef struct str_builder str_builder_t;

str_builder_t* str_builder_create(void);
void str_builder_destroy(str_builder_t* sb);
void str_builder_add_str(str_builder_t* sb, const char* str, size_t len);
void str_builder_add_builder(str_builder_t* sb, str_builder_t* x, size_t len);
void str_builder_add_char(str_builder_t* sb, char c);
void str_builder_add_int(str_builder_t* sb, int val);
void str_builder_clear(str_builder_t* sb);
void str_builder_truncate(str_builder_t* sb, size_t len);
void str_builder_drop(str_builder_t* sb, size_t len);
char *str_builder_dump(const str_builder_t* sb, size_t* len);
#endif
