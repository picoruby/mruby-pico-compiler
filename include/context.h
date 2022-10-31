#ifndef PICORBC_CONTEXT_H_
#define PICORBC_CONTEXT_H_

#include <stdint.h>

typedef uint8_t mrb_bool;
typedef uint32_t mrb_sym;

struct mrb_parser_state;

/* from compile.h */
typedef struct mrbc_context {
  mrb_sym *syms;
  int slen;
  char *filename;
  uint16_t lineno;
  int (*partial_hook)(struct mrb_parser_state*);
  void *partial_data;
  struct RClass *target_class;
  mrb_bool capture_errors:1;
  mrb_bool dump_result:1;
  mrb_bool no_exec:1;
  mrb_bool keep_lv:1;
  mrb_bool no_optimize:1;
  const struct RProc *upper;

  size_t parser_nerr;
} mrbc_context;

typedef mrbc_context picorbc_context;

picorbc_context* picorbc_context_new(picorbc_context *cxt);

void picorbc_context_free(picorbc_context *cxt);

void picorbc_cleanup_local_variables(picorbc_context *c);

const char* picorbc_filename(picorbc_context *c, const char *s);

#endif /* PICORBC_CONTEXT_H_*/
