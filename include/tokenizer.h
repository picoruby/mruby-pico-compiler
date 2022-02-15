#ifndef PICORBC_TOKENIZER_H_
#define PICORBC_TOKENIZER_H_

#include <stdbool.h>

#include "parse_header.h"
#include "token.h"
#include "stream.h"

typedef enum mode
{
  MODE_NONE,
  MODE_COMMENT,
  MODE_QWORDS,
  MODE_WORDS,
  MODE_QSYMBOLS,
  MODE_SYMBOLS,
  MODE_TSTRING_DOUBLE,
  MODE_TSTRING_SINGLE,
} Mode;

typedef struct tokenizer
{
  Mode mode;
  char *line;
  StreamInterface *si;
  Token *currentToken;
  int line_num;
  int pos;
  char modeTerminater;
} Tokenizer;

Tokenizer* const Tokenizer_new(ParserState *p, StreamInterface *si);

void Tokenizer_free(Tokenizer *self);

void Tokenizer_puts(Tokenizer* const self, char *line);

bool Tokenizer_hasMoreTokens(Tokenizer* const self);

int Tokenizer_advance(Tokenizer* const self, ParserState *p, bool recursive);

#endif
