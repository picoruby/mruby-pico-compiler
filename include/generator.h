#ifndef PICORBC_GENERATOR_H_
#define PICORBC_GENERATOR_H_

#include <stdint.h>

#include "scope.h"

#define HEADER_SIZE 32
#define FOOTER_SIZE 8

typedef struct generator_state
{
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
