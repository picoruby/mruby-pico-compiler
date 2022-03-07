#ifndef PICORBC_DUMP_H_
#define PICORBC_DUMP_H_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "opcode.h"
#include "scope.h"

#define PICORB_DUMP_DEBUG_INFO 1
#define PICORB_DUMP_STATIC 2

#define PICORB_DUMP_OK                     0
#define PICORB_DUMP_GENERAL_FAILURE      (-1)
#define PICORB_DUMP_WRITE_FAULT          (-2)
#define PICORB_DUMP_READ_FAULT           (-3)
#define PICORB_DUMP_INVALID_FILE_HEADER  (-4)
#define PICORB_DUMP_INVALID_IREP         (-5)
#define PICORB_DUMP_INVALID_ARGUMENT     (-6)

void Dump_hexDump(FILE *fp, uint8_t *irep);

int Dump_mrbDump(FILE *fp, Scope *scope, const char *initname);

int Dump_cstructDump(FILE *fp, Scope *scope, uint8_t flags, const char *initname);

#endif /* PICORBC_DUMP_H_ */
