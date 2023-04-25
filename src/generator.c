#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <picorbc.h>
#include <common.h>
#include <debug.h>
#include <scope.h>
#include <node.h>
#include <generator.h>
#include <opcode.h>
#include <parse_header.h>
#include <dump.h>
#include <my_regex.h>

typedef struct {
  int nargs;
  int op_send;
  bool has_splat;
  bool has_doublesplat;
  bool has_block;
  bool has_block_arg;
  bool is_super;
} GenValuesResult;

bool hasCar(Node *n) {
  if (n->type != CONS)
    return false;
  if (n->cons.car) {
    return true;
  }
  return false;
}

bool hasCdr(Node *n) {
  if (n->type != CONS)
    return false;
  if (n->cons.cdr) {
    return true;
  }
  return false;
}

typedef enum misc {
  NUM_POS = 0,
  NUM_NEG = 1
} Misc;

void codegen(Scope *scope, Node *tree);

JmpLabel *reserve_jmpLabel(Scope *scope)
{
  Scope_pushNCode((uint8_t *)"\0\0", 2);
  JmpLabel *label = picorbc_alloc(sizeof(JmpLabel));
  label->address = (void *)&scope->current_code_pool->data[scope->current_code_pool->index - 2];
  label->pos = scope->vm_code_size;
  return label;
}

void backpatch_jmpLabel(JmpLabel *label, uint32_t position)
{
  uint8_t *data = (uint8_t *)label->address;
  data[0] = ((position - label->pos) >> 8) & 0xff;
  data[1] = (position - label->pos) & 0xff;
  picorbc_free(label);
}

void push_backpatch(Scope *scope, JmpLabel *label)
{
  Backpatch *bp = picorbc_alloc(sizeof(Backpatch));
  bp->next = NULL;
  bp->label = label;
  if (!scope->g->backpatch) {
    scope->g->backpatch = bp;
    return;
  }
  Backpatch *tmp = scope->g->backpatch;
  while (tmp->next) tmp = tmp->next;
  tmp->next = bp;
}

void shift_backpatch(Scope *scope)
{
  if (!scope->g->backpatch) return;
  Backpatch *bp = scope->g->backpatch;
  scope->g->backpatch = scope->g->backpatch->next;
  picorbc_free(bp);
}

void push_retryStack(Scope *scope)
{
  RetryStack *retry_stack = picorbc_alloc(sizeof(RetryStack));
  retry_stack->pos = scope->vm_code_size;
  if (scope->g->retry_stack) {
    retry_stack->prev = scope->g->retry_stack;
  } else {
    retry_stack->prev = NULL;
  }
  scope->g->retry_stack = retry_stack;
}

void pop_retryStack(Scope *scope)
{
  RetryStack *memo = scope->g->retry_stack;
  scope->g->retry_stack = scope->g->retry_stack->prev;
  picorbc_free(memo);
}

void push_breakStack(Scope *scope)
{
  BreakStack *break_stack = picorbc_alloc(sizeof(BreakStack));
  break_stack->point = NULL;
  break_stack->next_pos = scope->vm_code_size;
  if (scope->g->break_stack) {
    break_stack->prev = scope->g->break_stack;
  } else {
    break_stack->prev = NULL;
  }
  scope->g->break_stack = break_stack;
}

void pop_breakStack(Scope *scope)
{
  BreakStack *memo = scope->g->break_stack;
  if (scope->g->break_stack->point)
    backpatch_jmpLabel(scope->g->break_stack->point, scope->vm_code_size);
  scope->g->break_stack = scope->g->break_stack->prev;
  picorbc_free(memo);
}

void gen_self(Scope *scope)
{
  Scope_pushCode(OP_LOADSELF);
  Scope_pushCode(scope->sp);
}

/*
 *  n == 0 -> Condition nest
 *  n == 1 -> Block nest
 */
void push_nest_stack(Scope *scope, uint8_t n)
{
  if (scope->nest_stack & 0x80000000) {
    FATALP("Nest stack too deep!");
    return;
  }
  scope->nest_stack = (scope->nest_stack << 1) | (n & 1);
}

void pop_nest_stack(Scope *scope)
{
  scope->nest_stack = (scope->nest_stack >> 1);
}

/* Deprecated? */
const char *push_gen_literal(Scope *scope, const char *s)
{
  GenLiteral *lit = picorbc_alloc(sizeof(GenLiteral));
  if (scope->gen_literal) {
    lit->prev = scope->gen_literal;
  } else {
    lit->prev = NULL;
  }
  scope->gen_literal = lit;
  size_t len = strlen(s);
  char *value = picorbc_alloc(len + 1);
  memcpy(value, s, len);
  value[len] = '\0';
  lit->value = (const char *)value;
  return lit->value;
}

Scope *scope_nest(Scope *scope)
{
  GeneratorState *g = scope->g;
  uint32_t nest_stack = scope->nest_stack;
  scope = scope->first_lower;
  for (uint16_t i = 0; i < scope->upper->next_lower_number; i++) {
    scope = scope->next;
  }
  scope->upper->next_lower_number++;
  scope->nest_stack = nest_stack;
  push_nest_stack(scope, 1); /* 1 represents BLOCK NEST */
  scope->g = g;
  return scope;
}

Scope *scope_unnest(Scope *scope)
{
  pop_nest_stack(scope->upper);
  return scope->upper;
}

int count_args(Scope *scope, Node *node)
{
  int total_nargs = 0;
  while (node) {
    if (Node_atomType(node->cons.cdr->cons.car) == ATOM_args_new) break;
    if (Node_atomType(node->cons.cdr->cons.car) == ATOM_args_add) total_nargs++;
    if (hasCdr(node)) {
      node = node->cons.cdr->cons.car;
    } else {
      node = NULL;
    }
  }
  return total_nargs;
}

void
gen_values_sub(Node **node, GenValuesResult **result, Node **block_node)
{
  if (Node_atomType(*node) == ATOM_block_arg){
    (*result)->has_block_arg = true;
    *block_node = *node;
    *node = NULL;
  } else if (Node_atomType(*node) == ATOM_block){
    (*result)->has_block = true;
    *block_node = *node;
    *node = NULL;
  }
}

void gen_values(Scope *scope, Node *tree, GenValuesResult *result)
{
  result->op_send = OP_SEND;
  uint8_t prev_gen_splat_status = scope->g->gen_splat_status;
  scope->g->gen_splat_status = GEN_SPLAT_STATUS_NONE;
  int splat_pos = 0;
  Node *block_node = NULL;
  Node *node = tree;
  Node *node2;
  while (1) {
    if (Node_atomType(node->cons.cdr->cons.car) == ATOM_args_new) {
      if (Node_atomType(node->cons.cdr->cons.car->cons.cdr->cons.car) == ATOM_NONE) {
        /* kw_hash is the first arg */
        scope->sp--;
        result->nargs--;
      }
      node2 = node->cons.cdr->cons.car->cons.cdr;
      if (node2 && Node_atomType(node2->cons.car) == ATOM_splat) {
        splat_pos = 1;
      }
      if (node->cons.cdr && node->cons.cdr->cons.cdr) {
        gen_values_sub(&node->cons.cdr->cons.cdr->cons.cdr->cons.car, &result, &block_node);
        /* FIXME: in case like `p(*3){}` that only has one splat */
        if (node->cons.cdr->cons.cdr->cons.cdr->cons.cdr) {
          gen_values_sub(&node->cons.cdr->cons.cdr->cons.cdr->cons.cdr->cons.car, &result, &block_node);
        }
      }
      break;
    }
    if (hasCdr(node) && hasCar(node->cons.cdr)) result->nargs++;
    node2 = node->cons.cdr->cons.cdr;
    if (node2 && Node_atomType(node2->cons.car) == ATOM_splat) {
      splat_pos = result->nargs;
    }
    if (node2 && node2->cons.cdr) {
      gen_values_sub(&node2->cons.cdr->cons.cdr->cons.car, &result, &block_node);
      if (node2->cons.cdr->cons.cdr->cons.cdr) {
        gen_values_sub(&node2->cons.cdr->cons.cdr->cons.cdr->cons.car, &result, &block_node);
      }
    }
    if (!node->cons.cdr || !node->cons.cdr->cons.car) break;
    node = node->cons.cdr->cons.car;
  }
  { /* splat */
    if (splat_pos > 0) {
      scope->g->gen_splat_status = GEN_SPLAT_STATUS_BEFORE_SPLAT;
    }
    if (splat_pos == 1) { // The first arg is a splat
      Scope_pushCode(OP_LOADNIL);
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      scope->g->gen_splat_status = GEN_SPLAT_STATUS_AFTER_SPLAT;
    }
  }
  { /* kw args */
    if (tree->cons.cdr->cons.car) {
      node2 = tree->cons.cdr->cons.car->cons.cdr->cons.cdr;
      while (node2) {
        if (Node_atomType(node2->cons.car) == ATOM_kw_hash) {
          node2 = node2->cons.car->cons.cdr;
          while (node2) {
            if (Node_atomType(node2->cons.car) == ATOM_assoc_new) {
              result->nargs += 1<<4;
            } else if (Node_atomType(node2->cons.car) == ATOM_args_tail_kw_rest_args) {
              result->has_doublesplat = true;
            }
            node2 = node2->cons.cdr;
          }
          break;
        }
        node2 = node2->cons.cdr;
      }
    }
  }
  codegen(scope, tree->cons.cdr->cons.car);
  scope->g->gen_splat_status = prev_gen_splat_status;
  if (splat_pos > 0) result->has_splat = true;
  if (block_node) {
    codegen(scope, block_node);
    result->op_send = OP_SENDB;
  }
}

void gen_str(Scope *scope, Node *node)
{
  Scope_pushCode(OP_STRING);
  Scope_pushCode(scope->sp);
  int litIndex = Scope_newLit(scope, Node_literalName(node), STRING_LITERAL);
  Scope_pushCode(litIndex);
}

void gen_sym(Scope *scope, Node *node)
{
  Scope_pushCode(OP_LOADSYM);
  Scope_pushCode(scope->sp);
  int litIndex = Scope_newSym(scope, Node_literalName(node));
  Scope_pushCode(litIndex);
}

void gen_literal_numeric(Scope *scope, const char *num, LiteralType type, Misc is_neg)
{
  Scope_pushCode(OP_LOADL);
  Scope_pushCode(scope->sp);
  int litIndex;
  if (is_neg) {
    size_t len = strlen(num) + 1;
    char lit[len];
    lit[0] = '-';
    memcpy(&lit[1], num, len-1);
    lit[len] = '\0';
    const char *str = push_gen_literal(scope, lit);
    litIndex = Scope_newLit(scope, str, type);
  } else {
    litIndex = Scope_newLit(scope, num, type);
  }
  Scope_pushCode(litIndex);
}

void cleanup_numeric_literal(char *lit)
{
  char result[strlen(lit) + 1];
  int j = 0;
  for (int i = 0; i <= strlen(lit); i++) {
    if (lit[i] == '_') continue;
    result[j] = lit[i];
    j++;
  }
  result[j] = '\0';
  /* Rewrite StingPool */
  memcpy(lit, result, strlen(result));
  lit[strlen(result)] = '\0';
}

