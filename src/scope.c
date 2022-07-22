#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <common.h>
#include <debug.h>
#include <scope.h>

void generateCodePool(Scope *self, uint16_t size)
{
  CodePool *pool = (CodePool *)picorbc_alloc(sizeof(CodePool) - IREP_HEADER_SIZE + size);
  pool->size = size;
  pool->index = 0;
  pool->next = NULL;
  if (self->current_code_pool)
    self->current_code_pool->next = pool;
  self->current_code_pool = pool;
}

Scope *Scope_new(Scope *upper, bool lvar_top)
{
  Scope *self = picorbc_alloc(sizeof(Scope));
  memset(self, 0, sizeof(Scope));
  self->upper = upper;
  self->lvar_top = lvar_top;
  if (upper != NULL) {
    if (upper->first_lower == NULL) {
      upper->first_lower = self;
    } else {
      Scope *prev = upper->first_lower;
      for (int i = 1; i < upper->nlowers; i++) prev = prev->next;
      prev->next = self;
    }
    upper->nlowers++;
  }
  generateCodePool(self, IREP_HEADER_SIZE);
  self->first_code_pool = self->current_code_pool;
  self->first_code_pool->index = IREP_HEADER_SIZE;
  self->nlocals = 1;
  self->sp = 1;
  self->rlen = 1;
  self->exc_handler = NULL;
  return self;
}

void Scope_pushBackpatch(Scope *self, JmpLabel *label)
{
  Backpatch *bp = picorbc_alloc(sizeof(Backpatch));
  bp->next = NULL;
  bp->label = label;
  if (!self->backpatch) {
    self->backpatch = bp;
    return;
  }
  Backpatch *tmp = self->backpatch;
  while (tmp->next) tmp = tmp->next;
  tmp->next = bp;
}

void Scope_shiftBackpatch(Scope *self)
{
  if (!self->backpatch) return;
  Backpatch *bp = self->backpatch;
  self->backpatch = self->backpatch->next;
  picorbc_free(bp);
}

void freeLiteralRcsv(Literal *literal)
{
  if (literal == NULL) return;
  freeLiteralRcsv(literal->next);
  picorbc_free(literal);
}

void freeGenLiteralRcsv(GenLiteral *gen_literal)
{
  if (gen_literal == NULL) return;
  freeGenLiteralRcsv(gen_literal->prev);
  picorbc_free((void *)gen_literal->value);
  picorbc_free(gen_literal);
}

void freeSymbolRcsv(Symbol *symbol)
{
  if (symbol == NULL) return;
  freeSymbolRcsv(symbol->next);
  picorbc_free(symbol);
}

void Scope_freeLvar(Lvar *lvar)
{
  if (lvar == NULL) return;
  Scope_freeLvar(lvar->next);
  if (lvar->to_be_free) picorbc_free((void *)lvar->name);
  picorbc_free(lvar);
}

void freeAssignSymbol(AssignSymbol *assign_symbol)
{
  AssignSymbol *prev;
  while (assign_symbol) {
    prev = assign_symbol->prev;
    picorbc_free((void *)assign_symbol->value);
    picorbc_free(assign_symbol);
    assign_symbol = prev;
  }
}

void Scope_free(Scope *self)
{
  if (self == NULL) return;
  Scope_free(self->next);
  Scope_free(self->first_lower);
  freeLiteralRcsv(self->literal);
  freeGenLiteralRcsv(self->gen_literal);
  freeSymbolRcsv(self->symbol);
  Scope_freeLvar(self->lvar);
  freeAssignSymbol(self->last_assign_symbol);
  if (self->upper == NULL) {
    picorbc_free(self->vm_code);
  }
  picorbc_free(self);
}

void Scope_pushNCode_self(Scope *self, uint8_t *str, int size)
{
  CodePool *pool;
  if (size > CODE_POOL_SIZE)
    generateCodePool(self, size);
  if (self->current_code_pool->index + size > self->current_code_pool->size)
    generateCodePool(self, CODE_POOL_SIZE);
  pool = self->current_code_pool;
  memcpy(&pool->data[pool->index], str, size);
  pool->index += size;
  self->vm_code_size = self->vm_code_size + size;
}


