#include <string.h>

#include <context.h>
#include "common.h"

picorbc_context*
picorbc_context_new(void)
{
  picorbc_context *c = (picorbc_context *)picorbc_alloc(sizeof(picorbc_context));
  memset(c, 0, sizeof(picorbc_context));
  return c;
}

void
picorbc_context_free(picorbc_context *cxt)
{
  picorbc_free(cxt->filename);
  picorbc_free(cxt->syms);
  picorbc_free(cxt);
}

void
picorbc_cleanup_local_variables(picorbc_context *c)
{
  if (c->syms) {
    picorbc_free(c->syms);
    c->syms = NULL;
    c->slen = 0;
  }
}

const char*
picorbc_filename(picorbc_context *c, const char *s)
{
  if (s) {
    size_t len = strlen(s);
    char *p = (char *)picorbc_alloc(len + 1);

    memcpy(p, s, len + 1);
    if (c->filename) {
      picorbc_free(c->filename);
    }
    c->filename = p;
  }
  return c->filename;
}