void gen_float(Scope *scope, Node *node, Misc is_neg)
{
  char *value = (char *)Node_valueName(node->cons.car);
  cleanup_numeric_literal(value);
  gen_literal_numeric(scope, (const char *)value, FLOAT_LITERAL, is_neg);
}

void gen_int(Scope *scope, Node *node, Misc is_neg)
{
  char *value = (char *)Node_valueName(node->cons.car);
  cleanup_numeric_literal(value);
  unsigned long val;
  switch (value[1]) {
    case ('b'):
    case ('B'):
      val = strtol(value+2, NULL, 2);
      break;
    case ('o'):
    case ('O'):
      val = strtol(value+2, NULL, 8);
      break;
    case ('x'):
    case ('X'):
      val = strtol(value+2, NULL, 16);
      break;
    default:
      if (Regex_match2(value, "^[0][0-7]+$")) {
        val = strtol(value, NULL, 8);
      } else {
        val = strtol(value, NULL, 10);
      }
  }
  if (!is_neg && 0 <= val && val <= 7) {
    Scope_pushCode(OP_LOADI_0 + val);
    Scope_pushCode(scope->sp);
  } else if (is_neg && val == 1) {
    Scope_pushCode(OP_LOADI__1);
    Scope_pushCode(scope->sp);
  } else if (val <= 0xff) {
    if (is_neg) {
      Scope_pushCode(OP_LOADINEG);
    } else {
      Scope_pushCode(OP_LOADI);
    }
    Scope_pushCode(scope->sp);
    Scope_pushCode(val);
  } else if (val <= 0x7fff && !is_neg) {
    Scope_pushCode(OP_LOADI16);
    Scope_pushCode(scope->sp);
    Scope_pushCode(val >> 8);
    Scope_pushCode(val & 0xff);
  } else if (val <= 0x8000 && is_neg) {
    Scope_pushCode(OP_LOADI16);
    Scope_pushCode(scope->sp);
    Scope_pushCode((signed long)val*(-1) >> 8);
    Scope_pushCode((signed long)val*(-1) & 0xff);
  } else if (val <= 0x7fffffff && !is_neg) {
    Scope_pushCode(OP_LOADI32);
    Scope_pushCode(scope->sp);
    Scope_pushCode(val >> 24);
    Scope_pushCode(val >> 16 & 0xff);
    Scope_pushCode(val >> 8 & 0xff);
    Scope_pushCode(val & 0xff);
  } else if (val <= 0x80000000 && is_neg) {
    Scope_pushCode(OP_LOADI32);
    Scope_pushCode(scope->sp);
    Scope_pushCode((signed long)val*(-1) >> 24 & 0xff);
    Scope_pushCode((signed long)val*(-1) >> 16 & 0xff);
    Scope_pushCode((signed long)val*(-1) >> 8 & 0xff);
    Scope_pushCode((signed long)val*(-1) & 0xff);
  } else {
    /* Deprecated? INT32 */
    uint8_t digit = 2;
    unsigned long n = val;
    while (n /= 10) ++digit; /* count number of digit */
    char lit[digit];
    snprintf(lit, digit, "%lu", val);
    gen_literal_numeric(scope, push_gen_literal(scope, lit), INT32_LITERAL, is_neg);
  }
}

void gen_call(Scope *scope, Node *node, bool is_fcall, bool is_scall)
{
  GenValuesResult result = {0};
  result.op_send = OP_SEND;
  int reg = scope->sp;
  // receiver
  if (!is_fcall) {
    codegen(scope, node->cons.car);
    node = node->cons.cdr;
  }
  JmpLabel *jmpLabel;
  if (is_scall) {
    Scope_pushCode(OP_MOVE);
    Scope_push(scope);
    Scope_pushCode(scope->sp);
    Scope_pushCode(scope->sp - 1);
    Scope_pushCode(OP_JMPNIL);
    Scope_pushCode(scope->sp--);
    jmpLabel = reserve_jmpLabel(scope);
  }
  // args
  if (node->cons.cdr->cons.car) {
    Scope_push(scope);
    gen_values(scope, node, &result);
  }
  const char *method_name = Node_literalName(node->cons.car->cons.cdr);
  Scope_setSp(scope, reg);
  if (method_name[1] == '\0' && strchr("><+-*/", method_name[0]) != NULL) {
    switch (method_name[0]) {
      case '>': Scope_pushCode(OP_GT); break;
      case '<': Scope_pushCode(OP_LT); break;
      case '+': Scope_pushCode(OP_ADD); break;
      case '-': Scope_pushCode(OP_SUB); break;
      case '*': Scope_pushCode(OP_MUL); break;
      case '/': Scope_pushCode(OP_DIV); break;
      default: FATALP("This should not happen"); break;
    }
    Scope_pushCode(scope->sp);
  } else if (strcmp(method_name, "==") == 0) {
    Scope_pushCode(OP_EQ);
    Scope_pushCode(scope->sp);
  } else if (strcmp(method_name, ">=") == 0) {
    Scope_pushCode(OP_GE);
    Scope_pushCode(scope->sp);
  } else if (strcmp(method_name, "<=") == 0) {
    Scope_pushCode(OP_LE);
    Scope_pushCode(scope->sp);
  } else if (strcmp(method_name, "[]") == 0 && result.nargs == 1) {
    Scope_pushCode(OP_GETIDX);
    Scope_pushCode(scope->sp);
  } else {
    if (is_fcall) {
      /*
       * OP_SEND  -> OP_SSEND
       * OP_SENDB -> OP_SSENDB
       */
      Scope_pushCode(result.op_send - 2);
    } else {
      Scope_pushCode(result.op_send);
    }
    Scope_pushCode(scope->sp);
    int symIndex = Scope_newSym(scope, method_name);
    Scope_pushCode(symIndex);
    if (result.op_send == OP_SEND || result.op_send == OP_SENDB) {
      if (result.has_splat || result.has_doublesplat) {
        int b = 0;
        if (result.has_splat) b |= 0x0f;
        if (result.has_doublesplat) b |= 0xf0;
        Scope_pushCode(b);
      } else {
        Scope_pushCode(result.nargs);
      }
    }
  }
  if (is_scall) backpatch_jmpLabel(jmpLabel, scope->vm_code_size);
}

void gen_masgn_2(Scope *scope, int total_nargs, Node *mlhs, bool has_splat);

void gen_array(Scope *scope, Node *node, Node *mlhs)
{
  uint8_t prev_gen_array_status = scope->g->gen_array_status;
  uint8_t prev_gen_array_count = scope->g->gen_array_count;
  uint16_t prev_nargs_before_splat = scope->g->nargs_before_splat;
  uint16_t prev_nargs_after_splat = scope->g->nargs_after_splat;
  scope->g->gen_array_status = GEN_ARRAY_STATUS_GENERATING;
  scope->g->gen_array_count = 0;
  scope->g->nargs_before_splat = 0;
  scope->g->nargs_after_splat = 0;
  int sp = scope->sp;
  GenValuesResult result = {0};
  if (node->cons.cdr->cons.car) gen_values(scope, node, &result);
  if (mlhs) {
    int total_nargs = 0;
    if (result.has_splat) {
      total_nargs = count_args(scope, node);
      Scope_setSp(scope, sp + total_nargs);
    } else {
      total_nargs = result.nargs;
    }
    gen_masgn_2(scope, total_nargs, mlhs, result.has_splat);
    Scope_setSp(scope, sp + result.nargs);
  }
  if (result.has_splat) {
    Scope_setSp(scope, sp);
  } else if (result.nargs == 0) {
    Scope_pushCode(OP_ARRAY);
    Scope_pushCode(scope->sp);
    Scope_pushCode(0);
  } else {
    result.nargs %= PICORUBY_ARRAY_SPLIT_COUNT;
    if (result.nargs) {
      scope->sp -= result.nargs;
      Scope_pushCode(OP_ARRAY);
      Scope_pushCode(scope->sp);
      Scope_pushCode(result.nargs);
      if (scope->g->gen_array_status == GEN_ARRAY_STATUS_GENERATING_SPLIT) {
        Scope_pushCode(OP_ARYCAT);
        Scope_pushCode(--scope->sp);
      }
    } else {
      scope->sp--;
    }
  }
  scope->g->gen_array_status = prev_gen_array_status;
  scope->g->gen_array_count = prev_gen_array_count;
  scope->g->nargs_before_splat = prev_nargs_before_splat;
  scope->g->nargs_after_splat = prev_nargs_after_splat;
}

void gen_hash(Scope *scope, Node *node, bool skip_op_hash)
{
  int reg = scope->sp;
  int nassocs = 0;
  Node *assoc = node;
  bool split = false;
  bool kw_rest = false;
  while (assoc) {
    if (assoc->cons.car->cons.cdr->cons.car->cons.cdr) {
      codegen(scope, assoc->cons.car->cons.cdr->cons.car->cons.cdr->cons.car);
    }
    Scope_push(scope);
    if (Node_atomType(assoc->cons.car) == ATOM_args_tail_kw_rest_args) {
      if (0 < nassocs && kw_rest) {
        Scope_pushCode(OP_HASHADD);
        Scope_pushCode(reg);
        Scope_pushCode(nassocs);
        nassocs = 0;
      }
      if (!kw_rest) {
        Scope_pushCode(OP_HASH);
        Scope_pushCode(reg);
        Scope_pushCode(0);
        kw_rest = true;
      }
      int reg_2 = scope->sp;
      scope->sp = reg + 1;
      if (assoc->cons.car->cons.cdr)
      codegen(scope, assoc->cons.car->cons.cdr);
      Scope_pushCode(OP_HASHCAT);
      Scope_pushCode(reg);
      scope->sp = reg_2;
    } else {
      codegen(scope, assoc->cons.car->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car);
      Scope_push(scope);
      nassocs++;
      if (nassocs % PICORUBY_HASH_SPLIT_COUNT == 0) {
        if (!split) {
          Scope_pushCode(OP_HASH);
        } else {
          Scope_pushCode(OP_HASHADD);
        }
        Scope_pushCode(reg);
        Scope_pushCode(PICORUBY_HASH_SPLIT_COUNT);
        Scope_setSp(scope, reg + 1);
        split = true;
      }
    }
    assoc = assoc->cons.cdr;
  }
  if (0 < nassocs && kw_rest) {
    Scope_pushCode(OP_HASHADD);
    Scope_pushCode(reg);
    Scope_pushCode(nassocs);
    nassocs = 0;
  }
  if (!skip_op_hash) {
    if (nassocs == 0) {
      Scope_pushCode(OP_HASH);
      Scope_pushCode(reg);
      Scope_pushCode(0);
    } else {
      nassocs %= PICORUBY_HASH_SPLIT_COUNT;
      if (nassocs) {
        if (!split) {
          Scope_pushCode(OP_HASH);
        } else {
          Scope_pushCode(OP_HASHADD);
        }
        Scope_pushCode(reg);
        Scope_pushCode(nassocs);
      }
    }
  }
  if (kw_rest) reg--;
  Scope_setSp(scope, reg);
}

