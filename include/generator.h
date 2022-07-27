#ifndef PICORBC_GENERATOR_H_
#define PICORBC_GENERATOR_H_

#include <stdint.h>

#include "scope.h"

#define HEADER_SIZE 32
#define FOOTER_SIZE 8

typedef struct jmp_label
{
  void *address;
  uint32_t pos;
} JmpLabel;

typedef struct backpatch
{
  JmpLabel *label;
  struct backpatch *next;
} Backpatch;

typedef struct break_stack
{
  void *point;
  struct break_stack *prev;
  uint32_t next_pos;
  uint32_t redo_pos;
} BreakStack;

typedef struct retry_stack
{
  uint32_t pos;
  struct retry_stack *prev;
} RetryStack;

typedef struct generator_state
{
  BreakStack *break_stack;
  RetryStack *retry_stack;
  Backpatch *backpatch; /* for backpatching of JMP label */
  uint16_t nargs_added;
  uint16_t nargs_before_splat;
  uint16_t nargs_after_splat;
  uint8_t gen_splat_status;
  uint8_t gen_array_status;
  uint8_t gen_array_count;
} GeneratorState;

typedef struct node Node;

void Generator_generate(Scope *scope, Node *root, bool verbose);

#endif