void Scope_pushCode_self(Scope *self, int val)
{
  uint8_t str[1];
  str[0] = (uint8_t)val;
  Scope_pushNCode_self(self, str, 1);
}

static Literal *literal_new(const char *value, LiteralType type)
{
  Literal *literal = picorbc_alloc(sizeof(Literal));
  literal->next = NULL;
  literal->type = type;
  literal->value = value;
  return literal;
}

/*
 * returns -1 if literal was not found
 */
int literal_findIndex(Literal *literal, const char *value, LiteralType type)
{
  int i = 0;
  while (literal != NULL) {
    if (literal->type == type && strcmp(literal->value, value) == 0) {
      return i;
    }
    literal = literal->next;
    i++;
  }
  return -1;
}

int Scope_newLit(Scope *self, const char *value, LiteralType type){
  int index = literal_findIndex(self->literal, value, type);
  if (index >= 0) return index;
  Literal *newLit = literal_new(value, type);
  self->plen++;
  Literal *lit = self->literal;
  if (lit == NULL) {
    self->literal = newLit;
    return 0;
  }
  for (index = 1; ; index++) {
    if (lit->next == NULL) break;
    lit = lit->next;
  }
  lit->next = newLit;
  return index;
}

static Symbol *symbol_new(const char *value)
{
  Symbol *symbol = picorbc_alloc(sizeof(Symbol));
  symbol->next = NULL;
  symbol->value = value;
  return symbol;
}

static Lvar *lvar_new(const char *name, int regnum)
{
  Lvar *lvar = picorbc_alloc(sizeof(Lvar));
  lvar->regnum = regnum;
  lvar->next = NULL;
  lvar->name = name;
  lvar->len = strlen(name);
  lvar->to_be_free = false;
  return lvar;
}

/*
 * returns -1 if symbol was not found
 */
int symbol_findIndex(Symbol *symbol, const char *value)
{
  int i = 0;
  while (symbol != NULL) {
    if (strcmp(symbol->value, value) == 0) {
      return i;
    }
    symbol = symbol->next;
    i++;
  }
  return -1;
}

int Scope_newSym(Scope *self, const char *value){
  int index = symbol_findIndex(self->symbol, value);
  if (index >= 0) return index;
  Symbol *newSym = symbol_new(value);
  self->slen++;
  Symbol *sym = self->symbol;
  if (sym == NULL) {
    self->symbol = newSym;
    return 0;
  }
  for (index = 1; ; index++) {
    if (sym->next == NULL) break;
    sym = sym->next;
  }
  sym->next = newSym;
  return index;
}

int Scope_assignSymIndex(Scope *self, const char *method_name)
{
  size_t length = strlen(method_name);
  char *assign_method_name = picorbc_alloc(length + 2);
  memcpy(assign_method_name, method_name, length);
  assign_method_name[length] = '=';
  assign_method_name[length + 1] = '\0';
  int symIndex = symbol_findIndex(self->symbol, assign_method_name);
  if (symIndex < 0) {
    symIndex = Scope_newSym(self, (const char *)assign_method_name);
    AssignSymbol *assign_symbol = picorbc_alloc(sizeof(AssignSymbol));
    assign_symbol->prev = NULL;
    assign_symbol->value = assign_method_name;
    assign_symbol->prev = self->last_assign_symbol;
    self->last_assign_symbol = assign_symbol;
  } else {
    picorbc_free(assign_method_name);
  }
  return symIndex;
}

/*
 * reg_num = 0 if lvar was not found
 */
LvarScopeReg Scope_lvar_findRegnum(Scope *self, const char *name)
{
  LvarScopeReg scopeReg = {0, 0};
  Scope *scope = self;
  Lvar *lvar;
  while (1) {
    lvar = scope->lvar;
    while (lvar != NULL) {
      if (strcmp(lvar->name, name) == 0) {
        scopeReg.reg_num = lvar->regnum;
        return scopeReg;
      }
      lvar = lvar->next;
    }
    if (scope->upper == NULL || scope->lvar_top == true) break;
    scopeReg.scope_num++;
    scope = scope->upper;
  };
  return (LvarScopeReg){0, 0};
}