void gen_var(Scope *scope, Node *node)
{
  int num;
  LvarScopeReg lvar;
  switch(Node_atomType(node)) {
    case (ATOM_lvar):
      lvar = Scope_lvar_findRegnum(scope, Node_literalName(node->cons.cdr));
      if (lvar.scope_num > 0) {
        Scope_pushCode(OP_GETUPVAR);
        Scope_pushCode(scope->sp);
        Scope_pushCode(lvar.reg_num);
        Scope_pushCode(lvar.scope_num - 1);
      } else {
        Scope_pushCode(OP_MOVE);
        Scope_pushCode(scope->sp);
        Scope_pushCode(lvar.reg_num);
      }
      break;
    case (ATOM_at_ivar):
    case (ATOM_at_gvar):
    case (ATOM_at_const):
      num = Scope_newSym(scope, Node_literalName(node->cons.cdr));
      switch (Node_atomType(node)) {
        case (ATOM_at_ivar):
          Scope_pushCode(OP_GETIV);
          break;
        case (ATOM_at_gvar):
          Scope_pushCode(OP_GETGV);
          break;
        case (ATOM_at_const):
          Scope_pushCode(OP_GETCONST);
          break;
        default:
          FATALP("error");
      }
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      Scope_pushCode(num);
      break;
    default:
      break;
  }
}

void gen_splat(Scope *scope, Node *node)
{
  if (scope->g->gen_splat_status == GEN_SPLAT_STATUS_BEFORE_SPLAT) {
    Scope_pushCode(OP_ARRAY);
    scope->sp -= scope->g->nargs_before_splat;
    Scope_pushCode(scope->sp);
    Scope_pushCode(scope->g->nargs_before_splat);
    scope->g->gen_splat_status = GEN_SPLAT_STATUS_AFTER_SPLAT;
    Scope_push(scope);
  }
  codegen(scope, node->cons.car);
  Scope_pushCode(OP_ARYCAT);
  Scope_pushCode(--scope->sp);
}

void gen_assign(Scope *scope, Node *node, int mrhs_reg)
{
  int num, reg;
  LvarScopeReg lvar = {0, 0};
  switch(Node_atomType(node->cons.car)) {
    case (ATOM_lvar):
      lvar = Scope_lvar_findRegnum(scope, Node_literalName(node->cons.car->cons.cdr));
      if (lvar.scope_num == 0) {
        codegen(scope, node->cons.cdr);
        Scope_pushCode(OP_MOVE);
        Scope_pushCode(lvar.reg_num);
        Scope_pushCode(mrhs_reg ? mrhs_reg : scope->sp);
      } else {
        codegen(scope, node->cons.cdr);
        Scope_pushCode(OP_SETUPVAR);
        Scope_pushCode(scope->sp);
        Scope_pushCode(lvar.reg_num);
        Scope_pushCode(lvar.scope_num - 1); // TODO: fix later
      }
      break;
    case (ATOM_at_ivar):
    case (ATOM_at_gvar):
    case (ATOM_at_const):
      num = Scope_newSym(scope, Node_literalName(node->cons.car->cons.cdr));
      codegen(scope, node->cons.cdr);
      switch(Node_atomType(node->cons.car)) {
        case (ATOM_at_ivar):
          Scope_pushCode(OP_SETIV);
          break;
        case (ATOM_at_gvar):
          Scope_pushCode(OP_SETGV);
          break;
        case (ATOM_at_const):
          Scope_pushCode(OP_SETCONST);
          break;
        default:
          FATALP("error");
      }
      Scope_pushCode(mrhs_reg ? mrhs_reg : scope->sp);
      Scope_pushCode(num);
      break;
    case ATOM_call:
      if (!mrhs_reg) {
        codegen(scope, node->cons.cdr->cons.car); /* right hand */
        reg = scope->sp;
        Scope_push(scope);
      } else {
        scope->sp++;
      }
      codegen(scope, node->cons.car->cons.cdr->cons.car); /* left hand */
      Node *call_node = node->cons.car->cons.cdr->cons.cdr;
      GenValuesResult result = {0};
      if (call_node->cons.cdr->cons.car) {
        Scope_push(scope);
        gen_values(scope, call_node, &result);
      }
      const char *method_name = Node_literalName(call_node->cons.car->cons.cdr);
      bool is_call_name_at_ary = false;
      if (!strcmp(method_name, "[]")) {
        is_call_name_at_ary = true;
      } else {
        Scope_push(scope);
      }
      Scope_pushCode(OP_MOVE);
      Scope_pushCode(scope->sp);
      scope->sp -= result.nargs + 2;
      Scope_pushCode(mrhs_reg ? mrhs_reg : scope->sp);
      Scope_push(scope);
      if (is_call_name_at_ary && result.nargs == 1) {
        Scope_pushCode(OP_SETIDX);
        Scope_pushCode(scope->sp);
      } else {
        Scope_pushCode(OP_SEND);
        Scope_pushCode(scope->sp);
        int symIndex = Scope_assignSymIndex(scope, method_name);
        Scope_pushCode(symIndex);
        Scope_pushCode(result.nargs + 1);
      }
      mrhs_reg ? (scope->sp -= 2) : Scope_setSp(scope, reg);
      break;
    case ATOM_masgn: {
      // Node *mlhs = node->cons.cdr->cons.car;
      // TODO
      ERRORP("No nested mass assigment supported!");
      break;
    }
    default:
      FATALP("error");
  }
}

void gen_masgn(Scope *scope, Node *node)
{
  Node *mlhs = node->cons.car;
  Node *mrhs = node->cons.cdr->cons.car->cons.cdr->cons.car;
  if (Node_atomType(mrhs) == ATOM_array) {
    gen_array(scope, mrhs, mlhs);
  } else {
    codegen(scope, mrhs);
    Scope_push(scope);
    gen_masgn_2(scope, 1, mlhs, true);
  }
}

void gen_masgn_node(Scope *scope, Node *node, int nargs, int *gen_count, int *mrhs_reg, bool has_splat)
{
  LvarScopeReg lvar = {0, 0};
  Node *next;
  while (node) {
    next = node->cons.cdr;
    node->cons.cdr = 0;
    if (has_splat) {
      Scope_pushCode(OP_AREF);
      bool is_lvar = false;
      if (Node_atomType(node->cons.car) == ATOM_lvar) {
        lvar = Scope_lvar_findRegnum(scope, Node_literalName(node->cons.car->cons.cdr));
        if (lvar.scope_num == 0) {
          Scope_pushCode(lvar.reg_num);
          is_lvar = true;
        }
      }
      if (!is_lvar) Scope_pushCode(*mrhs_reg + 1);
      Scope_pushCode(*mrhs_reg);
      Scope_pushCode((*gen_count)++);
      if (!is_lvar) gen_assign(scope, node, *mrhs_reg + 1);
    } else if (*gen_count < nargs){
      gen_assign(scope, node, (*mrhs_reg)++);
      (*gen_count)++;
    } else {
      switch (Node_atomType(node->cons.car)) {
        case (ATOM_lvar):
          lvar = Scope_lvar_findRegnum(scope, Node_literalName(node->cons.car->cons.cdr));
          if (lvar.scope_num == 0) {
            Scope_pushCode(OP_LOADNIL);
            Scope_pushCode(lvar.reg_num);
          } else {
            Scope_pushCode(OP_LOADNIL);
            Scope_pushCode(scope->sp);
            Scope_pushCode(OP_SETUPVAR);
            Scope_pushCode(scope->sp);
            Scope_pushCode(lvar.reg_num);
            Scope_pushCode(lvar.scope_num - 1);
          }
          break;
        case (ATOM_at_ivar):
        case (ATOM_at_gvar):
        case (ATOM_at_const):
          Scope_pushCode(OP_LOADNIL);
          Scope_pushCode(scope->sp);
          switch(Node_atomType(node->cons.car)) {
            case (ATOM_at_ivar):  Scope_pushCode(OP_SETIV); break;
            case (ATOM_at_gvar):  Scope_pushCode(OP_SETGV); break;
            case (ATOM_at_const): Scope_pushCode(OP_SETCONST); break;
            default: FATALP("error");
          }
          Scope_pushCode(scope->sp--);
          Scope_pushCode(Scope_newSym(scope, Node_literalName(node->cons.car->cons.cdr)));
          break;
        case (ATOM_call):
          codegen(scope, node->cons.car->cons.cdr->cons.car); /* left hand */
          Node *call_node = node->cons.car->cons.cdr->cons.cdr;
          GenValuesResult result = {0};
          if (call_node->cons.cdr->cons.car) {
            Scope_push(scope);
            gen_values(scope, call_node, &result);
          }
          const char *method_name = Node_literalName(call_node->cons.car->cons.cdr);
          bool is_call_name_at_ary = false;
          if (!strcmp(method_name, "[]")) {
            is_call_name_at_ary = true;
          } else {
            Scope_push(scope);
          }
          Scope_pushCode(OP_MOVE);
          Scope_pushCode(scope->sp);
          scope->sp -= result.nargs + 2;
          Scope_pushCode(scope->sp);
          Scope_push(scope);
          if (is_call_name_at_ary) {
            Scope_pushCode(OP_SETIDX);
            Scope_pushCode(scope->sp);
          } else {
            Scope_pushCode(OP_SEND);
            Scope_pushCode(scope->sp);
            int symIndex = Scope_assignSymIndex(scope, method_name);
            Scope_pushCode(symIndex);
            Scope_pushCode(result.nargs + 1);
          }
          break;
        default:
          FATALP("This should not happen!");
      }
    }
    Scope_push(scope);
    node = next;
  }
}

