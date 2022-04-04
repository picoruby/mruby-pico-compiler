#ifndef PICORBC_COMPILE_H_
#define PICORBC_COMPILE_H_

#include <stdbool.h>

#include "scope.h"
#include "stream.h"
#include "parse_header.h"
#include "context.h"

bool Compiler_compile(ParserState *p, StreamInterface *si, picorbc_context *cxt);

ParserState *Compiler_parseInitState(uint8_t node_box_size);

void Compiler_parserStateFree(ParserState *p);

#endif /* PICORBC_COMPILE_H_*/