void
Scope_newLvar(Scope *self, const char *name, int newRegnum){
  Lvar *newLvar = lvar_new(name, newRegnum);
  self->nlocals++;
  if (self->lvar == NULL) {
    self->lvar = newLvar;
  } else {
    Lvar *lvar = self->lvar;
    while (lvar->next) {
      lvar = lvar->next;
    }
    lvar->next = newLvar;
  }
}

void Scope_setSp(Scope *self, uint16_t sp){
  self->sp = sp;
  if (self->rlen < self->sp) self->rlen = self->sp;
}

void Scope_push(Scope *self){
  self->sp++;
  if (self->rlen < self->sp) self->rlen = self->sp;
}

int scope_codeSize(CodePool *code_pool)
{
  int size = 0;
  while (code_pool != NULL) {
    size += code_pool->index;
    code_pool = code_pool->next;
  }
  return size;
}

#define PICORUBYNULL "PICORUBYNULL\xF5"
/*
 * Replace back to null letter
 *  -> see REPLACE_NULL_PICORUBY, too
 */
size_t replace_picoruby_null(char *value)
{
  int i = 0;
  int j = 0;
  char j_value[strlen(value) + 1];
  while (value[i]) {
    if (value[i] != '\xF5') {
      j_value[j] = value[i];
    } else if (strncmp(value+i+1, PICORUBYNULL, sizeof(PICORUBYNULL)-1) == 0) {
      j_value[j] = '\0';
      i += sizeof(PICORUBYNULL) - 1;
    }
    i++;
    j++;
  }
  j_value[j] = '\0';
  if (j < i) memcpy(value, j_value, j);
  return j;
}

void Scope_finish(Scope *scope)
{
  scope->ilen = (uint32_t)scope->vm_code_size;
  ExcHandler *tmp;
  ExcHandler *exc_handler = scope->exc_handler;
  for (int i = 0; i < scope->clen; i++) {
    Scope_pushNCode_self(scope, exc_handler->table, 13);
    tmp = exc_handler;
    exc_handler = exc_handler->next;
    picorbc_free(tmp);
  }
  int len;
  uint8_t *data = scope->first_code_pool->data;
  // literal
  Literal *lit;
  Scope_pushCode((scope->plen >> 8) & 0xff);
  Scope_pushCode(scope->plen & 0xff);
  lit = scope->literal;
  while (lit != NULL) {
    Scope_pushCode(lit->type);
    if (lit->type == FLOAT_LITERAL) {
      double d = atof(lit->value);
      Scope_pushNCode((uint8_t *)&d, 8);
    } else {
      len = replace_picoruby_null((char *)lit->value);
      lit->type = len<<2; /* used in cdump_pool() */
      Scope_pushCode((len >> 8) & 0xff);
      Scope_pushCode(len & 0xff);
      Scope_pushNCode((uint8_t *)lit->value, len);
      Scope_pushCode(0);
    }
    lit = lit->next;
  }
  // symbol
  Symbol *sym;
  Scope_pushCode((scope->slen >> 8) & 0xff);
  Scope_pushCode(scope->slen & 0xff);
  sym = scope->symbol;
  while (sym != NULL) {
    /* len is used in dump.c */
    sym->len = replace_picoruby_null((char *)sym->value);
    Scope_pushCode((sym->len >>8) & 0xff);
    Scope_pushCode(sym->len & 0xff);
    Scope_pushNCode((uint8_t *)sym->value, sym->len);
    Scope_pushCode(0);
    sym = sym->next;
  }
  // irep header - record length.
  {
    scope->vm_code_size += IREP_HEADER_SIZE;
    data[0] = ((scope->vm_code_size >> 24) & 0xff);
    data[1] = ((scope->vm_code_size >> 16) & 0xff);
    data[2] = ((scope->vm_code_size >> 8) & 0xff);
    data[3] =  (scope->vm_code_size & 0xff);
  }
  // nlocals
  data[4] = (scope->nlocals >> 8) & 0xff;
  data[5] = scope->nlocals & 0xff;
  // nregs
  data[6] = ((scope->rlen) >> 8) & 0xff;
  data[7] = (scope->rlen) & 0xff;
  // rlen (Children)
  data[8] = (scope->nlowers >> 8) & 0xff;
  data[9] = scope->nlowers & 0xff;
  // clen (Catch handlers)
  data[10] = (scope->clen >> 8) & 0xff;
  data[11] =  scope->clen & 0xff;
  // ilen (IREP length)
  data[12] = (scope->ilen >> 24) & 0xff;
  data[13] = (scope->ilen >> 16) & 0xff;
  data[14] = (scope->ilen >> 8) & 0xff;
  data[15] =  scope->ilen & 0xff;
}