void gen_masgn_2(Scope *scope, int total_nargs, Node *mlhs, bool has_splat)
{
  int mrhs_reg = scope->sp - total_nargs;
  int gen_count = 0;
  Node *node;
  int npre = 0;
  int nrest = 0;
  int npost = 0;
  /* mlhs_pre */
  if (mlhs->cons.cdr->cons.car) {
    npre++;
    node = mlhs->cons.cdr->cons.car->cons.cdr;
    while (node->cons.cdr) {
      npre++;
      node = node->cons.cdr;
    }
    gen_masgn_node(scope, mlhs->cons.cdr->cons.car->cons.cdr, total_nargs, &gen_count, &mrhs_reg, has_splat);
    scope->sp -= gen_count;
  }
  /* mlhs_rest */
  if (mlhs->cons.cdr->cons.cdr) {
    { /* prepare mlhs_post */
      if (mlhs->cons.cdr->cons.cdr->cons.cdr) {
        npost++;
        node = mlhs->cons.cdr->cons.cdr->cons.cdr->cons.car->cons.cdr;
        while (node->cons.cdr) {
          npost++;
          node = node->cons.cdr;
        }
      }
    }
    LvarScopeReg lvar = {0, 0};
    node = mlhs->cons.cdr->cons.cdr->cons.car->cons.cdr;
    if (total_nargs - gen_count - npost > 0) nrest = total_nargs - gen_count - npost;
    if (has_splat) {
      Scope_pushCode(OP_MOVE);
      Scope_pushCode(mrhs_reg + 1);
      Scope_pushCode(mrhs_reg);
      Scope_pushCode(OP_APOST);
      Scope_pushCode(mrhs_reg + 1);
      Scope_pushCode(npre);
      Scope_pushCode(npost);
      Scope_setSp(scope, mrhs_reg + 1);
    } else {
      Scope_pushCode(OP_ARRAY2);
      Scope_pushCode(scope->sp);
      Scope_pushCode(mrhs_reg);
      Scope_pushCode(nrest);
    }
    switch (Node_atomType(node->cons.car)) {
      case (ATOM_lvar):
        lvar = Scope_lvar_findRegnum(scope, Node_literalName(node->cons.car->cons.cdr));
        if (lvar.scope_num == 0) {
          Scope_pushCode(OP_MOVE);
          Scope_pushCode(lvar.reg_num);
          Scope_pushCode(scope->sp);
        } else {
          Scope_pushCode(OP_SETUPVAR);
          Scope_pushCode(scope->sp);
          Scope_pushCode(lvar.reg_num);
          Scope_pushCode(lvar.scope_num - 1);
        }
        break;
      case (ATOM_at_ivar):
      case (ATOM_at_gvar):
      case (ATOM_at_const):
        switch(Node_atomType(node->cons.car)) {
          case (ATOM_at_ivar):  Scope_pushCode(OP_SETIV); break;
          case (ATOM_at_gvar):  Scope_pushCode(OP_SETGV); break;
          case (ATOM_at_const): Scope_pushCode(OP_SETCONST); break;
          default: FATALP("error");
        }
        Scope_pushCode(scope->sp--);
        Scope_pushCode(Scope_newSym(scope, Node_literalName(node->cons.car->cons.cdr)));
        break;
      case (ATOM_call):
        scope->sp++;
        codegen(scope, node->cons.car->cons.cdr->cons.car); /* left hand */
        Node *call_node = node->cons.car->cons.cdr->cons.cdr;
        GenValuesResult result = {0};
        if (call_node->cons.cdr->cons.car) {
          Scope_push(scope);
          gen_values(scope, call_node, &result);
        }
        const char *method_name = Node_literalName(call_node->cons.car->cons.cdr);
        bool is_call_name_at_ary = false;
        if (!strcmp(method_name, "[]")) {
          is_call_name_at_ary = true;
        } else {
          Scope_push(scope);
        }
        Scope_pushCode(OP_MOVE);
        Scope_pushCode(scope->sp);
        scope->sp -= result.nargs + 2;
        Scope_pushCode(scope->sp);
        Scope_push(scope);
        if (is_call_name_at_ary) {
          Scope_pushCode(OP_SETIDX);
          Scope_pushCode(scope->sp);
        } else {
          Scope_pushCode(OP_SEND);
          Scope_pushCode(scope->sp);
          int symIndex = Scope_assignSymIndex(scope, method_name);
          Scope_pushCode(symIndex);
          Scope_pushCode(result.nargs + 1);
        }
      default:
        FATALP("Should not happen!");
    }
  }
  /* mlhs_post */
  if (npost > 0) {
    node = mlhs->cons.cdr->cons.cdr->cons.cdr->cons.car->cons.cdr;
    if (has_splat) {
      mrhs_reg += 2;
      gen_count -= npost;
    } else {
      mrhs_reg += nrest;
    }
    gen_masgn_node(scope, node, total_nargs, &gen_count, &mrhs_reg, false);
  }
  scope->sp--;
}

void gen_op_assign(Scope *scope, Node *node)
{
  int num = 0;
  LvarScopeReg lvar = {0, 0};
  char *method_name = NULL;
  const char *call_name;
  { /* setup call_name here to avoid warning */
    if (Node_atomType(node->cons.car) == ATOM_call)
      call_name = Node_literalName(node->cons.car->cons.cdr->cons.cdr->cons.car->cons.cdr);
    else
      call_name = NULL;
  }
  bool is_call_name_at_ary = false;
  method_name = (char *)Node_literalName(node->cons.cdr->cons.car->cons.cdr);
  bool isANDOPorOROP = false; /* &&= or ||= */
  if (method_name[1] == '|' || method_name[1] == '&') {
    isANDOPorOROP = true;
  }
  JmpLabel *jmpLabel = NULL;
  switch(Node_atomType(node->cons.car)) {
    case (ATOM_lvar):
      lvar = Scope_lvar_findRegnum(scope, Node_literalName(node->cons.car->cons.cdr));
      if (lvar.scope_num > 0) {
        Scope_push(scope);
        Scope_pushCode(OP_GETUPVAR);
        Scope_pushCode(scope->sp - 1);
        Scope_pushCode(lvar.reg_num);
        Scope_pushCode(lvar.scope_num - 1);
      } else {
        num = lvar.reg_num;
        Scope_pushCode(OP_MOVE);
        Scope_pushCode(scope->sp);
        Scope_push(scope);
        Scope_pushCode(num);
      }
      break;
    case (ATOM_at_ivar):
    case (ATOM_at_gvar):
    case (ATOM_at_const):
      switch(Node_atomType(node->cons.car)) {
        case (ATOM_at_ivar):  Scope_pushCode(OP_GETIV); break;
        case (ATOM_at_gvar):  Scope_pushCode(OP_GETGV); break;
        case (ATOM_at_const): Scope_pushCode(OP_GETCONST); break;
        default: FATALP("error");
      }
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      num = Scope_newSym(scope, Node_literalName(node->cons.car->cons.cdr));
      Scope_pushCode(num);
      break;
    case (ATOM_call):
      if (!strcmp(call_name, "[]")) {
        is_call_name_at_ary = true;
      }
      Scope_push(scope);
      codegen(scope, node->cons.car->cons.cdr->cons.car);
      Scope_push(scope);
      node->cons.car->cons.cdr->cons.car = NULL;
      if (is_call_name_at_ary) {
        codegen(scope, node->cons.car->cons.cdr);
        Scope_pushCode(OP_MOVE);
        Scope_pushCode(scope->sp);
        Scope_pushCode(scope->sp - 2);
        Scope_push(scope);
        Scope_pushCode(OP_MOVE);
        Scope_pushCode(scope->sp);
        Scope_pushCode(scope->sp - 2);
        /* op assign doesn't use OP_SETIDX */
        Scope_pushCode(OP_SEND);
        Scope_pushCode(scope->sp - 1);
        Scope_pushCode(Scope_newSym(scope, "[]"));
        Scope_pushCode(1);
      } else {
        Scope_pushCode(OP_MOVE);
        Scope_pushCode(scope->sp);
        Scope_pushCode(scope->sp - 1);
        codegen(scope, node->cons.car);
        Scope_push(scope);
      }
      break;
    default:
      FATALP("error");
  }
  /* right hand */
  if (!isANDOPorOROP) codegen(scope, node->cons.cdr->cons.cdr);
  switch (method_name[0]) {
    case '+':
      Scope_pushCode(OP_ADD);
      Scope_pushCode(--scope->sp);
      break;
    case '-':
      Scope_pushCode(OP_SUB);
      Scope_pushCode(--scope->sp);
      break;
    case '/':
      Scope_pushCode(OP_DIV);
      Scope_pushCode(--scope->sp);
      break;
    case '%':
    case '^':
    case '&':
    case '|':
    case '<': /* <<= */
    case '>': /* >>= */
    case '*': /* *= and **= */
      if (!strcmp(method_name, "*=")) {
        Scope_pushCode(OP_MUL);
        Scope_pushCode(--scope->sp);
      } else if (isANDOPorOROP) {
        switch (method_name[1]) {
          case '|':
            Scope_pushCode(OP_JMPIF);
            break;
          case '&':
            Scope_pushCode(OP_JMPNOT);
            break;
        }
        Scope_pushCode(--scope->sp);
        jmpLabel = reserve_jmpLabel(scope);
        /* right condition of `___ &&= ___` */
        codegen(scope, node->cons.cdr);
      } else {
        Scope_pushCode(OP_SEND);
        Scope_pushCode(--scope->sp);
        /*
         * method_name[strlen(method_name) - 1] = '\0';
         * 👆🐛
         * Because the same method_name may be reused
         */
        for (int i=1; i<strlen(method_name); i++) {
          if (method_name[i] == '=') method_name[i] = '\0';
        }
        Scope_pushCode(Scope_newSym(scope, (const char *)method_name));
        Scope_pushCode(1);
      }
      break;
    default:
      FATALP("error");
  }
  switch(Node_atomType(node->cons.car)) {
    case (ATOM_lvar):
      if (lvar.scope_num > 0) {
        Scope_pushCode(OP_SETUPVAR);
        Scope_pushCode(scope->sp);
        Scope_pushCode(lvar.reg_num);
        Scope_pushCode(lvar.scope_num - 1);
      } else {
        Scope_pushCode(OP_MOVE);
      }
      break;
    case (ATOM_at_ivar):
      Scope_pushCode(OP_SETIV);
      break;
    case (ATOM_at_gvar):
      Scope_pushCode(OP_SETGV);
      break;
    case (ATOM_at_const):
      Scope_pushCode(OP_SETCONST);
      break;
    case (ATOM_call):
      /* right hand of assignment (mass-assign dosen't work) */
      Scope_pushCode(OP_MOVE);
      if (is_call_name_at_ary) {
        Scope_pushCode(scope->sp - 3);
      } else {
        Scope_pushCode(scope->sp - 2);
      }
      Scope_pushCode(scope->sp);
      GenValuesResult result = {0};
      if (!is_call_name_at_ary)
        gen_values(scope, node->cons.car->cons.cdr->cons.cdr, &result);
      /* exec assignment .[]= or .attr= */
      Scope_pushCode(OP_SEND);
      scope->sp--;
      if (is_call_name_at_ary) scope->sp--;
      Scope_pushCode(scope->sp--);
      Scope_pushCode(Scope_assignSymIndex(scope, call_name));
      /* count of args */
      if (is_call_name_at_ary) {
        Scope_pushCode(2); /* .[]= */
      } else {
        Scope_pushCode(1); /* .attr= */
      }
      break;
    default:
      FATALP("error");
  }
  switch(Node_atomType(node->cons.car)) {
    case (ATOM_lvar):
      if (lvar.scope_num > 0) break;
      Scope_pushCode(num);
      Scope_pushCode(scope->sp);
      break;
    case (ATOM_at_ivar):
    case (ATOM_at_gvar):
    case (ATOM_at_const):
      Scope_pushCode(scope->sp);
      Scope_pushCode(num);
      break;
    default:
      break;
  }
  /* goto label */
  if (isANDOPorOROP) backpatch_jmpLabel(jmpLabel, scope->vm_code_size);
}

void gen_dstr(Scope *scope, Node *node)
{
  int num = 0; /* number of ATOM_dstr_add */
  Node *dstr = node->cons.cdr->cons.car;
  while (Node_atomType(dstr) != ATOM_dstr_new) {
    dstr = dstr->cons.cdr->cons.car;
    num++;
  }
  /* start from the bottom (== the first str) of the tree */
  int sp = scope->sp; /* copy */
  for (int count = num; 0 < count; count--) {
    dstr = node->cons.cdr->cons.car;
    for (int i = 1; i < count; i++) {
      dstr = dstr->cons.cdr->cons.car;
    }
    codegen(scope, dstr->cons.cdr->cons.cdr->cons.car);
    Scope_push(scope);
    if (count != num) {
      /* TRICKY (I'm not sure if this is a correct way) */
      Scope_pushCode(OP_STRCAT);
      Scope_setSp(scope, sp);
      Scope_pushCode(scope->sp);
      Scope_push(scope);
    }
  }
  scope->sp--;
}

