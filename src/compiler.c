#include <stdbool.h>

#include "parse.c"
#include <token_helper.h>
#include <common.h>
#include <compiler.h>
#include <debug.h>
#include <generator.h>
#include <scope.h>
#include <stream.h>
#include <token.h>
#include <tokenizer.h>
#include <tokenizer_helper.h>
#include <my_regex.h>

#define ZERO     "\e[30;1m"
#define CTRL     "\e[35;1m"
#define LETTER   "\e[36m"
#define EIGHTBIT "\e[33m"
#define DUMP_LINE_LEN 210
void dumpCode(Scope *scope)
{
  char line[DUMP_LINE_LEN];
  memset(line, '\0', DUMP_LINE_LEN);
  int linelen = 0;
  int c, i, j;
  for (i = 0; i < scope->vm_code_size; i++) {
    c = scope->vm_code[i];
    if (linelen == 8) strsafecat(line, "|", DUMP_LINE_LEN);
    if (i != 0) {
      if (i % 16 == 0) {
        printf(" %s\n", line);
        linelen = 0;
        memset(line, '\0', 18);
      } else if (i % 8 == 0) {
        printf("| ");
      }
    }
    if (c == 0) {
      printf(ZERO);
      strsafecat(line, ZERO, DUMP_LINE_LEN);
      strsafecat(line, "0", DUMP_LINE_LEN);
    } else if (c < 0x20) {
      printf(CTRL);
      strsafecat(line, CTRL, DUMP_LINE_LEN);
      strsafecat(line, "\uFFED", DUMP_LINE_LEN);
    } else if (c < 0x7f) {
      printf(LETTER);
      strsafecat(line, LETTER, DUMP_LINE_LEN);
      strsafecat(line, (char *)&c, DUMP_LINE_LEN);
    } else {
      printf(EIGHTBIT);
      strsafecat(line, EIGHTBIT, DUMP_LINE_LEN);
      strsafecat(line, "\uFFED", DUMP_LINE_LEN);
    }
    strsafecat(line, "\e[m", DUMP_LINE_LEN);
    linelen++;
    printf("%02x\e[m ", c);
  }
  if (linelen != 16) {
    if (i % 16 < 9) printf("  "); /* equiv size to "| " */
    for (j = i % 16; j < 16; j++) printf("   ");
  }
  printf(" %s\n", line);
}

void
printToken(Tokenizer *tokenizer, Token *token) {
  printf("\e[32;40;1mtoken> %s\e[m \e[35;40;1m%s\e[m `\e[31;40;1m%s\e[m` \e[36;40;1m%s\e[m len=%ld line=%d pos=%d\n",
     tokenizer_mode_name(tokenizer->mode),
     token_name(token->type),
     token->value,
     tokenizer_state_name(token->state),
     strlen(token->value),
     token->line_num,
     token->pos);
}

bool Compiler_compile(ParserState *p, StreamInterface *si)
{
  /* unusing global_preg_cache prefers smaller RAM consumption */
  MyRegex_setup(false);
  /* using global_preg_cache prefers fewer number of step */
  // MyRegex_setup(true);
  Tokenizer *tokenizer = Tokenizer_new(p, si);
  Token *topToken = tokenizer->currentToken;
  yyParser *parser = ParseAlloc(picorbc_alloc, p);
#ifdef PICORUBY_DEBUG
  if (p->verbose) {
    ParseTrace(stdout, (char *)"parse> ");
  }
#endif
  Type prevType = 0;
  while( Tokenizer_hasMoreTokens(tokenizer) ) {
    if (Tokenizer_advance(tokenizer, p, false) > 0) {
      p->error_count++;
      break;
    }
    for (;;) {
      if (topToken->value == NULL) {
        DEBUGP("(main)%p null", topToken);
      } else {
        if (topToken->type != ON_SP) {
          if (p->verbose) printToken(tokenizer, topToken);
          const char *string;
          switch (topToken->type) {
            case IVAR:
            case GVAR:
            case CHAR:
            case LABEL:
            case INTEGER:
            case FLOAT:
            case IDENTIFIER:
            case CONSTANT:
            case STRING:
            case OP_ASGN:
            case DSTRING_TOP:
            case DSTRING_MID:
              string = ParsePushStringPool(p, topToken->value);
              break;
            case COMMENT:
              string = NULL;
              break;
            default:
              string = topToken->value;
              break;
          }
          if ((prevType == DSTRING_END || prevType == STRING_BEG)
              && topToken->type == STRING_END) {
            Parse(parser, STRING, ""); /* to help pareser */
          }
          if (topToken->type != STRING_END && topToken->type != COMMENT) {
            Parse(parser, topToken->type, string);
          }
          if (p->error_count != 0) {
            Token_free(topToken);
            goto FAIL;
          }
          prevType = topToken->type;
        }
      }
      if (topToken->next == NULL) {
        break;
      } else {
        topToken->refCount--;
        topToken = topToken->next;
      }
    }
  }
  Parse(parser, 0, "");
FAIL:
  MyRegexCache_free();
  bool success;
  if (p->error_count == 0) {
    success = true;
    if (p->verbose) ParseShowAllNode(parser, 1);
    Generator_generate(p->scope, p->root_node_box->nodes, p->verbose);
  } else {
    // FIXME should print prev line
    ERRORP("Syntax error at line:%d", tokenizer->line_num);
    success = false;
  }
  if (p->verbose && success) dumpCode(p->scope);
  ParseFreeAllNode(parser);
  picorbc_free(parser);
  Tokenizer_free(tokenizer);
  return success;
}

ParserState *
Compiler_parseInitState(uint8_t node_box_size)
{
  ParserState *p = ParseInitState(node_box_size);
  return p;
}

void
Compiler_parserStateFree(ParserState *p)
{
  ParserStateFree(p);
}
