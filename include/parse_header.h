#ifndef LEMON_PARSE_HEADER_H_
#define LEMON_PARSE_HEADER_H_

#include <stdbool.h>

#include "scope.h"
#include "token.h"
#ifndef PICORBC_PTR_SIZE
#include <ptr_size.h>
#endif

typedef enum atom_type {
  ATOM_NONE = 0,
  ATOM_program = 1,
  ATOM_method_add_arg,
  ATOM_kw_nil,
  ATOM_kw_true,
  ATOM_kw_false,
  ATOM_lvar,
  ATOM_array,
  ATOM_hash,
  ATOM_kw_hash,
  ATOM_assoc_new,
  ATOM_assoc_key,
  ATOM_assoc_value,
  ATOM_call,
  ATOM_scall,
  ATOM_fcall,
  ATOM_vcall,
  ATOM_command,
  ATOM_masgn,
  ATOM_assign,
  ATOM_assign_backpatch,
  ATOM_op_assign,
  ATOM_at_op,
  ATOM_and,
  ATOM_or,
  ATOM_var_field,
  ATOM_var_ref,
  ATOM_dstr,
  ATOM_str,
  ATOM_dstr_add,
  ATOM_dstr_new,
  ATOM_string_content,
  ATOM_mrhs,
  ATOM_mlhs,
  ATOM_mlhs_pre,
  ATOM_mlhs_rest,
  ATOM_mlhs_post,
  ATOM_args_new,
  ATOM_args_add,
  ATOM_args_add_block,
  ATOM_block_arg,
  ATOM_kw_self,
  ATOM_kw_return,
  ATOM_kw_yield,
  ATOM_at_int,
  ATOM_at_float,
  ATOM_stmts_add,
  ATOM_string_literal,
  ATOM_symbol_literal,
  ATOM_dsymbol,
  ATOM_binary,
  ATOM_unary,
  ATOM_stmts_new,
  ATOM_at_ident,
  ATOM_at_ivar,
  ATOM_at_gvar,
  ATOM_at_const,
  ATOM_at_tstring_content,
  ATOM_if,
  ATOM_while,
  ATOM_until,
  ATOM_case,
  ATOM_case_body,
  ATOM_break,
  ATOM_next,
  ATOM_redo,
  ATOM_block,
  ATOM_def,
  ATOM_sdef,
  ATOM_block_parameters,
  ATOM_arg,
  ATOM_margs,
  ATOM_optargs,
  ATOM_m2args,
  ATOM_args_tail,
  ATOM_args_tail_kw_arg,
  ATOM_args_tail_kw_rest_args,
  ATOM_args_tail_block,
  ATOM_restarg,
  ATOM_class,
  ATOM_sclass,
  ATOM_module,
  ATOM_alias,
  ATOM_dot2,
  ATOM_dot3,
  ATOM_colon2,
  ATOM_colon3,
  ATOM_splat,
  ATOM_super,
  ATOM_zsuper,
  ATOM_lambda,
  ATOM_keyword_rest_args,
  ATOM_rescue,
  ATOM_exc_list,
  ATOM_exc_var,
  ATOM_another_rescue,
  ATOM_ensure,
  ATOM_retry,
} AtomType;

typedef enum {
  ATOM,
  CONS,
  LITERAL,
} NodeType;

typedef struct node Node;

typedef struct {
  struct node *car;
  struct node *cdr;
} Cons;

typedef struct {
  AtomType type;
} Atom;

typedef struct {
  const char *name;
} Value;

struct node {
  NodeType type;
  union {
    Atom atom;
    Cons cons;
    Value value;
  };
};

typedef struct node_box NodeBox;

typedef struct node_box
{
  NodeBox *next;
  uint16_t size;
  uint16_t index;
  Node *nodes;
} NodeBox;

#define STRING_POOL_SIZE (PICORBC_PTR_SIZE * 16)
#define STRING_POOL_HEADER_SIZE (PICORBC_PTR_SIZE * 3)
#define STRING_POOL_POOL_SIZE (STRING_POOL_SIZE - STRING_POOL_HEADER_SIZE)
typedef struct string_pool StringPool;
typedef struct string_pool
{
  StringPool *prev;
  uint16_t size;  /* maximum size of strings */
  uint16_t index; /* current size + 1 of strings */
  char strings[STRING_POOL_POOL_SIZE];
} StringPool;

typedef struct specail_string_pool {
  char null[1]; /* "" */
  char neg[2];  /* "-" */
  char ary[3];  /* "[]" */
} SpecialStringPool;;

typedef enum paren
{
  PAREN_NONE,
  PAREN_PAREN,
  PAREN_BRACE,
} Paren;

/* margin size to malloc tokenizer */
#define PAREN_STACK_SIZE 40

typedef struct parser_state {
  Scope *scope;
  NodeBox *root_node_box;
  NodeBox *current_node_box;
  uint8_t node_box_size;
  StringPool *current_string_pool;
  SpecialStringPool special_string_pool;
  unsigned int error_count;
  unsigned int cond_stack;
  unsigned int cmdarg_stack;
  bool cmd_start;
  State state;
  bool verbose;
  int paren_stack_num;
  Paren paren_stack[PAREN_STACK_SIZE];
  unsigned int lpar_beg;
  int in_def;
  int in_single;
} ParserState;

#define BITSTACK_PUSH(stack, n) ((stack) = ((stack) << 1) | ((n) & 1))
#define BITSTACK_POP(stack)     ((stack) = (stack) >> 1)
#define BITSTACK_LEXPOP(stack)  ((stack) = ((stack) >> 1) | ((stack) & 1))
#define BITSTACK_SET_P(stack)   ((stack) & 1)

#define COND_PUSH(n)    BITSTACK_PUSH(p->cond_stack, (n))
#define COND_POP()      BITSTACK_POP(p->cond_stack)
#define COND_LEXPOP()   BITSTACK_LEXPOP(p->cond_stack)
#define COND_P()        BITSTACK_SET_P(p->cond_stack)

#define CMDARG_PUSH(n)  BITSTACK_PUSH(p->cmdarg_stack, (n))
#define CMDARG_POP()    BITSTACK_POP(p->cmdarg_stack)
#define CMDARG_LEXPOP() BITSTACK_LEXPOP(p->cmdarg_stack)
#define CMDARG_P()      BITSTACK_SET_P(p->cmdarg_stack)

//#define sym(x) ((mrb_sym)(intptr_t)(x))
#define nsym(x) ((Node*)(intptr_t)(x))
#define nint(x) ((Node*)(intptr_t)(x))
#define intn(x) ((int)(intptr_t)(x))
//#define typen(x) ((enum node_type)(intptr_t)(x))
#endif