void gen_and_or(Scope *scope, Node *node, int opcode)
{
  /* left condition */
  codegen(scope, node->cons.car);
  Scope_pushCode(opcode); /* and->OP_JMPNOT, or->OP_JMPIF */
  Scope_pushCode(scope->sp);
  JmpLabel *label = reserve_jmpLabel(scope);
  /* right condition */
  codegen(scope, node->cons.cdr);
  /* goto label */
  backpatch_jmpLabel(label, scope->vm_code_size);
}

void gen_case_when(Scope *scope, Node *node, int cond_reg, JmpLabel *label_true[])
{
  if (Node_atomType(node->cons.car) == ATOM_args_add) {
    if (Node_atomType(node->cons.car->cons.cdr->cons.car) != ATOM_args_new) {
      gen_case_when(scope, node->cons.car->cons.cdr, cond_reg, label_true + 1);
      node = node->cons.car->cons.cdr->cons.cdr;
    } else {
      node = node->cons.car->cons.cdr->cons.car->cons.cdr;
    }
  } else {
    return;
  }
  Scope_setSp(scope, cond_reg);
  const char *method;
  if (Node_atomType(node->cons.car) == ATOM_splat) {
    method = "__case_eqq";
    codegen(scope, node->cons.car->cons.cdr);
  } else {
    method = "===";
    codegen(scope, node);
  }
  Scope_pushCode(OP_MOVE);
  Scope_pushCode(scope->sp + 1);
  Scope_pushCode(cond_reg - 1);
  Scope_pushCode(OP_SEND);
  Scope_pushCode(cond_reg);
  Scope_pushCode(Scope_newSym(scope, method));
  Scope_pushCode(1);
  /* when condition matched */
  Scope_pushCode(OP_JMPIF);
  Scope_pushCode(cond_reg);
  *label_true = reserve_jmpLabel(scope);
  Scope_setSp(scope, cond_reg);
}

void gen_case(Scope *scope, Node *node)
{
  int start_reg = scope->sp;
  /* count number of cases including else clause */
  Node *a_case = node->cons.cdr->cons.car;
  int i = 0;
  while (1) {
    if (a_case && a_case->cons.car) i++; else break;
    if (a_case->cons.cdr) {
      a_case = a_case->cons.cdr->cons.cdr;
    } else {
      break;
    }
  }
  int when_count = i - 1;
  JmpLabel *label_end_array[when_count];
  /* case expression */
  codegen(scope, node->cons.car);
  Scope_push(scope);
  int cond_reg = scope->sp; /* cond_reg === when_expr */
  /* each a_case */
  Node *case_body = node->cons.cdr->cons.car;
  i = 0;
  a_case = case_body;
  while (1) {
    /* when expression */
    /* count number of args */
    int args_count = count_args(scope, a_case);
    JmpLabel *label_true_array[args_count];
    gen_case_when(scope, a_case->cons.cdr, cond_reg, label_true_array);
    /* when condition didn't match */
    Scope_pushCode(OP_JMP);
    JmpLabel *label_false = reserve_jmpLabel(scope);
    /* content */
    for (int j = 0; j < args_count; j++)
      backpatch_jmpLabel(label_true_array[j], scope->vm_code_size);
    { /* inside when */
      Scope_setSp(scope, cond_reg);
      int16_t current_vm_code_size = scope->vm_code_size;
      /* stmts_add */
      codegen(scope, a_case->cons.cdr->cons.cdr->cons.car);
      /* if code was empty */
      if (current_vm_code_size == scope->vm_code_size) {
        Scope_pushCode(OP_LOADNIL);
        Scope_pushCode(scope->sp);
      }
    }
    Scope_pushCode(OP_JMP);
    label_end_array[i++] = reserve_jmpLabel(scope);
    backpatch_jmpLabel(label_false, scope->vm_code_size);
    /* next case */
    a_case = a_case->cons.cdr->cons.cdr;
    if (a_case && a_case->cons.cdr && Node_atomType(a_case->cons.cdr->cons.car) == ATOM_args_add) {
      continue;
    } else if (a_case && a_case->cons.cdr) {
      if (Node_atomType(a_case->cons.cdr->cons.car) == ATOM_stmts_add) {
        /* else */
        codegen(scope, a_case->cons.cdr->cons.car);
      } else if (Node_atomType(a_case->cons.cdr->cons.car) == ATOM_stmts_new) {
        /* empty else */
        Scope_pushCode(OP_LOADNIL);
        Scope_pushCode(start_reg + 1);
      }
      break;
    } else {
      /* no else clause */
      Scope_pushCode(OP_LOADNIL);
      Scope_pushCode(start_reg + 1);
      break;
    }
  }
  for (i = 0; i < when_count; i++)
    backpatch_jmpLabel(label_end_array[i], scope->vm_code_size);
  Scope_setSp(scope, start_reg);
  Scope_pushCode(OP_MOVE);
  Scope_pushCode(scope->sp++);
  Scope_pushCode(scope->sp--);
}

void gen_if(Scope *scope, Node *node)
{
  int start_reg = scope->sp;
  /* assert condition */
  codegen(scope, node->cons.car);
  Scope_pushCode(OP_JMPNOT);
  Scope_pushCode(scope->sp);
  JmpLabel *label_false = reserve_jmpLabel(scope);
  /* condition true */
  codegen(scope, node->cons.cdr->cons.car);
  Scope_pushCode(OP_JMP);
  JmpLabel *label_end = reserve_jmpLabel(scope);
  /* condition false */
  backpatch_jmpLabel(label_false, scope->vm_code_size);
  Scope_setSp(scope, start_reg);
  if (Node_atomType(node->cons.cdr->cons.cdr->cons.car) == ATOM_NONE) {
  // FIXME
  //if (node->cons.cdr->cons.cdr->cons.cdr == NULL) {
    /* right before KW_end */
    Scope_pushCode(OP_LOADNIL);
    Scope_pushCode(scope->sp);
  } else {
    /* if_tail */
    codegen(scope, node->cons.cdr->cons.cdr->cons.car);
    Scope_setSp(scope, start_reg);
  }
  /* right after KW_end */
  backpatch_jmpLabel(label_end, scope->vm_code_size);
}

void gen_alias(Scope *scope, Node *node)
{
  Scope_pushCode(OP_ALIAS);
  int litIndex = Scope_newSym(scope, Node_literalName(node->cons.car->cons.cdr));
  Scope_pushCode(litIndex);
  litIndex = Scope_newSym(scope, Node_literalName(node->cons.cdr->cons.car->cons.cdr));
  Scope_pushCode(litIndex);
}

void gen_dot2_3(Scope *scope, Node *node, int op)
{
  codegen(scope, node->cons.car);
  Scope_push(scope);
  codegen(scope, node->cons.cdr->cons.car);
  Scope_pushCode(op);
  Scope_pushCode(--scope->sp);
}

void gen_colon2(Scope *scope, Node *node)
{
  if (Node_atomType(node->cons.car) == ATOM_at_const) {
    Scope_pushCode(OP_GETCONST);
    Scope_pushCode(scope->sp);
    Scope_pushCode(Scope_newSym(scope, Node_literalName(node->cons.car->cons.cdr)));
  } else {
    /* Should be colon2 or none */
    codegen(scope, node->cons.car);
  }
  //codegen(scope, node->cons.cdr->cons.car);
  Scope_pushCode(OP_GETMCNST);
  Scope_pushCode(scope->sp);
  Scope_pushCode(Scope_newSym(scope, Node_literalName(node->cons.cdr->cons.car)));
}

void gen_colon3(Scope *scope, Node *node)
{
  Scope_pushCode(OP_OCLASS);
  Scope_pushCode(scope->sp);
  Scope_pushCode(OP_GETMCNST);
  Scope_pushCode(scope->sp);
  Scope_pushCode(Scope_newSym(scope, Node_literalName(node)));
}

void gen_while(Scope *scope, Node *node, int op_jmp)
{
  push_nest_stack(scope, 0); /* 0 represents CONDITION NEST */
  push_breakStack(scope);
  Scope_pushCode(OP_JMP);
  JmpLabel *label_cond = reserve_jmpLabel(scope);
  scope->g->break_stack->redo_pos = scope->vm_code_size;
  /* inside while */
  uint32_t top = scope->vm_code_size;
  codegen(scope, node->cons.cdr);
  /* just before condition */
  backpatch_jmpLabel(label_cond, scope->vm_code_size);
  /* condition */
  codegen(scope, node->cons.car);
  Scope_pushCode(op_jmp);
  Scope_pushCode(scope->sp);
  JmpLabel *label_top = reserve_jmpLabel(scope);
  backpatch_jmpLabel(label_top, top);
  {
    /* after while block / TODO: should be omitted if subsequent code exists */
    Scope_pushCode(OP_LOADNIL);
    Scope_pushCode(scope->sp);
  }
  pop_breakStack(scope);
  pop_nest_stack(scope);
}

void gen_break(Scope *scope, Node *node)
{
  Scope_push(scope);
  codegen(scope, node);
  scope->sp--;
  if (scope->nest_stack & 1) { /* BLOCK NEST */
    Scope_pushCode(OP_BREAK);
    Scope_pushCode(scope->sp);
  } else {                     /* CONDITION NEST */
    Scope_pushCode(OP_JMP);
    scope->g->break_stack->point = reserve_jmpLabel(scope);
  }
}

void gen_next(Scope *scope, Node *node)
{
  Scope_push(scope);
  codegen(scope, node);
  scope->sp--;
  if (scope->nest_stack & 1) { /* BLOCK NEST */
    Scope_pushCode(OP_RETURN);
    Scope_pushCode(scope->sp);
  } else {                     /* CONDITION NEST */
    Scope_pushCode(OP_JMP);
    JmpLabel *label = reserve_jmpLabel(scope);
    backpatch_jmpLabel(label, scope->g->break_stack->next_pos);
  }
}

void gen_redo(Scope *scope)
{
  Scope_push(scope);
  Scope_pushCode(OP_JMP);
  if (scope->nest_stack & 1) { /* BLOCK NEST */
    Scope_pushCode(0);
    Scope_pushCode(0);
  } else {                     /* CONDITION NEST */
    JmpLabel *label = reserve_jmpLabel(scope);
    backpatch_jmpLabel(label, scope->g->break_stack->redo_pos);
  }
}

void
setup_parameters_sub(Node *node, uint32_t *bbb, int shift_size)
{
  for (;;) {
    if (!hasCar(node->cons.cdr)) break;
    if (Node_atomType(node->cons.cdr->cons.car) == ATOM_args_new) break;
    *bbb += 1 << shift_size;
    node = node->cons.cdr->cons.car;
  }
}

