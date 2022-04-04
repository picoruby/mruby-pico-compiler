#include <string.h>
#include <stdbool.h>

#include <context.h>
#include "common.h"
#include "scope.h"

picorbc_context*
picorbc_context_new(void)
{
  picorbc_context *cxt = (picorbc_context *)picorbc_alloc(sizeof(picorbc_context));
  memset(cxt, 0, sizeof(picorbc_context));
  return cxt;
}

void
picorbc_context_free(picorbc_context *cxt)
{
  picorbc_free(cxt->filename);
  Scope_freeLvar((Lvar *)cxt->syms);
  picorbc_free(cxt);
}

void
picorbc_cleanup_local_variables(picorbc_context *cxt)
{
  if (cxt->syms) {
    Scope_freeLvar((Lvar *)cxt->syms);
    cxt->syms = NULL;
    cxt->slen = 0;
  }
}

const char*
picorbc_filename(picorbc_context *cxt, const char *s)
{
  if (s) {
    size_t len = strlen(s);
    char *p = (char *)picorbc_alloc(len + 1);

    memcpy(p, s, len + 1);
    if (cxt->filename) {
      picorbc_free(cxt->filename);
    }
    cxt->filename = p;
  }
  return cxt->filename;
}

