#ifndef _LEGACY_HPP_
#define _LEGACY_HPP_

typedef enum {
  BM_ITER,
  BM_REC,
} brute_mode_t;

#define MAX_PASSWORD_LENGTH (4)
typedef char password_t[MAX_PASSWORD_LENGTH + 1];

typedef struct result_t {
  int id;
  int idx;
  bool found;
  password_t password;
} result_t;

typedef struct task_t {
  result_t result;
  char * alph;
  char * hash;
  int len;
  int from;
  int to;
  int prefix;
  brute_mode_t brute_mode;
} task_t;

#endif /* _LEGACY_HPP_ */