/*
 * bbb: 000000000000000000000000
 *       ^^^^^     ^     ^^^^^ ^
 *      ^     ^^^^^ ^^^^^     ^
 *      x  m1    o r  m2   k  db
 * range| size |area|descripiton
 * -----|------|----|-----------
 * 23   |      |  x |not in use
 * 22-18|5 bits| m1 |mandatory 1
 * 17-13|5 bits|  o |option
 *    12|1 bit |  r |rest
 * 11- 7|5 bits| m2 |mandatory 2
 *  6- 2|5 bits|  k |keyword
 *     1|1 bit |  d |dictionary
 *     0|1 bit |  b |block
 */
uint32_t setup_parameters(Scope *scope, Node *node)
{
  uint32_t bbb = 0;
  if (Node_atomType(node) != ATOM_block_parameters) return bbb;
  Node *node2;
  { /* mandatory args */
    node2 = node->cons.cdr->cons.car; // ATOM_margs
    setup_parameters_sub(node2, &bbb, 18);
  }
  { /* option args which have an initial value */
    node2 = node->cons.cdr->cons.cdr->cons.car; // ATOM_optargs
    setup_parameters_sub(node2, &bbb, 13);
  }
  { /* rest arg */
    Node *restarg = node->cons.cdr->cons.cdr->cons.cdr->cons.car;
    if (restarg->cons.cdr->cons.car) bbb |= 0b1000000000000;
  }
  { /* m2args */
    node2 = node->cons.cdr->cons.cdr->cons.cdr->cons.cdr->cons.car; // ATOM_optargs
    setup_parameters_sub(node2, &bbb, 7);
  }
  /* tailargs */
  Node *args_tail = node->cons.cdr->cons.cdr->cons.cdr->cons.cdr->cons.cdr->cons.car;
  if (Node_atomType(args_tail) != ATOM_args_tail) return bbb;
  { /* kw_args */
    Node *kw_args = args_tail->cons.cdr->cons.car->cons.cdr;
    int kw_args_num = 0;
    for (;;) {
      if (!kw_args || Node_atomType(kw_args->cons.car) != ATOM_args_tail_kw_arg) break;
      kw_args_num++;
      kw_args = kw_args->cons.cdr;
    }
    bbb += kw_args_num<<2;
  }
  { /* kw_rest_args */
    Node *kw_rest_args = args_tail->cons.cdr->cons.cdr->cons.car;
    if (kw_rest_args->cons.cdr->cons.car && kw_rest_args->cons.cdr->cons.car->value.name) {
      bbb += 2;
    }
  }
  { /* block_arg */
    Node *block_arg = args_tail->cons.cdr->cons.cdr->cons.cdr->cons.car;
    if (block_arg->cons.cdr->cons.car->value.name) bbb += 1;
  }
  return bbb;
}

void gen_irep(Scope *scope, Node *node)
{
  scope = scope_nest(scope);
  int sp = scope->sp;
  uint32_t bbb = setup_parameters(scope, node->cons.car);
  { /* adjustments */
    Scope_setSp(scope, sp); // cancel gen_var()'s effect
  }
  scope->irep_parameters = bbb;
  scope->rlen++;
  Scope_pushCode(OP_ENTER);
  Scope_pushCode((uint8_t)(bbb >> 16 & 0xFF));
  Scope_pushCode((uint8_t)(bbb >> 8 & 0xFF));
  Scope_pushCode((uint8_t)(bbb & 0xFF));
  { /* option args */
    uint8_t nopt = (bbb>>13)&31;
    if (nopt) {
      for (int i=0; i < nopt; i++) {
        Scope_pushCode(OP_JMP);
        push_backpatch(scope, reserve_jmpLabel(scope));
      }
      Scope_pushCode(OP_JMP);
      JmpLabel *label = reserve_jmpLabel(scope);
      codegen(scope, node->cons.car->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car);
      scope->sp -= nopt;
      backpatch_jmpLabel(label, scope->vm_code_size);
    }
  }
  { /* keyword args */
    uint8_t nkw = (bbb>>2)&31;
    if (nkw) {
      LvarScopeReg scopeReg;
      /* ATOM_args_tail_kw_args */
      Node *kw_args = node->cons.car->cons.cdr->cons.cdr->cons.cdr->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car->cons.cdr;;
      int _sp = scope->sp;
      for (int i=0; i < nkw; i++) {
        const char *lvarName = Node_literalName(kw_args->cons.car->cons.cdr);
        scopeReg = Scope_lvar_findRegnum(scope, lvarName);
        scope->sp = scopeReg.reg_num;
        int litIndex = Scope_newSym(scope, lvarName);
        JmpLabel *label_2 = NULL;
        if (kw_args->cons.car->cons.cdr->cons.cdr->cons.car) {
          Scope_pushCode(OP_KEY_P);
          Scope_pushCode(scopeReg.reg_num);
          Scope_pushCode(litIndex);
          Scope_pushCode(OP_JMPIF);
          Scope_pushCode(scopeReg.reg_num);
          JmpLabel *label_1 = reserve_jmpLabel(scope);
          codegen(scope, kw_args->cons.car->cons.cdr);
          Scope_pushCode(OP_JMP);
          label_2 = reserve_jmpLabel(scope);
          backpatch_jmpLabel(label_1, scope->vm_code_size);
        }
        Scope_pushCode(OP_KARG);
        Scope_pushCode(scopeReg.reg_num);
        Scope_pushCode(litIndex);
        kw_args = kw_args->cons.cdr;
        if (label_2) backpatch_jmpLabel(label_2, scope->vm_code_size);
        Scope_push(scope);
        scope->sp = _sp;
      }
      if ((bbb & 0b10) == 0) {
        Scope_pushCode(OP_KEYEND);
      }
    }
  }
  { /* inside def */
    int32_t current_vm_code_size = scope->vm_code_size;
    codegen(scope, node->cons.cdr->cons.car);
    /* if code was empty */
    if (current_vm_code_size == scope->vm_code_size) {
      Scope_pushCode(OP_LOADNIL);
      Scope_pushCode(scope->sp);
    }
    int op = scope->current_code_pool->data[scope->current_code_pool->index - 2];
    if (op != OP_RETURN) {
      Scope_pushCode(OP_RETURN);
      Scope_pushCode(scope->sp);
    }
  }
  Scope_finish(scope);
  scope = scope_unnest(scope);
}

void gen_lambda(Scope *scope, Node *node)
{
  Scope_pushCode(OP_LAMBDA);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  Scope_pushCode(scope->next_lower_number);
  gen_irep(scope, node->cons.cdr);
  scope->sp--;
}

void gen_block(Scope *scope, Node *node)
{
  Scope_pushCode(OP_BLOCK);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  Scope_pushCode(scope->next_lower_number);
  gen_irep(scope, node->cons.cdr);
}

void gen_def(Scope *scope, Node *node, bool is_singleton)
{
  if (is_singleton) {
    Node *singleton_node = node->cons.cdr;
    node->cons.cdr = NULL;
    codegen(scope, node);
    node = singleton_node;
    Scope_pushCode(OP_SCLASS);
    Scope_pushCode(scope->sp);
  } else {
    Scope_pushCode(OP_TCLASS);
    Scope_pushCode(scope->sp);
  }
  Scope_push(scope);
  Scope_pushCode(OP_METHOD);
  Scope_pushCode(scope->sp);
  Scope_pushCode(scope->next_lower_number);
  Scope_pushCode(OP_DEF);
  Scope_pushCode(--scope->sp);
  int litIndex = Scope_newSym(scope, Node_literalName(node));
  Scope_pushCode(litIndex);

  gen_irep(scope, node->cons.cdr->cons.cdr);
}

void gen_sclass(Scope *scope, Node *node)
{
  codegen(scope, node->cons.car);
  Scope_pushCode(OP_SCLASS);
  Scope_pushCode(scope->sp);
  Scope_pushCode(OP_EXEC);
  Scope_pushCode(scope->sp);
  Scope_pushCode(scope->next_lower_number);
  scope = scope_nest(scope);
  codegen(scope, node->cons.cdr);
  Scope_pushCode(OP_RETURN);
  Scope_pushCode(scope->sp);
  Scope_finish(scope);
  scope = scope_unnest(scope);
}

void gen_class_module(Scope *scope, Node *node, AtomType type)
{
  int litIndex;
  bool colon2_flag = false;
  if (type == ATOM_module) {
    if (Node_atomType(node->cons.car) == ATOM_colon2) {
      codegen(scope, node->cons.car->cons.cdr);
      colon2_flag = true;
    } else if (Node_atomType(node->cons.car) == ATOM_colon3) {
      Scope_pushCode(OP_OCLASS);
      Scope_pushCode(scope->sp);
    } else {
      Scope_pushCode(OP_LOADNIL);
      Scope_pushCode(scope->sp);
    }
    Scope_pushCode(OP_MODULE);
    Scope_pushCode(scope->sp);
    if (colon2_flag) {
      litIndex = Scope_newSym(scope, Node_literalName(node->cons.car->cons.cdr->cons.cdr->cons.car));
    } else {
      litIndex = Scope_newSym(scope, Node_literalName(node->cons.car->cons.cdr));
    }
    Scope_pushCode(litIndex);
  } else { /* ATOM_class */
    if (Node_atomType(node->cons.car) == ATOM_colon2) {
      codegen(scope, node->cons.car->cons.cdr);
      colon2_flag = true;
    } else {
      if (Node_atomType(node->cons.car) == ATOM_colon3) {
        Scope_pushCode(OP_OCLASS);
      } else {
        Scope_pushCode(OP_LOADNIL);
      }
      Scope_pushCode(scope->sp);
    }
    Scope_push(scope);
    if (Node_atomType(node->cons.cdr->cons.car) == ATOM_at_const) {
      Scope_pushCode(OP_GETCONST);
      Scope_pushCode(scope->sp--);
      litIndex = Scope_newSym(scope, Node_literalName(node->cons.cdr->cons.car->cons.cdr));
      Scope_pushCode(litIndex);
    } else {
      Scope_pushCode(OP_LOADNIL);
      Scope_pushCode(scope->sp--);
    }
    Scope_pushCode(OP_CLASS);
    Scope_pushCode(scope->sp);
    if (colon2_flag) {
      litIndex = Scope_newSym(scope, Node_literalName(node->cons.car->cons.cdr->cons.cdr->cons.car));
    } else {
      litIndex = Scope_newSym(scope, Node_literalName(node->cons.car->cons.cdr));
    }
    Scope_pushCode(litIndex);
  }
  if (node->cons.cdr->cons.cdr->cons.car->cons.cdr == NULL) {
    /* empty class/module */
    Scope_pushCode(OP_LOADNIL);
    Scope_pushCode(scope->sp);
    Scope *target = scope->first_lower;
    Scope *prev = NULL;
    for (uint16_t i = 0; i < scope->next_lower_number; i++) {
      prev = target;
      target = target->next;
    }
    if (prev) {
      prev->next = target->next; /* possibly NULL */
    } else {
      scope->first_lower = target->next;
    }
    scope->nlowers--;
    picorbc_free(target->first_code_pool); /* should be only one */
    target->next = NULL;
    Scope_free(target);
  } else {
    if (type == ATOM_class) {
      node->cons.cdr->cons.car = NULL; /* Stop generating super class CONST */
    }
    Scope_pushCode(OP_EXEC);
    Scope_pushCode(scope->sp);
    Scope_pushCode(scope->next_lower_number);
    scope = scope_nest(scope);
    codegen(scope, node->cons.cdr);
    Scope_pushCode(OP_RETURN);
    Scope_pushCode(scope->sp);
    Scope_finish(scope);
    scope = scope_unnest(scope);
  }
}

