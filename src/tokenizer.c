#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <picorbc.h>
#include <parse.h>
#include <keyword_helper.h>
#include <token_helper.h>
#include <common.h>
#include <tokenizer.h>
#include <token.h>
#include <my_regex.h>

#include <token_data.h>
#define IS_ARG() (p->state == EXPR_ARG || p->state == EXPR_CMDARG)
#define IS_END() (p->state == EXPR_END || p->state == EXPR_ENDARG || p->state == EXPR_ENDFN)
#define IS_BEG() (p->state & EXPR_BEG || p->state == EXPR_MID || p->state == EXPR_VALUE || p->state == EXPR_CLASS)

#define IS_NUM(n) ('0' <= self->line[self->pos+(n)] && self->line[self->pos+(n)] <= '9')

static bool tokenizer_is_operator(char *word, size_t len)
{
  switch (len) {
    case 3:
      for (int i = 0; OPERATORS_3[i].string != NULL; i++){
        if ( !strncmp(word, OPERATORS_3[i].string, len) )
          return true;
      }
      break;
    case 2:
      for (int i = 0; OPERATORS_2[i].string != NULL; i++){
        if ( !strncmp(word, OPERATORS_2[i].string, len) )
          return true;
      }
      break;
    default:
      ERRORP("Out of range!");
      break;
  }
  return false;
}

static bool tokenizer_is_paren(int letter)
{
  for (int i = 0; PARENS[i].letter != 0; i++){
    if ( letter == PARENS[i].letter )
      return true;
  }
  return false;
}

static bool tokenizer_is_semicolon(int letter)
{
  for (int i = 0; SEMICOLONS[i].letter != 0; i++){
    if ( letter == SEMICOLONS[i].letter )
      return true;
  }
  return false;
}

static bool tokenizer_is_comma(int letter)
{
  for (int i = 0; COMMAS[i].letter != 0; i++){
    if ( letter == COMMAS[i].letter )
      return true;
  }
  return false;
}

static void paren_stack_pop(ParserState *p)
{
  p->paren_stack_num--;
}

static void paren_stack_add(ParserState *p, Paren paren)
{
  p->paren_stack_num++;
  p->paren_stack[p->paren_stack_num] = paren;
}

Tokenizer* const Tokenizer_new(ParserState *p, StreamInterface *si)
{
  Tokenizer *self = (Tokenizer *)picorbc_alloc(sizeof(Tokenizer));
  { /* class vars in mmrbc.gem */
    self->currentToken = Token_new();
    self->line = (char *)picorbc_alloc(sizeof(char) * (MAX_LINE_LENGTH));
    self->line[0] = '\0';
    self->line_num = 0;
    self->pos = 0;
  }
  paren_stack_add(p, PAREN_NONE);
  { /* instance vars in mmrbc.gem */
    self->si = si;
    self->mode = MODE_NONE;
    self->modeTerminater = '\0';
    p->state = EXPR_BEG;
    p->cmd_start = true;
  }
  return self;
}

void Tokenizer_free(Tokenizer *self)
{
  Token_free(self->currentToken);
  picorbc_free(self->line);
  picorbc_free(self);
}

void tokenizer_readLine(Tokenizer* const self)
{
  DEBUGP("readLine line_num: %d, pos: %d", self->line_num, self->pos);
  if (self->pos >= strlen(self->line)) {
    if (self->si->fgetsProc(self->line, MAX_LINE_LENGTH, self->si->stream) == NULL)
      self->line[0] = '\0';
    DEBUGP("line size: %ld", strlen(self->line));
    self->line_num++;
    self->pos = 0;
  }
}

bool Tokenizer_hasMoreTokens(Tokenizer* const self)
{
  if (self->si->stream == NULL) {
    return false;
  } else if ( self->si->feofProc(self->si->stream) == 0 || (self->line[0] != '\0') ) {
    return true;
  } else {
    return false;
  }
}

void tokenizer_pushToken(Tokenizer *self, int line_num, int pos, Type type, char *value, State state)
{
  DEBUGP("pushToken: `%s`", value);
  self->currentToken->pos = pos;
  self->currentToken->line_num = line_num;
  self->currentToken->type = type;
  self->currentToken->value = (char *)picorbc_alloc(sizeof(char) * (strlen(value) + 1));
  self->currentToken->value[0] = '\0';
  strsafecpy(self->currentToken->value, value, strlen(value) + 1);
  DEBUGP("Pushed token: `%s`", self->currentToken->value);
  self->currentToken->state = state;
  Token *newToken = Token_new();
  newToken->prev = self->currentToken;
  self->currentToken->next = newToken;
  self->currentToken = newToken;
}

