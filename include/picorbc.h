#ifndef PICORBC_PICORBC_H_
#define PICORBC_PICORBC_H_

/*
 * banned.h occurs errors like
 * "error: expected ‘=’, ‘,’, ‘;’, ‘asm’ or ‘__attribute__’ before ‘{’ token"
 * because of banned.h
 * If PICORUBY_DEBUG is not defined, CFLAGS will contain "-O0" which can avoid the error above.
 * See picoruby/build_config/default.rb
 */
#ifdef PICORUBY_DEBUG
  #include "banned.h"
#endif

#include "version.h"
#include "debug.h"
#include "common.h"
#include "tokenizer.h"
#include "generator.h"
#include "scope.h"
#include "compiler.h"
#include "stream.h"
#include "parse.h"

#ifndef PICORUBY_ARRAY_SPLIT_COUNT
#define PICORUBY_ARRAY_SPLIT_COUNT 64
#endif

#ifndef PICORUBY_HASH_SPLIT_COUNT
#define PICORUBY_HASH_SPLIT_COUNT 64 // TODO: Alter into `32` when mruby/c implements OP_HASHADD
#endif

#endif /* PICORBC_PICORBC_H_ */
