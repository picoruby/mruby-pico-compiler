#ifndef PICORBC_DUMP_H_
#define PICORBC_DUMP_H_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "../include/opcode.h"

void Dump_hexDump(FILE *fp, uint8_t *irep);

#endif /* PICORBC_DUMP_H_ */