void freeCodePool(CodePool *pool)
{
  CodePool *next ;
  while (1) {
    next = pool->next;
    picorbc_free(pool);
    if (next == NULL) break;
    pool = next;
  }
}

void Scope_freeCodePool(Scope *self)
{
  if (self == NULL) return;
  Scope_freeCodePool(self->next);
  Scope_freeCodePool(self->first_lower);
  freeCodePool(self->first_code_pool);
}

JmpLabel *Scope_reserveJmpLabel(Scope *scope)
{
  Scope_pushNCode((uint8_t *)"\0\0", 2);
  JmpLabel *label = picorbc_alloc(sizeof(JmpLabel));
  label->address = (void *)&scope->current_code_pool->data[scope->current_code_pool->index - 2];
  label->pos = scope->vm_code_size;
  return label;
}

void Scope_backpatchJmpLabel(JmpLabel *label, uint32_t position)
{
  uint8_t *data = (uint8_t *)label->address;
  data[0] = ((position - label->pos) >> 8) & 0xff;
  data[1] = (position - label->pos) & 0xff;
  picorbc_free(label);
}

void Scope_pushRetryStack(Scope *self)
{
  RetryStack *retry_stack = picorbc_alloc(sizeof(RetryStack));
  retry_stack->pos = self->vm_code_size;
  if (self->retry_stack) {
    retry_stack->prev = self->retry_stack;
  } else {
    retry_stack->prev = NULL;
  }
  self->retry_stack = retry_stack;
}

void Scope_popRetryStack(Scope *self)
{
  RetryStack *memo = self->retry_stack;
  self->retry_stack = self->retry_stack->prev;
  picorbc_free(memo);
}

void Scope_pushBreakStack(Scope *self)
{
  BreakStack *break_stack = picorbc_alloc(sizeof(BreakStack));
  break_stack->point = NULL;
  break_stack->next_pos = self->vm_code_size;
  if (self->break_stack) {
    break_stack->prev = self->break_stack;
  } else {
    break_stack->prev = NULL;
  }
  self->break_stack = break_stack;
}

void Scope_popBreakStack(Scope *self)
{
  BreakStack *memo = self->break_stack;
  if (self->break_stack->point)
    Scope_backpatchJmpLabel(self->break_stack->point, self->vm_code_size);
  self->break_stack = self->break_stack->prev;
  picorbc_free(memo);
}

int Scope_updateVmCodeSizeThenReturnTotalSize(Scope *self)
{
  int totalSize = 0;
  if (self == NULL) return 0;
  /* check if it's an empty child scope like `class A; #HERE; end` */
  if (self->first_code_pool->next == NULL &&
      self->first_code_pool->index == IREP_HEADER_SIZE) return 0;
  totalSize += Scope_updateVmCodeSizeThenReturnTotalSize(self->first_lower);
  totalSize += Scope_updateVmCodeSizeThenReturnTotalSize(self->next);
  self->vm_code_size = scope_codeSize(self->first_code_pool);
  totalSize += self->vm_code_size;
  return totalSize;
}