/*
 * Replace null letter with "\xF5PICORUBYNULL\xF5"
 *  -> see also replace_picoruby_null();
 */
#define REPLACE_NULL_PICORUBY \
  do { \
    strsafecat(value, "\xF5PICORUBYNULL", MAX_TOKEN_LENGTH); \
    c[0] = '\xF5'; \
  } while (0)

int Tokenizer_advance(Tokenizer* const self, ParserState *p, bool recursive)
{
  DEBUGP("Aadvance. mode: `%d`", self->mode);
  Token_GC(self->currentToken->prev);
  Token *lazyToken = Token_new();
  char value[MAX_TOKEN_LENGTH];
  char c[3];
  int cmd_state;
  cmd_state = p->cmd_start;
  p->cmd_start = false;
  bool ignore_keyword;
retry:
  ignore_keyword = false;
  memset(value, '\0', MAX_TOKEN_LENGTH);
  Type type = ON_NONE;
  c[0] = '\0';
  c[1] = '\0';
  c[2] = '\0';

  RegexResult regexResult[REGEX_MAX_RESULT_NUM];

  tokenizer_readLine(self);
  if (self->line[0] == '\0') {
    Token_free(lazyToken);
    return -1;
  }
  if (self->mode == MODE_COMMENT) {
    if (Regex_match2(self->line, "^=end$") || Regex_match2(self->line, "^=end\\s")) {
      type = EMBDOC_END;
    } else {
      type = EMBDOC;
    }
    self->mode = MODE_NONE;
    strsafecpy(value, self->line, MAX_TOKEN_LENGTH);
  } else if (self->mode == MODE_WORDS
          || self->mode == MODE_SYMBOLS) {
    for (;;) {
      tokenizer_readLine(self);
      value[0] = '\0';
      type = ON_NONE;
      if (self->line[self->pos] == self->modeTerminater) {
        lazyToken->line_num = self->line_num;
        lazyToken->pos = self->pos;
        lazyToken->type = STRING_END;
        lazyToken->value = (char *)picorbc_alloc(sizeof(char) *(2));
        *(lazyToken->value) = self->modeTerminater;
        *(lazyToken->value + 1) = '\0';
        lazyToken->state = EXPR_END;
        p->state = EXPR_END;
        self->pos++;
        self->mode = MODE_NONE;
        break;
      } else if (self->line[self->pos] == ' ' ||
                 self->line[self->pos] == '\n' ||
                 self->line[self->pos] == '\r') {
        Regex_match3(&(self->line[self->pos]), "^(\\s+)", regexResult);
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = WORDS_SEP;
      } else {
        int i = 0;
        for (;;) {
          c[0] = self->line[self->pos + i];
          c[1] = '\0';
          if (c[0] == '\0') break;
          if (c[0] != ' ' && c[0] != '\t' && c[0] != '\n' && c[0] != '\r' &&
              c[0] != self->modeTerminater) {
            strsafecat(value, c, MAX_TOKEN_LENGTH);
            i++;
          } else {
            break;
          }
        }
        type = STRING;
      }
      if (strlen(value) > 0) {
        tokenizer_pushToken(self,
          self->line_num,
          self->pos,
          type,
          value,
          EXPR_BEG);
        self->pos += strlen(value);
      }
    }
    self->pos--;
  } else if (self->mode == MODE_TSTRING_DOUBLE) {
    bool string_top = true;
    for (;;) {
      DEBUGP("modeTerminater: `%c`", self->modeTerminater);
      tokenizer_readLine(self);
      if (self->line[0] == '\0') {
        Token_free(lazyToken);
        return -1;
      }
      if (self->line[self->pos] == self->modeTerminater) {
        lazyToken->line_num = self->line_num;
        lazyToken->pos = self->pos;
        lazyToken->type = STRING_END;
        lazyToken->value = (char *)picorbc_alloc(sizeof(char) *(2));
        *(lazyToken->value) = self->modeTerminater;
        *(lazyToken->value + 1) = '\0';;
        lazyToken->state = EXPR_END;
        p->state = EXPR_END;
        self->pos++;
        self->mode = MODE_NONE;
        break;
      } else if (self->line[self->pos] == '#' && self->line[self->pos + 1] == '{') {
        tokenizer_pushToken(self,
          self->line_num,
          self->pos,
          (string_top ? DSTRING_TOP : DSTRING_MID),
          value,
          EXPR_BEG);
        string_top = false;
        value[0] = '\0';
        c[0] = '\0';
        tokenizer_pushToken(self,
          self->line_num,
          self->pos,
          DSTRING_BEG,
          (char *)"#{",
          EXPR_BEG);
        self->pos += 2;
        paren_stack_add(p, PAREN_BRACE);
        /**
         * Reserve some properties in order to revert them
         * after recursive calling
         */
        {
          Mode reserveMode = self->mode;
          State reserveState = p->state;
          char reserveModeTerminater = self->modeTerminater;
          bool reserveCmd_start = p->cmd_start;
          self->mode = MODE_NONE;
          p->state = EXPR_BEG;
          self->modeTerminater = '\0';
          p->cmd_start = true;
          while (Tokenizer_hasMoreTokens(self)) { /* recursive */
            if (Tokenizer_advance(self, p, false) == 1) break;
          }
          self->mode = reserveMode;
          p->state = reserveState;
          self->modeTerminater = reserveModeTerminater;
          p->cmd_start = reserveCmd_start;
        }
        tokenizer_pushToken(self,
          self->line_num,
          self->pos,
          DSTRING_END,
          (char *)"}",
          EXPR_END);
        self->pos++;
      } else if (self->line[self->pos] == '\\') {
        self->pos++;
        if (self->line[self->pos] == self->modeTerminater) {
          c[0] = self->modeTerminater;
        } else if (Regex_match3(&(self->line[self->pos]), "^([0-7]+)", regexResult)) {
          /*
           * An octal number
           */
          regexResult[0].value[3] = '\0'; /* maximum 3 digits */
          c[0] = strtol(regexResult[0].value, NULL, 8) % 0x100;
          if (c[0] == 0) REPLACE_NULL_PICORUBY;
          self->pos += strlen(regexResult[0].value) - 1;
        } else {
          switch (self->line[self->pos]) {
            case 'a':  c[0] =  7; break;
            case 'b':  c[0] =  8; break;
            case 't':  c[0] =  9; break;
            case 'n':  c[0] = 10; break;
            case 'v':  c[0] = 11; break;
            case 'f':  c[0] = 12; break;
            case 'r':  c[0] = 13; break;
            case 'e':  c[0] = 27; break;
            case '\\': c[0] = 92; break;
            case 'x':
              /*
               * Should ba a hex number
               */
              self->pos++;
              if (Regex_match3(&(self->line[self->pos]), "^([0-9a-fA-F]+)", regexResult)) {
                regexResult[0].value[2] = '\0'; /* maximum 2 digits */
                if (regexResult[0].value[1] != '\0') self->pos++;
                c[0] = strtol(regexResult[0].value, NULL, 16);
                if (c[0] == '\0') REPLACE_NULL_PICORUBY;
              } else {
                ERRORP("Invalid hex escape");
                return 1;
              }
              break;
            default: c[0] = self->line[self->pos];
          }
        }
      } else {
        c[0] = self->line[self->pos];
      }
      self->pos += strlen(c);
      strsafecat(value, c, MAX_TOKEN_LENGTH);
    }
    self->pos--;
    if (strlen(value) > 0) {
      type = STRING;
    }
  } else if (self->mode == MODE_TSTRING_SINGLE) {
    for (;;) {
      tokenizer_readLine(self);
      if (self->line[0] == '\0') {
        Token_free(lazyToken);
        return -1;
      }
      else if (self->line[self->pos] == '\\' && 
               self->line[self->pos+1] == self->modeTerminater) {
        self->pos++;
        c[0] = self->modeTerminater;
      }
      else if (self->line[self->pos] == self->modeTerminater) {
        lazyToken->line_num = self->line_num;
        lazyToken->pos = self->pos;
        lazyToken->type = STRING_END;
        lazyToken->value = (char *)picorbc_alloc(sizeof(char) *(2));
        *(lazyToken->value) = self->modeTerminater;
        *(lazyToken->value + 1) = '\0';
        lazyToken->state = EXPR_END;
        p->state = EXPR_END;
        self->pos++;
        self->mode = MODE_NONE;
        break;
      } else {
        c[0] = self->line[self->pos];
      }
      self->pos += strlen(c);
      strsafecat(value, c, MAX_TOKEN_LENGTH);
    }
    self->pos--;
    if (strlen(value) > 0) type = STRING;
  } else if (Regex_match2(self->line, "^=begin$") || Regex_match2(self->line, "^=begin\\s")) { // multi lines comment began
    self->mode = MODE_COMMENT;
    strsafecpy(value, strsafecat(self->line, "\n", MAX_TOKEN_LENGTH), MAX_TOKEN_LENGTH);
    type = EMBDOC_BEG;
  } else if (self->line[self->pos] == '\n') {
    value[0] = '\\';
    value[1] = 'n';
    value[2] = '\0';
    type = NL;
  } else if (self->line[self->pos] == '\r' && self->line[self->pos + 1] == '\n') {
    value[0] = '\\';
    value[1] = 'r';
    value[2] = '\\';
    value[3] = 'n';
    value[4] = '\0';
    type = NL;
  } else if (p->state != EXPR_DOT && p->state != EXPR_FNAME && tokenizer_is_operator(&(self->line[self->pos]), 3)) {
    value[0] = self->line[self->pos];
    value[1] = self->line[self->pos + 1];
    value[2] = self->line[self->pos + 2];
    value[3] = '\0';
    if (strcmp(value, "===") == 0) {
      type = EQQ;
    } else if (strcmp(value, "<=>") == 0) {
      type = CMP;
    } else if (value[2] == '=') {
      type = OP_ASGN;
    } else if (value[2] == '.') {
      if (IS_BEG()) {
        type = BDOT3;
      } else {
        type = DOT3;
      }
    }
    p->state = EXPR_BEG;
  } else if (p->state != EXPR_DOT && p->state != EXPR_FNAME && tokenizer_is_operator(&(self->line[self->pos]), 2)) {
    value[0] = self->line[self->pos];
    value[1] = self->line[self->pos + 1];
    value[2] = '\0';
    switch (value[0]) {
      case '=':
        switch (value[1]) {
          case '=': type = EQ; break;
          case '>': type = ASSOC; break;
          case '~': type = MATCH; break;
        }
        p->state = EXPR_BEG;
        break;
      case '!':
        switch (value[1]) {
          case '=': type = NEQ; break;
          case '~': type = NMATCH; break;
        }
        p->state = EXPR_BEG;
        break;
      case '*':
        switch (value[1]) {
          case '*':
            if (IS_BEG()) {
              type = DSTAR;
            } else {
              type = POW;
            }
            break;
          case '=': type = OP_ASGN; break;
        }
        p->state = EXPR_BEG;
        break;
      case '<':
        switch (value[1]) {
          case '<': type = LSHIFT; break;
          case '=': type = LEQ; break;
        }
        if (p->state == EXPR_FNAME || p->state == EXPR_DOT) {
          p->state = EXPR_ARG;
        } else {
          p->state = EXPR_BEG;
        }
        break;
      case '>':
        switch (value[1]) {
          case '>': type = RSHIFT; break;
          case '=': type = GEQ; p->state = EXPR_ARG; break;
        }
        break;
      case '+':
      case '-':
      case '/':
      case '^':
      case '%':
        type = OP_ASGN;
        p->state = EXPR_BEG;
        break;
      case '&':
        if (value[1] == '&') {
          type = ANDOP;
        } else {
          type = OP_ASGN;
        }
        p->state = EXPR_BEG;
        break;
      case '|':
        if (value[1] == '|') {
          type = OROP;
        } else {
          type = OP_ASGN;
        }
        p->state = EXPR_BEG;
        break;
      case '.':
        if (IS_BEG()) {
          type = BDOT2;
        } else {
          type = DOT2;
        }
        p->state = EXPR_BEG;
        break;
      case ':':
        if (IS_BEG() || p->state == EXPR_CLASS) { // || IS_SPCARG(-1)) {
          p->state = EXPR_BEG;
          type = COLON3;
        } else {
          p->state = EXPR_COLON2;
          type = COLON2;
        }
        break;
      default:
        FATALP("error");
        break;
    }
  } else if (Regex_match3(&(self->line[self->pos]), "^(@\\w+)", regexResult)) {
    strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
    type = IVAR;
    p->state = EXPR_END;
  } else if (Regex_match3(&(self->line[self->pos]), "^(\\$\\w+)", regexResult)) {
    strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
    type = GVAR;
    p->state = EXPR_END;
  } else if (self->line[self->pos] == '?') {
    char c1 = self->line[self->pos + 1];
    char c2 = self->line[self->pos + 2];
    if ( (IS_BEG() || IS_ARG()) && c1 != ' ' &&
        (c2 == ' ' || c2 == '\n' || c2 == '\r' || c2 == '\t' || c2 == ';' || c2 == '\0') ) {
      value[0] = c1;
      type = STRING;
      p->state = EXPR_END;
      self->pos++;
    } else {
      value[0] = '?';
      type = QUESTION;
      p->state = EXPR_VALUE;
    }
    value[1] = '\0';
  } else if (self->line[self->pos] == '-' && self->line[self->pos + 1] == '>') {
    value[0] = '-';
    value[1] = '>';
    value[2] = '\0';
    p->state = EXPR_ENDFN;
    type = LAMBDA;
  } else {
    if (self->line[self->pos] == '\\') {
      // ignore
      self->pos++;
    } else if (self->line[self->pos] == ':') {
      value[0] = ':';
      value[1] = '\0';
      if (p->state == EXPR_LABEL) {
        type = LABEL_TAG;
        p->state = EXPR_BEG;
      } else if (Regex_match2(&(self->line[self->pos]), "^:[_A-Za-z0-9'\"]")) {
        type = SYMBEG;
        if (self->line[self->pos + 1] == '\'' || self->line[self->pos + 1] == '"') {
          p->state = EXPR_CMDARG;
        } else {
          p->state = EXPR_FNAME;
        }
      } else {
        type = COLON;
        p->state = EXPR_BEG;
      }
    } else if (self->line[self->pos] == '#') {
      strsafecpy(value, &(self->line[self->pos]), MAX_TOKEN_LENGTH);
      if (value[strlen(value) - 1] == '\n') value[strlen(value) - 1] = '\0'; /* eliminate \n */
      type = COMMENT;
    } else if (self->line[self->pos] == ' ' || self->line[self->pos] == '\t') {
      Regex_match3(&(self->line[self->pos]), "^([ \t]+)", regexResult);
      strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
      type = ON_SP;
    } else if (tokenizer_is_semicolon(self->line[self->pos])) {
      value[0] = self->line[self->pos];
      type = SEMICOLON;
      p->state = EXPR_BEG;
    } else if (p->state == EXPR_FNAME || p->state == EXPR_DOT) {
      if ( Regex_match3(&(self->line[self->pos]), "^([A-Z]\\w*)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = CONSTANT;
      } else if (self->line[self->pos] == '(') {
        value[0] = '(';
        value[1] = '\0';
        type = LPAREN_EXPR;
        p->state = EXPR_BEG;
      } else {
        if ( Regex_match3(&(self->line[self->pos]), "^(\\w+[!?=]?)", regexResult) ) {
          if (keyword(regexResult[0].value)) {
            if (!strncmp(regexResult[0].value, "self", sizeof("self") + 1)) {
            } else {
              ignore_keyword = true;
            }
          }
          strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        } else if ( Regex_match3(&(self->line[self->pos]), "^(\\[\\]=?)", regexResult) ) {
          strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        } else if ( Regex_match3(&(self->line[self->pos]), "^([\\-+]@?)", regexResult) ) {
          strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        } else if ( Regex_match3(&(self->line[self->pos]), "^(\\*\\*?)", regexResult) ) {
          strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        } else if ( Regex_match3(&(self->line[self->pos]), "^([`~/%|&])", regexResult) ) {
          strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        } else {
          value[1] = '\0';
          value[2] = '\0';
          value[3] = '\0';
          if (self->line[self->pos] == '!') {
            value[0] = '!';
            if (self->line[self->pos + 1] == '=') value[1] = '=';
            else if (self->line[self->pos + 1] == '~') value[1] = '~';
          } else if (self->line[self->pos] == '=') {
            value[0] = '=';
            if (self->line[self->pos + 1] == '~') value[1] = '~';
            else if (self->line[self->pos + 1] == '=') {
              value[1] = '=';
              if (self->line[self->pos + 2] == '=') value[2] = '=';
            }
          } else if (self->line[self->pos] == '<') {
            value[0] = '<';
            if (self->line[self->pos + 1] == '<') {
              value[1] = '<';
            } else if (self->line[self->pos + 1] == '=') {
              value[1] = '=';
              if (self->line[self->pos + 2] == '>') value[2] = '>';
            }
          } else if (self->line[self->pos] == '>') {
            value[0] = '>';
            if (self->line[self->pos + 1] == '>') {
              value[1] = '>';
            } else if (self->line[self->pos + 1] == '=') {
              value[1] = '=';
            }
          } else {
            ERRORP("Invalid method name!");
            Token_free(lazyToken);
            self->pos += 1; /* skip one */
            return 1;
          }
        }
        type = IDENTIFIER;
      }
      p->state = EXPR_ENDFN;
    } else if (tokenizer_is_paren(self->line[self->pos])) {
      value[0] = self->line[self->pos];
      value[1] = '\0';
      switch (value[0]) {
        case '(':
        case '[':
        case '{':
          COND_PUSH(0);
          CMDARG_PUSH(0);
          break;
        default: /* must be ) ] } */
          COND_LEXPOP();
          CMDARG_LEXPOP();
          break;
      }
      switch (value[0]) {
        case '(':
          if (IS_BEG()) {
            type = LPAREN;
          } else if (IS_ARG() && self->line[self->pos - 1] == ' ') {
            type = LPAREN_ARG;
          } else {
            type = LPAREN_EXPR;
          }
          p->state = EXPR_BEG;
          COND_PUSH(0);
          CMDARG_PUSH(0);
          paren_stack_add(p, PAREN_PAREN);
          break;
        case ')':
          type = RPAREN;
          p->state = EXPR_ENDFN;
          paren_stack_pop(p);
          COND_LEXPOP();
          CMDARG_LEXPOP();
          break;
        case '[':
          if ( IS_BEG() || (IS_ARG() && self->line[self->pos - 1] == ' ') ) {
            type = LBRACKET_ARRAY;
          } else {
            type = LBRACKET;
          }
          p->state = EXPR_BEG|EXPR_LABEL;
          COND_PUSH(0);
          CMDARG_PUSH(0);
          break;
        case ']':
          type = RBRACKET;
          p->state = EXPR_END;
          COND_LEXPOP();
          CMDARG_LEXPOP();
          break;
        case '{':
          if (p->lpar_beg && p->lpar_beg == p->paren_stack_num) {
            p->lpar_beg = 0;
            p->paren_stack_num--;
            type = LAMBEG;
          } else if (IS_ARG() || p->state == EXPR_END || p->state == EXPR_ENDFN) {
            type = LBRACE_BLOCK_PRIMARY; /* block (primary) */
          } else if (p->state == EXPR_ENDARG) {
            type = LBRACE_ARG;  /* block (expr) */
          } else {
            type = LBRACE; /* hash */
          }
          COND_PUSH(0);
          CMDARG_PUSH(0);
          p->state = EXPR_BEG;
          break;
        case '}':
          if (p->paren_stack[p->paren_stack_num] == PAREN_BRACE) {
            paren_stack_pop(p);
            Token_free(lazyToken);
            return 1;
          }
          type = RBRACE;
          p->state = EXPR_END;
          COND_LEXPOP();
          CMDARG_LEXPOP();
          break;
        default:
          ERRORP("unknown paren error");
      }
    } else if (strchr("+-*/%&|^!~><=?:", self->line[self->pos]) != NULL) {
      if (Regex_match3(&(self->line[self->pos]), "^(%[iIwWqQ][]-~!@#$%^&*()_=+\[{};:'\"?/])", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        switch (value[1]) {
          case 'w':
          case 'W':
            type = WORDS_BEG;
            self->mode = MODE_WORDS;
            break;
          case 'q':
            type = STRING_BEG;
            self->mode = MODE_TSTRING_SINGLE;
            break;
          case 'Q':
            type = STRING_BEG;
            self->mode = MODE_TSTRING_DOUBLE;
            break;
          case 'i':
          case 'I':
            type = SYMBOLS_BEG;
            self->mode = MODE_SYMBOLS;
            break;
        }
        switch (value[2]) {
          case '[':
            self->modeTerminater = ']';
            break;
          case '{':
            self->modeTerminater = '}';
            break;
          case '(':
            self->modeTerminater = ')';
            break;
          default:
            self->modeTerminater = value[2];
        }
      } else {
        value[0] = self->line[self->pos];
        value[1] = '\0';
        if (value[0] == '-' && (IS_BEG() || (IS_ARG() && self->line[self->pos - 1] == ' ')) ) {
          if (IS_NUM(1)) {
            type = UMINUS_NUM;
          } else {
            if (self->line[self->pos + 1] == ' ') {
              type = MINUS;
            } else {
              type = UMINUS;
            }
          }
        } else if (IS_BEG() && value[0] == '+') {
          type = UPLUS;
        } else {
          switch (value[0]) {
            case '=':
              type = E;
              break;
            case '>':
              type = GT;
              break;
            case '<':
              type = LT;
              break;
            case '+':
              type = PLUS;
              break;
            case '-':
              type = MINUS;
              break;
            case '*':
              if (IS_BEG()) {
                type = STAR;
              } else {
                type = TIMES;
              }
              break;
            case '/':
              type = DIVIDE;
              break;
            case '%':
              type = SURPLUS;
              break;
            case '|':
              type = OR;
              break;
            case '^':
              type = XOR;
              break;
            case '&':
              if (self->line[self->pos + 1] == '.') {
                value[1] = '.';
                value[2] = '\0';
                type = ANDDOT;
                p->state = EXPR_DOT;
              } else if (self->line[self->pos + 1] != ' ') {
                //yywarning(p, "'&' interpreted as argument prefix");
                type = AMPER;
              } else if (IS_BEG()) {
                type = AMPER;
              } else {
                type = AND;
              }
              break;
            case '!':
              type = UNEG;
              break;
            case '~':
              type = UNOT;
              break;
            default:
              type = ON_OP;
          }
        }
        p->state = EXPR_BEG;
      }
    } else if (tokenizer_is_comma(self->line[self->pos])) {
      value[0] = self->line[self->pos];
      type = COMMA;
      p->state = EXPR_BEG|EXPR_LABEL;
    } else if (IS_NUM(0)) {
      p->state = EXPR_ENDARG;
      if (Regex_match3(&(self->line[self->pos]), "^(0[xX][0-9a-fA-F][0-9a-fA-F_]*)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = INTEGER;
      } else if (Regex_match3(&(self->line[self->pos]), "^(0[oO][0-7][0-7_]*)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = INTEGER;
      } else if (Regex_match3(&(self->line[self->pos]), "^(0[bB][01][01_]*)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = INTEGER;
      } else if (Regex_match3(&(self->line[self->pos]), "^([0-9_]+\\.[0-9][0-9_]*)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        if (strchr(value, (int)'.')[-1] == '_') {
          ERRORP("trailing `_' in number");
          return -1;
        }
        type = FLOAT;
        if (Regex_match3(&(self->line[self->pos + strlen(value)]), "^([eE][+\\-]?[0-9_]+)", regexResult)) {
          strsafecpy(value + strlen(value), regexResult[0].value, MAX_TOKEN_LENGTH);
        }
      } else if (Regex_match3(&(self->line[self->pos]), "^([0-9_]+)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        if (value[0] == '0' && Regex_match2(value, "[8-9]")) {
          ERRORP("Invalid octal digit");
          return 1;
        }
        if (Regex_match3(&(self->line[self->pos + strlen(value)]), "^([eE][+\\-]?[0-9_]+)", regexResult)) {
          strsafecpy(value + strlen(value), regexResult[0].value, MAX_TOKEN_LENGTH);
          type = FLOAT;
        } else {
          type = INTEGER;
        }
      } else {
        ERRORP("Failed to tokenize a number");
        return -1;
      }
      if (value[strlen(value) - 1] == '_') {
        ERRORP("trailing `_' in number");
        return -1;
      }
    } else if (self->line[self->pos] == '.') {
      value[0] = '.';
      value[1] = '\0';
      type = PERIOD;
      p->state = EXPR_DOT;
    } else if (Regex_match2(&(self->line[self->pos]), "^\\w")) {
      if (Regex_match3(&(self->line[self->pos]), "^([A-Z][A-Za-z0-9_]*::)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        value[strlen(value) - 2] = '\0';
        type = CONSTANT;
        p->state = EXPR_CMDARG;
      } else if (Regex_match3(&(self->line[self->pos]), "^([A-Za-z0-9_?!]+:)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        value[strlen(value) - 1] = '\0';
        type = LABEL;
        p->state = EXPR_LABEL;
      } else if (Regex_match3(&(self->line[self->pos]), "^([A-Z]\\w*[!?])", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = IDENTIFIER;
      } else if (Regex_match3(&(self->line[self->pos]), "^([A-Z]\\w*)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = CONSTANT;
        p->state = EXPR_CMDARG;
      } else if (Regex_match3(&(self->line[self->pos]), "^(\\w+[!?]?)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = IDENTIFIER;
      } else {
        ERRORP("Failed to tokenize!");
        Token_free(lazyToken);
        return 1;
      }
    } else if (self->line[self->pos] == '"') {
      value[0] = '"';
      value[1] = '\0';
      self->mode = MODE_TSTRING_DOUBLE;
      self->modeTerminater = '"';
      type = STRING_BEG;
    } else if (self->line[self->pos] == '\'') {
      value[0] = '\'';
      value[1] = '\0';
      self->mode = MODE_TSTRING_SINGLE;
      self->modeTerminater = '\'';
      type = STRING_BEG;
    } else {
      ERRORP("Failed to tokenize!");
      Token_free(lazyToken);
      self->pos += 1; /* skip one */
      return 1;
    }
  }
  if (lazyToken->value == NULL) {
    self->pos += strlen(value);
  }
  if (type == NL) {
    switch ((int)p->state) {
      /*     ^^^
       * Casting to int can suprress `Warning: case not evaluated in enumerated type`
       */
      case EXPR_BEG:
      case EXPR_FNAME:
      case EXPR_DOT:
      case EXPR_CLASS:
      case EXPR_BEG|EXPR_LABEL:
        goto retry;
      default:
        break;
    }
    p->state = EXPR_BEG;
  }
  if (type != ON_NONE) {
    /* FIXME from here */
    int8_t kw_num = 0;
    if (type != STRING && !ignore_keyword) kw_num = keyword(value);
    if ( kw_num > 0 ) {
      type = (uint8_t)kw_num;
      switch (type) {
        case KW_class:
        case KW_module:
          if (p->state == EXPR_BEG) {
            p->state = EXPR_CLASS;
          } else {
            type = IDENTIFIER;
            p->state = EXPR_ARG;
          }
          break;
        case KW_rescue:
          if (!IS_BEG())
            type = KW_modifier_rescue;
          p->state = EXPR_MID;
          break;
        case KW_if:
          if (p->state != EXPR_BEG && p->state != EXPR_VALUE)
            type = KW_modifier_if;
          p->state = EXPR_VALUE;
          break;
        case KW_unless:
          if (p->state != EXPR_BEG && p->state != EXPR_VALUE)
            type = KW_modifier_unless;
          p->state = EXPR_VALUE;
          break;
        case KW_while:
          if (p->state != EXPR_BEG && p->state != EXPR_VALUE)
            type = KW_modifier_while;
          p->state = EXPR_VALUE;
          break;
        case KW_do:
          if (p->lpar_beg && p->lpar_beg == p->paren_stack_num) {
            p->lpar_beg = 0;
            p->paren_stack_num--;
            type = KW_do_lambda;
          } else if (COND_P()) {
            type = KW_do_cond;
          } else if ((CMDARG_P() && p->state != EXPR_CMDARG)
                     || p->state == EXPR_ENDARG || p->state == EXPR_BEG){
            type = KW_do_block;
          }
        case KW_elsif:
        case KW_else:
        case KW_and:
        case KW_or:
        case KW_case:
        case KW_when:
          p->state = EXPR_BEG;
          break;
        case KW_return:
        case KW_break:
        case KW_next:
          p->state = EXPR_MID;
          break;
        case KW_def:
        case KW_alias:
//        case KW_undef:
          p->state = EXPR_FNAME;
          break;
//        case KW_super:
        case KW_end:
        case KW_redo:
        default:
          p->state = EXPR_END;
      }
    } else if (type == IDENTIFIER) {
      switch (p->state) {
        case EXPR_CLASS:
          p->state = EXPR_ARG;
          break;
        case EXPR_FNAME:
          p->state = EXPR_ENDFN;
          break;
        case EXPR_ENDFN:
          break;
        default:
          if (IS_BEG() || p->state == EXPR_DOT || IS_ARG()) {
            if (cmd_state) {
              if (self->line[self->pos] == ' ' || self->line[self->pos] == '\t') {
                p->state = EXPR_BEG;
              } else {
                p->state = EXPR_CMDARG;
              }
            }
            else {
              p->state = EXPR_ARG;
            }
          } else if (p->state == EXPR_FNAME) {
            p->state = EXPR_ENDFN;
          } else {
            p->state = EXPR_END;
          }
          break;
      }
    }
    DEBUGP("value len: %ld, `%s`", strlen(value), value);
    tokenizer_pushToken(self,
      self->line_num,
      self->pos - strlen(value),
        /* FIXME: ^^^^^^^^^^^^^ incorrect if value contains escape sequences */
      type,
      value,
      p->state);
  }
  if (lazyToken->value != NULL) {
    tokenizer_pushToken(self,
      lazyToken->line_num,
      lazyToken->pos,
      lazyToken->type,
      lazyToken->value,
      lazyToken->state);
    self->pos++;
  }
  Token_free(lazyToken);
  return 0;
}

