/*
** mrb_picocompiler.c - PicoCompiler class
**
** Copyright (c) HASUMI Hitoshi 2021
**
** See Copyright Notice in LICENSE
*/

#include "mruby.h"
#include "mruby/data.h"
#include "mrb_picocompiler.h"

#define DONE mrb_gc_arena_restore(mrb, 0);

typedef struct {
  char *str;
  mrb_int len;
} mrb_picocompiler_data;

static const struct mrb_data_type mrb_picocompiler_data_type = {
  "mrb_picocompiler_data", mrb_free,
};

static mrb_value mrb_picocompiler_init(mrb_state *mrb, mrb_value self)
{
  mrb_picocompiler_data *data;
  char *str;
  mrb_int len;

  data = (mrb_picocompiler_data *)DATA_PTR(self);
  if (data) {
    mrb_free(mrb, data);
  }
  DATA_TYPE(self) = &mrb_picocompiler_data_type;
  DATA_PTR(self) = NULL;

  mrb_get_args(mrb, "s", &str, &len);
  data = (mrb_picocompiler_data *)mrb_malloc(mrb, sizeof(mrb_picocompiler_data));
  data->str = str;
  data->len = len;
  DATA_PTR(self) = data;

  return self;
}

static mrb_value mrb_picocompiler_hello(mrb_state *mrb, mrb_value self)
{
  mrb_picocompiler_data *data = DATA_PTR(self);

  return mrb_str_new(mrb, data->str, data->len);
}

static mrb_value mrb_picocompiler_hi(mrb_state *mrb, mrb_value self)
{
  return mrb_str_new_cstr(mrb, "hi!!");
}

void mrb_mruby_pico_compiler_gem_init(mrb_state *mrb)
{
  struct RClass *picocompiler;
  picocompiler = mrb_define_class(mrb, "PicoCompiler", mrb->object_class);
  mrb_define_method(mrb, picocompiler, "initialize", mrb_picocompiler_init, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, picocompiler, "hello", mrb_picocompiler_hello, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, picocompiler, "hi", mrb_picocompiler_hi, MRB_ARGS_NONE());
  DONE;
}

void mrb_mruby_pico_compiler_gem_final(mrb_state *mrb)
{
}