/*
 * OP_BLKPUSH B bb
 * OP_ARGARY  B bb
 * bb: 0000000000000000
 *     ^^^^^ ^^^^^ ^^^^
 *      m1    m2    lv
 *          ^     ^
 *          r     d
 */
void gen_super_bb(Scope *scope)
{
  Scope *target = scope;
  int b = 0;
  while (!target->lvar_top) {
    target = target->upper;
    b = 1;
  }
  uint32_t bbb = target->irep_parameters;
  uint16_t bb = ( (bbb >> 18 & 0x1f) + (bbb >> 13 & 0x1f) ) << 11 | // m1
                (bbb>>7 & 0x3f) << 5 |                              // r m2
                (bbb>>1 & 1) << 4 |                                 // d
                (bbb>>2 & 0x1f ) << 4 |                             // keyword -> lv
                (b); // block. But I'm not sure if this is correct.
  Scope_pushCode((uint8_t)(bb >> 8));
  Scope_pushCode((uint8_t)(bb & 0xff));
}

void gen_super(Scope *scope, Node *node, bool zsuper)
{
  Scope_push(scope);
  GenValuesResult result = {0};
  result.is_super = true;
  gen_values(scope, node, &result);
  if (zsuper && scope->irep_parameters > 1) {
    Scope_pushCode(OP_ARGARY);
    Scope_pushCode(scope->sp);
    gen_super_bb(scope);
  } else {
    if (!result.has_block && !result.has_block_arg) {
      Scope_pushCode(OP_MOVE);
      Scope_pushCode(scope->sp);
      Lvar *lvar = scope->lvar;
      while (lvar->next) lvar = lvar->next;
      Scope_pushCode(lvar->regnum); // block var
    }
  }
  if (!result.has_splat) {
    scope->sp -= result.nargs;
  } else {
    scope->sp--;
  }
  Scope_pushCode(OP_SUPER);
  if (result.has_block) {
    --scope->sp;
    Scope_pushCode(--scope->sp);
    Scope_pushCode(result.nargs);
  } else {
    Scope_pushCode(--scope->sp);
    if (scope->irep_parameters > 1) {
      Scope_pushCode(15);
    } else {
      Scope_pushCode(result.nargs);
    }
  }
}

void gen_yield(Scope *scope, Node *node)
{
  GenValuesResult result = {0};
  if (node->cons.cdr->cons.car) {
    Scope_push(scope);
    gen_values(scope, node, &result);
    scope->sp -= result.nargs + 1;
  }
  Scope_pushCode(OP_BLKPUSH);
  Scope_pushCode(scope->sp);
  gen_super_bb(scope);
  Scope_pushCode(OP_SEND);
  Scope_pushCode(scope->sp);
  Scope_pushCode(Scope_newSym(scope, "call"));
  if (result.has_splat) {
    Scope_pushCode(127); //TODO ???????
  } else {
    Scope_pushCode(result.nargs);
  }
}

void gen_return(Scope *scope, Node *node)
{
  if (node->cons.cdr->cons.car) {
    int reg = scope->sp;
    codegen(scope, node->cons.cdr->cons.car);
    Scope_setSp(scope, reg);
  } else {
    Scope_pushCode(OP_LOADNIL);
    Scope_pushCode(scope->sp);
  }
  Scope_pushCode(scope->lvar_top ? OP_RETURN : OP_RETURN_BLK);
  Scope_pushCode(scope->sp);
}

void write_exc_handler(uint8_t *record, uint32_t value)
{
  record[0] = (value & 0xFF000000) >> 24;
  record[1] = (value & 0xFF0000) >> 16;
  record[2] = (value & 0xFF00) >> 8;
  record[3] =  value & 0xFF;
}

ExcHandler *new_exc_handler(Scope *scope, uint8_t type)
{
  ExcHandler *exc_handler = picorbc_alloc(sizeof(ExcHandler));
  scope->clen++;
  exc_handler->next = NULL;
  exc_handler->table[0] = type;
  if (scope->exc_handler == NULL) {
    scope->exc_handler = exc_handler;
  } else {
    ExcHandler *tmp;
    for (tmp = scope->exc_handler; tmp->next != NULL; tmp = tmp->next);
    tmp->next = exc_handler;
  }
  return exc_handler;
}

void gen_ensure(Scope *scope, Node *node)
{
  ExcHandler *exc_handler = new_exc_handler(scope, 1);
  write_exc_handler(&exc_handler->table[1], scope->vm_code_size);
  codegen(scope, node->cons.cdr->cons.car);
  write_exc_handler(&exc_handler->table[5], scope->vm_code_size);
  write_exc_handler(&exc_handler->table[9], scope->vm_code_size);
  Scope_push(scope);
  Scope_push(scope);
  Scope_pushCode(OP_EXCEPT);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  codegen(scope, node->cons.cdr->cons.cdr);
  scope->sp--;
  Scope_pushCode(OP_RAISEIF);
  Scope_pushCode(scope->sp);
  scope->sp--;
  scope->sp--;
}

JmpLabel *gen_another_rescue(Scope *scope, Node *node)
{
  /* prepare exc_list */
  int nexc = count_args(scope, node->cons.car);
  JmpLabel **labels_rescue = (JmpLabel **)picorbc_alloc(sizeof(JmpLabel*) * nexc);
  { /* exc_list */
    Node *tmp, *tmp2;
    for (int i = 0; i < nexc;  i++) {
      tmp = node->cons.car;
      for (int j = nexc; i < j; j--) tmp = tmp->cons.cdr->cons.car;
      bool splat = false;
      if (Node_atomType(tmp->cons.cdr->cons.car) == ATOM_args_new) {
        if (Node_atomType(tmp->cons.cdr->cons.car->cons.cdr->cons.car) == ATOM_splat) {
          splat = true;
          tmp = tmp->cons.cdr->cons.car;
        }
        tmp2 = NULL;
      } else {
        tmp2 = tmp->cons.cdr->cons.car;
        tmp->cons.cdr->cons.car = NULL;
        if (Node_atomType(tmp->cons.cdr->cons.cdr->cons.car) == ATOM_splat) {
          splat = true;
          tmp = tmp->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car;
        }
      }
      if (splat) {
        codegen(scope, tmp->cons.cdr->cons.car->cons.cdr);
        Scope_pushCode(OP_MOVE);
        Scope_pushCode(scope->sp + 1);
        Scope_pushCode(scope->sp - 1);
        Scope_pushCode(OP_SEND);
        Scope_pushCode(scope->sp);
        Scope_pushCode(Scope_newSym(scope, "__case_eqq"));
        Scope_pushCode(1);
      } else {
        codegen(scope, tmp);
        Scope_pushCode(OP_RESCUE);
        Scope_pushCode(--scope->sp - 1);
        Scope_pushCode(scope->sp);
      }
      tmp->cons.cdr->cons.car = tmp2;
      Scope_pushCode(OP_JMPIF);
      Scope_pushCode(scope->sp);
      labels_rescue[i] = reserve_jmpLabel(scope);
    }
    scope->sp--;
  }
  Scope_pushCode(OP_JMP);
  JmpLabel *label_rescue_missed = reserve_jmpLabel(scope);
  for (int i = 0; i < nexc; i++)
    backpatch_jmpLabel(labels_rescue[i], scope->vm_code_size);
  /* free */
  picorbc_free(labels_rescue);
  { /* exc_var */
    if (node->cons.cdr->cons.car->cons.cdr->cons.car)
    gen_assign(scope, node->cons.cdr->cons.car->cons.cdr, scope->sp);
  }
  /* rescue */
  int pc = scope->vm_code_size;
  codegen(scope, node->cons.cdr->cons.cdr->cons.car);
  if (pc == scope->vm_code_size) {
    Scope_pushCode(OP_LOADNIL);
    Scope_pushCode(scope->sp);
  }
  Scope_pushCode(OP_JMP);
  JmpLabel *label_else = reserve_jmpLabel(scope);
  backpatch_jmpLabel(label_rescue_missed, scope->vm_code_size);
  return label_else;
}

void gen_rescue(Scope *scope, Node *node)
{
  ExcHandler *exc_handler = new_exc_handler(scope, 0);
  write_exc_handler(&exc_handler->table[1], scope->vm_code_size);
  int pc = scope->vm_code_size;
  push_retryStack(scope);
  /* main stmt */
  codegen(scope, node->cons.cdr->cons.car);
  if (pc == scope->vm_code_size) {
    Scope_pushCode(OP_LOADNIL);
    Scope_pushCode(scope->sp);
  }
  write_exc_handler(&exc_handler->table[5], scope->vm_code_size);
  Scope_pushCode(OP_JMP);
  JmpLabel *label_bottom = reserve_jmpLabel(scope);
  write_exc_handler(&exc_handler->table[9], scope->vm_code_size);
  Scope_pushCode(OP_EXCEPT);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  /* rescue */
  Node *node_rescue = node->cons.cdr->cons.cdr;
  int nrescue = 0;
  while (Node_atomType(node_rescue->cons.car) == ATOM_exc_list) {
    nrescue++;
    node_rescue = node_rescue->cons.cdr->cons.cdr->cons.cdr;
  }
  JmpLabel *labels_else[nrescue];
  node_rescue = node->cons.cdr->cons.cdr;
  for (int i = 0; i < nrescue; i++) {
    labels_else[i] = gen_another_rescue(scope, node_rescue);
    Scope_push(scope);
    node_rescue = node_rescue->cons.cdr->cons.cdr->cons.cdr;
  }
  scope->sp--;
  Scope_pushCode(OP_RAISEIF);
  Scope_pushCode(scope->sp);
  backpatch_jmpLabel(label_bottom, scope->vm_code_size);
  /* else */
  codegen(scope, node->cons.cdr->cons.cdr->cons.cdr->cons.cdr->cons.cdr->cons.car);
  for (int i = 0; i < nrescue; i++)
    backpatch_jmpLabel(labels_else[i], scope->vm_code_size);
  pop_retryStack(scope);
}

void gen_retry(Scope *scope, Node *node)
{
  int16_t s = scope->g->retry_stack->pos - scope->vm_code_size - 3;
  Scope_pushCode(OP_JMPUW);
  Scope_pushCode((uint8_t)(s >> 8));
  Scope_pushCode((uint8_t)(s & 0xff));
  codegen(scope, node->cons.cdr);
}

