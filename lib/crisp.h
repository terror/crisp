#ifndef crisp_h
#define crisp_h

struct Env;
typedef struct Env Env;

char* run(char* input, Env* e);

Env* env_new(void);
void env_delete(Env* env);

#endif
