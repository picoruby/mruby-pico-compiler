#ifndef PICORBC_DUMP_H_
#define PICORBC_DUMP_H_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "opcode.h"
#include "scope.h"

void Dump_hexDump(FILE *fp, uint8_t *irep);

int Dump_mrbDump(FILE *fp, Scope *scope, const char *initname);

#endif /* PICORBC_DUMP_H_ */