void codegen(Scope *scope, Node *tree)
{
  int num;
  if (tree == NULL || !Node_isCons(tree)) return;
  switch (Node_atomType(tree)) {
    case ATOM_and:
      gen_and_or(scope, tree->cons.cdr, OP_JMPNOT);
      break;
    case ATOM_or:
      gen_and_or(scope, tree->cons.cdr, OP_JMPIF);
      break;
    case ATOM_case:
      gen_case(scope, tree->cons.cdr);
      break;
    case ATOM_if:
      gen_if(scope, tree->cons.cdr);
      break;
    case ATOM_while:
      gen_while(scope, tree->cons.cdr, OP_JMPIF);
      break;
    case ATOM_until:
      gen_while(scope, tree->cons.cdr, OP_JMPNOT);
      break;
    case ATOM_break:
      gen_break(scope, tree->cons.cdr);
      break;
    case ATOM_next:
      gen_next(scope, tree->cons.cdr);
      break;
    case ATOM_redo:
      gen_redo(scope);
      break;
    case ATOM_NONE:
      codegen(scope, tree->cons.car);
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_program:
      scope->nest_stack = 1; /* 00000000 00000000 00000000 00000001 */
      codegen(scope, tree->cons.cdr->cons.car);
      if (scope->current_code_pool->data[scope->current_code_pool->index - 2] != OP_RETURN) {
        Scope_pushCode(OP_RETURN);
        Scope_pushCode(scope->sp);
      }
      Scope_pushCode(OP_STOP);
      Scope_finish(scope);
      break;
    case ATOM_stmts_add:
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_stmts_new: // NEW_BEGIN
      break;
    case ATOM_masgn:
      gen_masgn(scope, tree->cons.cdr);
      break;
    case ATOM_assign:
      gen_assign(scope, tree->cons.cdr, 0);
      break;
    case ATOM_assign_backpatch:
      if (!scope->g->backpatch) return;
      backpatch_jmpLabel(scope->g->backpatch->label, scope->vm_code_size);
      shift_backpatch(scope);
      gen_assign(scope, tree->cons.cdr, 0);
      break;
    case ATOM_op_assign:
      gen_op_assign(scope, tree->cons.cdr);
      break;
    case ATOM_command:
    case ATOM_fcall:
      gen_call(scope, tree->cons.cdr, true, false);
      break;
    case ATOM_call:
      gen_call(scope, tree->cons.cdr, false, false);
      break;
    case ATOM_scall:
      gen_call(scope, tree->cons.cdr, false, true);
      break;
    case ATOM_args_add:
      scope->g->nargs_added++;
      codegen(scope, tree->cons.cdr);
      scope->g->nargs_added--;
      if (scope->g->gen_array_status > GEN_ARRAY_STATUS_NONE) {
        if (scope->g->gen_array_count == PICORUBY_ARRAY_SPLIT_COUNT - 1) {
          scope->sp -= PICORUBY_ARRAY_SPLIT_COUNT - 1;
          Scope_pushCode(OP_ARRAY);
          Scope_pushCode(scope->sp);
          Scope_pushCode(scope->g->gen_array_count + 1);
          if (scope->g->gen_array_status == GEN_ARRAY_STATUS_GENERATING_SPLIT) {
            Scope_pushCode(OP_ADD);
            Scope_pushCode(--scope->sp);
          }
          scope->g->gen_array_status = GEN_ARRAY_STATUS_GENERATING_SPLIT;
          scope->g->gen_array_count = 0;
        } else {
          scope->g->gen_array_count++;
        }
      }
      if (scope->g->gen_splat_status == GEN_SPLAT_STATUS_BEFORE_SPLAT) {
        scope->g->nargs_before_splat++;
      } else if (
        scope->g->gen_splat_status == GEN_SPLAT_STATUS_AFTER_SPLAT &&
        scope->current_code_pool->data[scope->current_code_pool->index - 2] != OP_ARYCAT
        )
      {
        scope->g->nargs_before_splat = 0;
        scope->g->nargs_after_splat++;
        if (scope->g->nargs_added == 0) {
          Scope_pushCode(OP_ARYPUSH);
          Scope_pushCode(scope->sp - scope->g->nargs_after_splat);
          Scope_pushCode(scope->g->nargs_after_splat);
        }
      }
      Scope_push(scope);
      break;
    case ATOM_args_new:
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_symbol_literal:
      gen_sym(scope, tree->cons.cdr);
      break;
    case ATOM_dsymbol:
      gen_dstr(scope, tree->cons.cdr->cons.car);
      Scope_pushCode(OP_INTERN);
      Scope_pushCode(scope->sp - 1);
      break;
    case ATOM_str:
      gen_str(scope, tree->cons.cdr);
      break;
    case ATOM_var_ref:
      gen_var(scope, tree->cons.cdr->cons.car);
      break;
    case ATOM_at_int:
      gen_int(scope, tree->cons.cdr, NUM_POS);
      break;
    case ATOM_at_float:
      gen_float(scope, tree->cons.cdr, NUM_POS);
      break;
    case ATOM_unary:
      switch (Node_atomType(tree->cons.cdr->cons.cdr->cons.car)) {
        case (ATOM_at_int):
          gen_int(scope, tree->cons.cdr->cons.cdr->cons.car->cons.cdr, NUM_NEG);
          break;
        case (ATOM_at_float):
          gen_float(scope, tree->cons.cdr->cons.cdr->cons.car->cons.cdr, NUM_NEG);
          break;
        default:
          break;
      }
      break;
    case ATOM_array:
      gen_array(scope, tree, 0);
      break;
    case ATOM_hash:
      gen_hash(scope, tree->cons.cdr, false);
      break;
    case ATOM_at_ivar:
    case ATOM_at_gvar:
    case ATOM_at_const:
      switch (Node_atomType(tree)) {
        case ATOM_at_ivar:
          Scope_pushCode(OP_GETIV);
          break;
        case (ATOM_at_gvar):
          Scope_pushCode(OP_GETGV);
          break;
        case (ATOM_at_const):
          Scope_pushCode(OP_GETCONST);
          break;
        default:
          FATALP("error");
      }
      Scope_pushCode(scope->sp);
      num = Scope_newSym(scope, Node_literalName(tree->cons.cdr));
      Scope_pushCode(num);
      break;
    case ATOM_lvar:
      gen_var(scope, tree);
      break;
    case ATOM_kw_nil:
      Scope_pushCode(OP_LOADNIL);
      Scope_pushCode(scope->sp);
      break;
    case ATOM_kw_self:
      gen_self(scope);
      break;
    case ATOM_kw_true:
      Scope_pushCode(OP_LOADT);
      Scope_pushCode(scope->sp);
      break;
    case ATOM_kw_false:
      Scope_pushCode(OP_LOADF);
      Scope_pushCode(scope->sp);
      break;
    case ATOM_kw_return:
      gen_return(scope, tree);
      break;
    case ATOM_kw_yield:
      gen_yield(scope, tree);
      break;
    case ATOM_dstr:
      gen_dstr(scope, tree);
      break;
    case ATOM_block:
      gen_block(scope, tree);
      break;
    case ATOM_arg:
      break;
    case ATOM_splat:
      gen_splat(scope, tree->cons.cdr);
      break;
    case ATOM_kw_hash:
      Scope_push(scope);
      gen_hash(scope, tree->cons.cdr, true);
      Scope_push(scope);
      break;
    case ATOM_block_arg:
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_def:
      gen_def(scope, tree->cons.cdr, false);
      break;
    case ATOM_sdef:
      gen_def(scope, tree->cons.cdr, true);
      break;
    case ATOM_sclass:
      gen_sclass(scope, tree->cons.cdr);
      break;
    case ATOM_class:
    case ATOM_module:
      gen_class_module(scope, tree->cons.cdr, Node_atomType(tree));
      break;
    case ATOM_alias:
      gen_alias(scope, tree->cons.cdr);
      break;
    case ATOM_dot2:
      gen_dot2_3(scope, tree->cons.cdr, OP_RANGE_INC);
      break;
    case ATOM_dot3:
      gen_dot2_3(scope, tree->cons.cdr, OP_RANGE_EXC);
      break;
    case ATOM_colon2:
      gen_colon2(scope, tree->cons.cdr);
      break;
    case ATOM_colon3:
      gen_colon3(scope, tree->cons.cdr);
      break;
    case ATOM_super:
      gen_super(scope, tree, false);
      break;
    case ATOM_zsuper:
      gen_super(scope, tree, true);
      break;
    case ATOM_lambda:
      gen_lambda(scope, tree);
      break;
    case ATOM_ensure:
      gen_ensure(scope, tree);
      break;
    case ATOM_rescue:
      gen_rescue(scope, tree);
      break;
    case ATOM_retry:
      gen_retry(scope, tree);
      break;
    default:
      // FIXME: `Unkown OP code: 2e`
//      FATALP("Unkown OP code: %x", Node_atomType(tree));
      break;
  }
}

void memcpyFlattenCode(uint8_t *body, CodePool *code_pool)
{
  if (code_pool->next == NULL && code_pool->index == code_pool->size) return;
  int pos = 0;
  while (code_pool) {
    memcpy(&body[pos], code_pool->data, code_pool->index);
    pos += code_pool->index;
    code_pool = code_pool->next;
  }
}


static uint8_t *
writeCode(Scope *scope, uint8_t *pos, bool verbose)
{
  if (scope == NULL) return pos;
  memcpyFlattenCode(pos, scope->first_code_pool);
  scope->vm_code = pos; /* used in dump.c */
  if (verbose) Dump_hexDump(stdout, pos);
  pos += scope->vm_code_size;
  pos = writeCode(scope->first_lower, pos, verbose);
  pos = writeCode(scope->next, pos, verbose);
  return pos;
}

void Generator_generate(Scope *scope, Node *root, bool verbose)
{
  GeneratorState *g = picorbc_alloc(sizeof(GeneratorState));
  memset(g, 0, sizeof(GeneratorState));
  scope->g = g;
  codegen(scope, root);
  picorbc_free(g);
  int irepSize = Scope_updateVmCodeSizeThenReturnTotalSize(scope);
  int32_t codeSize = MRB_HEADER_SIZE + irepSize + MRB_FOOTER_SIZE;
  uint8_t *vmCode = picorbc_alloc(codeSize);
  memcpy(&vmCode[0], "RITE0300", 8);
  vmCode[8] = (codeSize >> 24) & 0xff;
  vmCode[9] = (codeSize >> 16) & 0xff;
  vmCode[10] = (codeSize >> 8) & 0xff;
  vmCode[11] = codeSize & 0xff;
  memcpy(&vmCode[12], "MATZ0000", 8);
  memcpy(&vmCode[20], "IREP", 4);
  int sectionSize = irepSize + MRB_FOOTER_SIZE + 4;
  vmCode[24] = (sectionSize >> 24) & 0xff; // size of the section
  vmCode[25] = (sectionSize >> 16) & 0xff;
  vmCode[26] = (sectionSize >> 8) & 0xff;
  vmCode[27] = sectionSize & 0xff;
  memcpy(&vmCode[28], "0300", 4); // instruction version
  writeCode(scope, &vmCode[MRB_HEADER_SIZE], verbose);
  memcpy(&vmCode[MRB_HEADER_SIZE + irepSize], "END\0\0\0\0", 7);
  vmCode[codeSize - 1] = 0x08;
  scope->vm_code = vmCode;
  scope->vm_code_size = codeSize;
}
