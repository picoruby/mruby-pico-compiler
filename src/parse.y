%fallback
  ON_NONE
  KW
  ON_SP
  SEMICOLON
  PERIOD
  ON_OP
  ON_TSTRING_SINGLE
  EMBDOC
  EMBDOC_BEG
  EMBDOC_END
  DSTRING_BEG
  DSTRING_END
  STRING_END
  IVAR
  GVAR
  CHAR
  SYMBEG
  COMMENT
  LPAREN
  RPAREN
  LBRACKET
  RBRACKET
  LBRACE
  RBRACE
  WORDS_BEG
  WORDS_SEP
  SYMBOLS_BEG
  LABEL
  FLOAT
  .

%extra_context { ParserState *p }

%token_type { const char* }
//%token_destructor { LEMON_FREE($$); }

%default_type { Node* }
//%default_destructor { LEMON_FREE($$); }

%type call_op       { uint8_t }
%type cname         { const char* }
%type fname         { const char* }
%type sym           { const char* }
%type fsym          { const char* }
%type stmt_alias    { const char* }
%type basic_symbol  { const char* }
%type operation2    { const char* }
%type f_norm_arg    { const char* }
%type f_block_arg   { const char* }
%type opt_f_block_arg { const char* }
%type f_label       { const char* }
%type lambda_head           { unsigned int }
%type preserve_cmdarg_stack { unsigned int }
%type push_cmdarg           { unsigned int }
%type preserve_in_single    { unsigned int }
%type preserve_in_def       { unsigned int }

%include {
  #include <stdlib.h>
  #include <stdint.h>
  #include <string.h>
  #include <parse_header.h>
  #include "parse.h" /* Intentional */
  #include <atom_helper.h>
  #include <scope.h>
  #include <node.h>
  #include <token.h>
  #include <debug.h>
}

%ifdef LEMON_PICORBC
  %include {
    #ifdef MRBC_ALLOC_LIBC
      #define LEMON_ALLOC(size) malloc(size)
      #define LEMON_FREE(ptr)   free(ptr)
    #else
      void *picorbc_alloc(size_t size);
      void picorbc_free(void *ptr);
      #define LEMON_ALLOC(size) picorbc_alloc(size)
      #define LEMON_FREE(ptr)   picorbc_free(ptr)
    #endif /* MRBC_ALLOC_LIBC */
  }
%endif

%ifndef LEMON_PICORBC
  %include {
    #define LEMON_ALLOC(size) malloc(size)
    #define LEMON_FREE(ptr)   free(ptr)
  }
%endif

%include {
  #include <parse_header.h>

  #define STRING_NULL (p->special_string_pool.null)
  #define STRING_NEG  (p->special_string_pool.neg)
  #define STRING_ARY  (p->special_string_pool.ary)

  static Node*
  cons_gen(ParserState *p, Node *car, Node *cdr)
  {
    Node *c;
    c = Node_new(p);
    c->type = CONS;
    c->cons.car = car;
    c->cons.cdr = cdr;
    return c;
  }
  #define cons(a,b) cons_gen(p,(a),(b))

  static Node*
  atom_gen(ParserState *p, int t)
  {
    Node* a;
    a = Node_new(p);
    a->type = ATOM;
    a->atom.type = t;
    return a;
  }
  #define atom(t) atom_gen(p, (t))

  static Node*
  literal_gen(ParserState *p, const char *s)
  {
    Node* l;
    l = Node_new(p);
    Node_setValue(l, s);
    return l;
  }
  #define literal(s) literal_gen(p, (s))

  static Node*
  list1_gen(ParserState *p, Node *a)
  {
    return cons(a, 0);
  }
  #define list1(a) list1_gen(p, (a))

  static Node*
  list2_gen(ParserState *p, Node *a, Node *b)
  {
    return cons(a, cons(b,0));
  }
  #define list2(a,b) list2_gen(p, (a),(b))

  static Node*
  list3_gen(ParserState *p, Node *a, Node *b, Node *c)
  {
    return cons(a, cons(b, cons(c,0)));
  }
  #define list3(a,b,c) list3_gen(p, (a),(b),(c))

  static Node*
  list4_gen(ParserState *p, Node *a, Node *b, Node *c, Node *d)
  {
    return cons(a, cons(b, cons(c, cons(d, 0))));
  }
  #define list4(a,b,c,d) list4_gen(p, (a),(b),(c),(d))

  static Node*
  list5_gen(ParserState *p, Node *a, Node *b, Node *c, Node *d, Node *e)
  {
    return cons(a, cons(b, cons(c, cons(d, cons(e, 0)))));
  }
  #define list5(a,b,c,d,e) list5_gen(p, (a),(b),(c),(d),(e))

  static Node*
  list6_gen(ParserState *p, Node *a, Node *b, Node *c, Node *d, Node *e, Node *f)
  {
    return cons(a, cons(b, cons(c, cons(d, cons(e, cons(f, 0))))));
  }
  #define list6(a,b,c,d,e,f) list6_gen(p, (a),(b),(c),(d),(e),(f))

  static Node*
  append_gen(ParserState *p, Node *a, Node *b)
  {
    Node *c = a;
    if (!a) return b;
    if (!b) return a;
    while (c->cons.cdr) {
      c = c->cons.cdr;
    }
    c->cons.cdr = b;
    return a;
  }
  #define append(a,b) append_gen(p,(a),(b))
  #define push(a,b) append_gen(p,(a),list1(b))

  static Node*
  new_return(ParserState *p, Node *array)
  {
    return list2(atom(ATOM_kw_return), array);
  }

  static Node*
  new_yield(ParserState *p, Node *c)
  {
    return list2(atom(ATOM_kw_yield), c);
  }

  /* (:sym) */
  static Node*
  new_sym(ParserState *p, const char *s)
  {
    return list2(atom(ATOM_symbol_literal), literal(s));
  }

  static Node*
  new_dsym(ParserState *p, Node *n)
  {
    return list2(atom(ATOM_dsymbol), n);
  }

  /* (:call a b c) */
  static Node*
  new_call(ParserState *p, Node *a, const char* b, Node *c, int pass)
  {
    return list4(
      atom(pass ? ATOM_call : ATOM_scall),
      a,
      list2(atom(ATOM_at_ident), literal(b)),
      c
    );
  }

  static Node*
  new_super(ParserState *p, Node *node)
  {
    return list2(atom(ATOM_super), node);
  }

  static Node*
  new_zsuper(ParserState *p)
  {
    return list2(atom(ATOM_zsuper), 0);
  }

  static Node*
  new_lambda(ParserState *p, Node *a, Node *b)
  {
    return list3(atom(ATOM_lambda), a, b);
  }

  /* (:begin prog...) */
  static Node*
  new_begin(ParserState *p, Node *body)
  {
    if (body) {
      return list3(atom(ATOM_stmts_add), list1(atom(ATOM_stmts_new)), body);
    }
    return cons(atom(ATOM_stmts_new), 0);//TODO ここおかしい
  }

  static Node*
  new_first_arg(ParserState *p, Node *arg)
  {
    return list2(atom(ATOM_args_add), list2(atom(ATOM_args_new), arg));
  }

  static Node*
  new_splat(ParserState *p, Node *a)
  {
    return list2(atom(ATOM_splat), a);
  }

  static Node*
  call_bin_op_gen(ParserState *p, Node *recv, const char *op, Node *arg)
  {
    return new_call(p, recv, op, new_first_arg(p, arg), 1);
  }
  #define call_bin_op(a, m, b) call_bin_op_gen(p ,(a), (m), (b))

  static Node*
  call_uni_op(ParserState *p, Node *recv, const char *op)
  {
    return new_call(p, recv, op, 0, 1);
  }

    /* (:int . i) */
  static Node*
  new_lit(ParserState *p, const char *s, AtomType a)
  {
    Node* result = list2(atom(a), literal(s));
    return result;
  }

  const char *ParsePushStringPool(ParserState *p, char *s);

  /* nageted integer */
  static Node*
  new_neglit(ParserState *p, const char *s, AtomType a)
  {
    const char *const_neg = ParsePushStringPool(p, STRING_NEG);
    return list3(atom(ATOM_unary), literal(const_neg) ,list2(atom(a), literal(s)));
  }

  static Node*
  new_lvar(ParserState *p, const char *s)
  {
    return list2(atom(ATOM_lvar), literal(s));
  }

  static Node*
  new_ivar(ParserState *p, const char *s)
  {
    return list2(atom(ATOM_at_ivar), literal(s));
  }

  static Node*
  new_gvar(ParserState *p, const char *s)
  {
    return list2(atom(ATOM_at_gvar), literal(s));
  }

  static Node*
  new_const(ParserState *p, const char *s)
  {
    return list2(atom(ATOM_at_const), literal(s));
  }

  static void
  generate_lvar(Scope *scope, Node *n)
  {
    if (Node_atomType(n) == ATOM_lvar) {
      LvarScopeReg lvar = Scope_lvar_findRegnum(scope, Node_literalName(n->cons.cdr));
      if (lvar.reg_num == 0) {
        Scope_newLvar(scope, Node_literalName(n->cons.cdr), scope->sp);
        Scope_push(scope);
      }
    }
  }

  static Node*
  new_array(ParserState *p, Node *a)
  {
    return list2(atom(ATOM_array), a);
  }

  static Node*
  new_masgn(ParserState *p, Node *mlhs, Node *mrhs)
  {
    return list3(
      atom(ATOM_masgn),
      cons(atom(ATOM_mlhs), mlhs),
      list2(atom(ATOM_mrhs), mrhs)
    );
  }

  static Node*
  new_asgn(ParserState *p, Node *lhs, Node *rhs)
  {
    generate_lvar(p->scope, lhs);
    return list3(atom(ATOM_assign), lhs, rhs);
  }

  static Node*
  new_op_asgn(ParserState *p, Node *lhs, const char *op, Node *rhs)
  {
    generate_lvar(p->scope, lhs);
    return list4(atom(ATOM_op_assign), lhs, list2(atom(ATOM_at_op), literal(op)), rhs);
  }

  /* (:self) */
  static Node*
  new_self(ParserState *p)
  {
    return list1(atom(ATOM_kw_self));
  }

  static Node*
  new_nil(ParserState *p)
  {
    return list1(atom(ATOM_kw_nil));
  }

  /* (:fcall self mid args) */
  static Node*
  new_fcall(ParserState *p, Node *b, Node *c)
  {
    return list3(atom(ATOM_fcall), b, c);
  }

  static Node*
  new_callargs(ParserState *p, Node *args, Node *assocs, Node *block)
  {
    if (Node_atomType(args) == ATOM_NONE) {
      args = new_first_arg(p, args);
    }
    return push(push(args, assocs), block);
  }

  static Node*
  var_reference(ParserState *p, Node *lhs)
  {
    if (Node_atomType(lhs) == ATOM_lvar) {
      LvarScopeReg lvar = Scope_lvar_findRegnum(p->scope, lhs->cons.cdr->cons.car->value.name);
      if (lvar.reg_num == 0) {
        return new_fcall(p, lhs, 0);
      }
    }
    //Scope_newLvar(p->scope, lhs->cons.cdr->cons.car->value.name, p->scope->sp++);
    return lhs;
  }

  /* (:block_arg . a) */
  static Node*
  new_block_arg(ParserState *p, Node *a)
  {
    return list2(atom(ATOM_block_arg), a);
  }

//  static Node*
//  setup_numparams(ParserState *p, Node *a)
//  {
//  }

  static Node*
  new_block(ParserState *p, Node *a, Node *b)
  {
    //a = setup_numparams(p, a);
    return list3(atom(ATOM_block), a, b);
  }

  static void
  args_with_block(ParserState *p, Node *a, Node *b)
  {
    if (!b) return;
    Node *args_add;
    if (Node_atomType(a->cons.cdr->cons.cdr->cons.car) == ATOM_args_add) {
      /* fcall */
      args_add = a->cons.cdr->cons.cdr->cons.car;
    } else if (Node_atomType(a->cons.cdr->cons.cdr->cons.cdr->cons.car) == ATOM_args_add) {
      /* call */
      args_add = a->cons.cdr->cons.cdr->cons.cdr->cons.car;
    } else {
      FATALP("This should not happen");
      return;
    }
    if (Node_atomType(args_add->cons.cdr->cons.cdr->cons.cdr->cons.car) == ATOM_block_arg) {
      //yyerror(p, "both block arg and actual block given");
      ERRORP("both block arg and actual block given");
    } else {
      a = push(args_add, b);
    }
  }

  static void
  args_with_block_super(ParserState *p, Node *a, Node *b)
  {
    if (!b) return;
    Node *args_add;
    if (Node_atomType(a->cons.cdr->cons.car) == ATOM_args_add) {
      /* fcall */
      args_add = a->cons.cdr->cons.car;
    } else if (Node_atomType(a->cons.cdr->cons.cdr->cons.car) == ATOM_args_add) {
      /* call */
      args_add = a->cons.cdr->cons.cdr->cons.car;
    } else {
      FATALP("This should not happen");
    }
    if (Node_atomType(args_add->cons.cdr->cons.cdr->cons.cdr->cons.car) == ATOM_block_arg) {
      //yyerror(p, "both block arg and actual block given");
      ERRORP("both block arg and actual block given");
    } else {
      a = push(args_add, b);
    }
  }

  static void
  call_with_block(ParserState *p, Node *a, Node *b)
  {
    Node *n;
    switch (Node_atomType(a)) {
    case ATOM_super:
    case ATOM_zsuper:
      if (a->cons.cdr && !a->cons.cdr->cons.car) {
        a->cons.cdr->cons.car = new_callargs(p, 0, 0, b);
      } else {
        args_with_block_super(p, a, b);
      }
      break;
    case ATOM_call:
    case ATOM_fcall:
      n = a->cons.cdr->cons.cdr;
      if (n->cons.cdr && !n->cons.cdr->cons.car) {
        n->cons.cdr->cons.car = new_callargs(p, 0, 0, b);
      } else {
        args_with_block(p, a, b);
      }
      break;
    default:
      break;
    }
  }

  static Node*
  new_class(ParserState *p, Node *c, Node *b)
  {
    return list4(atom(ATOM_class), c->cons.car, c->cons.cdr, b);
  }

  static Node*
  new_sclass(ParserState *p, Node *c, Node *b)
  {
    return list3(atom(ATOM_sclass), c, b);
  }

  static Node*
  new_module(ParserState *p, Node *c, Node *b)
  {
    return list4(atom(ATOM_module), c->cons.car, NULL, b);
    //return list4(atom(ATOM_module), c->cons.cdr, NULL, b);
  }

  static Node*
  new_def(ParserState *p, const char* m, Node *a, Node *b)
  {
    return list5(atom(ATOM_def), literal(m), 0, a, b);
  }

  static Node*
  defn_setup(ParserState *p, Node *d, Node *a, Node *b)
  {
    /*
     * d->cons.cdr->cons.cdr->cons.cdr->cons.cdr
     *    ^^^^^^^^  ^^^^^^^^  ^^^^^^^^  ^^^^^^^^
     *  methodname  ???????   args      body
     */
    Node *n = d->cons.cdr->cons.cdr;
    p->cmdarg_stack = intn(n->cons.cdr->cons.car);
    n->cons.cdr->cons.car = a;
    //local_resume(p, n->cons.cdr->cons.cdr->cons.car);
    n->cons.cdr->cons.cdr->cons.car = b;
    return d;
  }

  static Node*
  new_sdef(ParserState *p, Node *o, const char* m, Node *a, Node *b)
  {
    return list6(atom(ATOM_sdef), o, literal(m), 0, a, b);
  }

  static Node*
  defs_setup(ParserState *p, Node *d, Node *a, Node *b)
  {
    /*
     * d->cons.cdr->cons.cdr->cons.cdr->cons.cdr
     *    ^^^^^^^^  ^^^^^^^^  ^^^^^^^^  ^^^^^^^^
     *  methodname  ???????   args      body
     */
    Node *n = d->cons.cdr->cons.cdr->cons.cdr;
    p->cmdarg_stack = intn(n->cons.cdr->cons.car);
    n->cons.cdr->cons.car = a;
    //local_resume(p, n->cons.cdr->cons.cdr->cons.car);
    n->cons.cdr->cons.cdr->cons.car = b;
    return d;
  }

  static void
  local_add_f(ParserState *p, const char *a)
  {
    // different way from mruby...
    LvarScopeReg lvar = Scope_lvar_findRegnum(p->scope, a);
    if (lvar.scope_num != 0 || lvar.reg_num == 0) {
      /* If no lvar found in the current scope */
      Scope_newLvar(p->scope, a, p->scope->sp++);
    }
  }

  static Node*
  new_arg(ParserState *p, const char* a)
  {
    LvarScopeReg lvar = Scope_lvar_findRegnum(p->scope, a);
    if (lvar.reg_num == 0)
      Scope_newLvar(p->scope, a, p->scope->sp);
    return list2(atom(ATOM_arg), literal(a));
  }

  static Node*
  new_args(ParserState *p, Node *m, Node *opt, Node *rest, Node *m2, Node *tail)
  {
    return list6(atom(ATOM_block_parameters),
      list2(atom(ATOM_margs), m),
      list2(atom(ATOM_optargs), opt),
      list2(atom(ATOM_restarg), rest),
      list2(atom(ATOM_m2args), m2),
      tail
    );
  }

  static void
  local_add_blk(ParserState *p, const char *blk)
  {
    local_add_f(p, blk ? blk : "&");
  }

  static Node*
  new_args_tail(ParserState *p, Node *kws, Node *kwrest, const char *blk)
  {
    local_add_blk(p, blk);
    return list4(
      atom(ATOM_args_tail),
      cons(atom(ATOM_args_tail_kw_args), kws),
      //kwrest,
      list2(atom(ATOM_args_tail_kw_rest_args), kwrest),
      list2(atom(ATOM_args_tail_block), literal(blk))
    );
  }

  static Node*
  new_kw_arg(ParserState *p, const char *kw, Node *a)
  {
    return cons(atom(ATOM_args_tail_kw_arg), list2(literal(kw), a));
  }

  /* (:dstr . a) */
  static Node*
  new_dstr(ParserState *p, Node *a)
  {
    return list2(atom(ATOM_dstr), a);
  }

  static Node*
  ret_args(ParserState *p, Node *a)
  {
    if (Node_atomType(a->cons.cdr->cons.car) == ATOM_args_add)
      /* multiple return values will be an array */
      return new_array(p, a);
    return a;
  }

  static Node*
  new_hash(ParserState *p, Node *a)
  {
    return cons(atom(ATOM_hash), a);
  }

  static Node*
  new_kw_hash(ParserState *p, Node *a)
  {
    return cons(atom(ATOM_kw_hash), a);
  }

  static Node*
  new_if(ParserState *p, Node *b, Node *c, Node *d)
  {
    return list4(atom(ATOM_if), b, c, d);
  }

  static Node*
  new_while(ParserState *p, Node *b, Node *c)
  {
    return list3(atom(ATOM_while), b, c);
  }

  static Node*
  new_until(ParserState *p, Node *b, Node *c)
  {
    return list3(atom(ATOM_until), b, c);
  }

  static Node*
  new_case(ParserState *p, Node *b, Node *c)
  {
    return list3(atom(ATOM_case), b, cons(atom(ATOM_case_body), c));
  }

  static Node*
  new_break(ParserState *p, Node *b)
  {
    return list2(atom(ATOM_break), b);
  }

  static Node*
  new_next(ParserState *p, Node *b)
  {
    return list2(atom(ATOM_next), b);
  }

  static Node*
  new_colon2(ParserState *p, Node *b, const char *c)
  {
    return list3(atom(ATOM_colon2), b, list1(literal(c)));
  }

  static Node*
  new_colon3(ParserState *p, const char *b)
  {
    return list2(atom(ATOM_colon3), literal(b));
  }

  static Node*
  new_and(ParserState *p, Node *b, Node *c)
  {
    return list3(atom(ATOM_and), b, c);
  }

  static Node*
  new_or(ParserState *p, Node *b, Node *c)
  {
    return list3(atom(ATOM_or), b, c);
  }

  static Node*
  new_str(ParserState *p, const char *a)
  {
    if (a == NULL || a[0] == '\0') {
      return list2(atom(ATOM_str), literal(STRING_NULL));
    } else {
      return list2(atom(ATOM_str), literal(a));
    }
  }

  static Node*
  unite_str(ParserState *p, Node *a, Node* b) {
    const char *a_value = Node_valueName(a);
    size_t a_len = strlen(a_value);
    const char *b_value = Node_valueName(b);
    size_t b_len = strlen(b_value);
    char new_value[a_len + b_len + 1];
    memcpy(new_value, a_value, a_len);
    memcpy(new_value + a_len, b_value, b_len);
    new_value[a_len + b_len] = '\0';
    const char *value = ParsePushStringPool(p, new_value);
    Node *n = literal(value);
    return n;
  }

  static Node*
  concat_string(ParserState *p, Node *a, Node* b)
  {
    Node *new_str_value;
    if (Node_atomType(a) == ATOM_str) {
      if (Node_atomType(b) == ATOM_str) { /* "str" "str" */
        new_str_value = unite_str(p, a->cons.cdr->cons.car, b->cons.cdr->cons.car);
        a->cons.cdr->cons.car = new_str_value;
        return a;
      } else { /* "str" "dstr" */
        Node *n = b->cons.cdr->cons.car;
        while (Node_atomType(n->cons.cdr->cons.car) != ATOM_dstr_new) {
          n = n->cons.cdr->cons.car;
        }
        new_str_value = unite_str(p, a->cons.cdr->cons.car, n->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car);
        n->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car = new_str_value;
        return b;
      }
    } else {
      if (Node_atomType(b) == ATOM_str) { /* "dstr" "str" */
        Node *a2 = a->cons.cdr->cons.car->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car;
        new_str_value = unite_str(p, a2, b->cons.cdr->cons.car);
        a->cons.cdr->cons.car->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car = new_str_value;
        return a;
      } else { /* "dstr_a" "dstr_b" ...ex) `"w#{1}x" "y#{2}z"`*/
        Node *a2, *b2;
        { /* unite the last str of dstr_a and the first str of dstr_b ...ex) "x" "y" */
          a2 = a->cons.cdr->cons.car->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car;
          b2 = b->cons.cdr->cons.car;
          while (Node_atomType(b2->cons.cdr->cons.car) != ATOM_dstr_new) {
            b2 = b2->cons.cdr->cons.car;
          }
          new_str_value = unite_str(p, a2, b2->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car);
          a->cons.cdr->cons.car->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car = new_str_value;
          b2 = b2->cons.cdr->cons.cdr->cons.car;
        }
        Node *orig_a = a->cons.cdr->cons.car; /* copy to reuse it later */
        { /* replace dstr_a tree with dstr_b tree */
          a->cons.cdr->cons.car = b->cons.cdr->cons.car;
        }
        { /* insert original dstr_a tree into the bottom of new dstr tree*/
          a2 = a->cons.cdr->cons.car;
          while (Node_atomType(a2->cons.cdr->cons.car->cons.cdr->cons.car) != ATOM_dstr_new) {
            a2 = a2->cons.cdr->cons.car;
          }
          a2->cons.cdr->cons.car = orig_a;  /* insert original dstr_a */
        }
        return a;
      }
    }
    FATALP("concat_string(); This should not happen");
  }

  static Node*
  new_alias(ParserState *p, const char *a, const char *b)
  {
    return list3(atom(ATOM_alias), new_sym(p, a), new_sym(p, b));
  }

  static Node*
  new_dot2(ParserState *p, Node *a, Node *b)
  {
    return list3(atom(ATOM_dot2), a, b);
  }

  static Node*
  new_dot3(ParserState *p, Node *a, Node *b)
  {
    return list3(atom(ATOM_dot3), a, b);
  }

  static void
  scope_nest(ParserState *p, bool lvar_top)
  {
    p->scope = Scope_new(p->scope, lvar_top);
  }

  static void
  scope_unnest(ParserState *p)
  {
    p->scope = p->scope->upper;
  }

  static Node*
  new_rescue(ParserState *p, Node *body, Node* resq, Node* els)
  {
    return push(append(list2(atom(ATOM_rescue), body), resq), els);
  }

  static Node*
  new_mod_rescue(ParserState *p, Node *body, Node* resq)
  {
    return new_rescue(p, body, list3(0, 0, resq), 0);
  }

  static Node*
  new_exc_var(ParserState *p, Node *a)
  {
    if (Node_atomType(a) == ATOM_lvar) generate_lvar(p->scope, a);
    return a;
  }

  static Node*
  new_ensure(ParserState *p, Node *a, Node *b)
  {
    return list3(atom(ATOM_ensure), a, b);
  }
}

%parse_accept {
}

%syntax_error {
  p->error_count++;
}

%parse_failure {
  p->error_count++;
}

%start_symbol program

%nonassoc LOWEST.
%nonassoc LBRACE_ARG.

%nonassoc  KW_modifier_if KW_modifier_unless KW_modifier_while KW_modifier_until.
%left KW_or KW_and.
%right KW_not.
%right E OP_ASGN.
%left KW_modifier_rescue.
%right QUESTION COLON LABEL_TAG.
%nonassoc DOT2 DOT3 BDOT2 BDOT3.
%left OROP.
%left ANDOP.
%nonassoc CMP EQ EQQ NEQ MATCH NMATCH.
%left GT GEQ LT LEQ. // > >= < <=
%left OR XOR.
%left AND.
%left LSHIFT RSHIFT.
%left PLUS MINUS.
%left TIMES DIVIDE SURPLUS.
%right UMINUS_NUM UMINUS.
%right POW.
%right UNEG UNOT UPLUS.

program ::= top_compstmt(B).  {
                                yypParser->p->root_node_box->nodes = list2(atom(ATOM_program), B);
                              }

top_compstmt(A) ::= top_stmts(B) opt_terms. { A = B; }

top_stmts(A) ::= none. { A = new_begin(p, 0); }
top_stmts(A) ::= top_stmt(B). { A = new_begin(p, B); }
top_stmts(A) ::= top_stmts(B) terms top_stmt(C).
  { A = list3(atom(ATOM_stmts_add), B, C); }

top_stmt ::= stmt.

bodystmt(A) ::= compstmt(B)
                opt_rescue(C)
                opt_else(D)
                opt_ensure(E).
                {
                  if (C) {
                    A = new_rescue(p, B, C, D);
                  }
                  else if (D) {
                    //yywarning(p, "else without rescue is useless");
                    A = B; // (B, D);
                  }
                  else {
                    A = B;
                  }
                  if (E) {
                    if (A) {
                      A = new_ensure(p, A, E);
                    }
                    else {
                      A = push(E, new_nil(p));
                    }
                  }
                }

compstmt(A) ::= stmts(B) opt_terms. { A = B; }

stmts(A) ::= none. { A = new_begin(p, 0); }
stmts(A) ::= stmt(B). { A = new_begin(p, B); }
stmts(A) ::= stmts(B) terms stmt(C). { A = list3(atom(ATOM_stmts_add), B, C); }

stmt(A) ::= none. { A = new_begin(p, 0); }
stmt_alias(A) ::= KW_alias set_expr_fname fsym(B). {
                   A = B;
                  }
set_expr_fname ::= .
                {
                  p->state = EXPR_FNAME;
                }
stmt(A) ::= stmt_alias(B) fsym(C). {
              p->state = EXPR_BEG;
              A = new_alias(p, B, C);
            }
stmt(A) ::= stmt(B) KW_modifier_if expr_value(C). {
              A = new_if(p, C, B, 0);
            }
stmt(A) ::= stmt(B) KW_modifier_unless expr_value(C). {
              A = new_if(p, C, 0, B);
            }
stmt(A) ::= stmt(B) KW_modifier_while expr_value(C). {
              A = new_while(p, C, B);
            }
stmt(A) ::= stmt(B) KW_modifier_until expr_value(C). {
              A = new_until(p, C, B);
            }
stmt(A) ::= stmt(B) KW_modifier_rescue stmt(C).
              {
                A = new_mod_rescue(p, B, C);
              }
stmt ::= command_asgn.
stmt(A) ::= mlhs(B) E command_call(C).
              {
                A = new_masgn(p, B, C);
              }
stmt(A) ::= lhs(B) E mrhs(C). {
              A = new_asgn(p, B, new_array(p, C));
            }
stmt(A) ::= mlhs(B) E arg(C).
              {
                A = new_masgn(p, B, C);
              }
stmt(A) ::= mlhs(B) E mrhs(C).
              {
                A = new_masgn(p, B, new_array(p, C));
              }
stmt(A) ::= arg(B) ASSOC IDENTIFIER(C).
              {
                Node *lhs = new_lvar(p, C);
                A = new_asgn(p, lhs, B);
              }
stmt ::= expr.

command_asgn(A) ::= lhs(B) E command_rhs(C). {
                      A = new_asgn(p, B, C);
                    }
//command_asgn(A) ::= var_lhs(B) OP_ASGN(C) command_rhs(D). { A = new_op_asgn(p, B, C, D); }
//command_asgn(A) ::= primary_value(B) LBRACKET opt_call_args(C) RBRACKET OP_ASGN(D) command_rhs(E).
//  { A = new_op_asgn(p, new_call(p, B, STRING_ARY, C, '.'), D, E); }
//
command_asgn(A) ::= defn_head(B) f_opt_arglist_paren(C) E command(D).
                {
                  A = defn_setup(p, B, C, D);
                  scope_unnest(p);
                  p->in_def--;
                }
command_asgn(A) ::= defn_head(B) f_opt_arglist_paren(C) E command(D) KW_modifier_rescue arg(E).
                {
                  A = defn_setup(p, B, C, new_mod_rescue(p, D, E));
                  scope_unnest(p);
                  p->in_def--;
                }
command_asgn(A) ::= defs_head(B) f_opt_arglist_paren(C) E command(D).
                {
                  A = defs_setup(p, B, C, D);
                  scope_unnest(p);
                  p->in_def--;
                  p->in_single--;
                }
command_asgn(A) ::= defs_head(B) f_opt_arglist_paren(C) E command(D) KW_modifier_rescue arg(E).
                {
                  A = defs_setup(p, B, C, new_mod_rescue(p, D, E));
                  scope_unnest(p);
                  p->in_def--;
                  p->in_single--;
                }
command_rhs    ::= command_call. [OP_ASGN]
command_rhs(A) ::= command_call(B) KW_modifier_rescue stmt(C).
                {
                  A = new_mod_rescue(p, B, C);
                }
command_rhs    ::= command_asgn.

expr ::= command_call.
expr(A) ::= expr(B) KW_and expr(C). { A = new_and(p, B, C); }
expr(A) ::= expr(B) KW_or expr(C). { A = new_or(p, B, C); }
expr ::= arg.

defn_head(A) ::= KW_def fname(C). {
                  // , local_switch(p)
                  A = new_def(p, C, nint(p->cmdarg_stack), 0);
                  p->cmdarg_stack = 0;
                  p->in_def++;
                  // nvars_block(p);
                  scope_nest(p, true);
                }
defs_head(A) ::= KW_def singleton(B) dot_or_colon fname(C). {
                  // , local_switch(p)
                  A = new_sdef(p, B, C, nint(p->cmdarg_stack), 0);
                  p->cmdarg_stack = 0;
                  p->in_def++;
                  p->in_single++;
                  // nvars_block(p);
                  scope_nest(p, true);

                }
dot_or_colon ::= PERIOD.
                {
                  p->state = EXPR_ENDFN;
                }
dot_or_colon ::= COLON2.
                {
                  p->state = EXPR_ENDFN;
                }
singleton(A) ::= var_ref(B).
                {
                  if (!B) A = new_nil(p);
                  else A = B;
                  p->state = EXPR_FNAME;
                }
singleton(A) ::= LPAREN_EXPR change_state_beg expr(B) RPAREN.
                {
                  A = B;
                }
change_state_beg ::= .
                {
                  p->state = EXPR_BEG;
                }
expr_value(A) ::= expr(B). {
                   if (!B) A = new_nil(p);
                   else A = B;
                  }


command_call ::= command.
command_call ::= block_command.

block_command ::= block_call.

command(A) ::= operation(B) command_args(C). [LOWEST]
                {
                  A = new_fcall(p, B, C);
                }
command(A) ::= primary_value(B) call_op(C) operation2(D) command_args(E). [LOWEST]
                {
                  A = new_call(p, B, D, E, C);
                }
command(A) ::= KW_super command_args(B).
              {
                A = new_super(p, B);
              }
command(A) ::= KW_yield command_args(B). { A = new_yield(p, B); }
command(A) ::= KW_return call_args(B). { A = new_return(p, ret_args(p, B)); }
command(A) ::= KW_break call_args(B). { A = new_break(p, ret_args(p, B)); }
command(A) ::= KW_next call_args(B). { A = new_next(p, ret_args(p, B)); }

command_args(A) ::= push_cmdarg(STACK) call_args(B).
                {
                  p->cmdarg_stack = STACK;
                  //A = list2(atom(ATOM_args_add), list2(atom(ATOM_args_new), B));
                  A = B;
                }
push_cmdarg(STACK) ::= .
                {
                  STACK = p->cmdarg_stack;
                  CMDARG_PUSH(1);
                }

mlhs    ::= mlhs_basic.
mlhs(A) ::= LPAREN mlhs_inner(B) RPAREN.
              {
                A = B;
              }

mlhs_inner    ::= mlhs_basic.
mlhs_inner(A) ::= LPAREN mlhs_inner(B) RPAREN.
              {
                A = B;
              }

mlhs_basic(A) ::= mlhs_list(B).
              {
                A = list1(cons(atom(ATOM_mlhs_pre), B));
              }
mlhs_basic(A) ::= mlhs_list(B) mlhs_item(C).
              {
                A = list1(cons(atom(ATOM_mlhs_pre), push(B, C)));
              }
mlhs_basic(A) ::= mlhs_list(B) STAR mlhs_node(C).
              {
                A = list2(cons(atom(ATOM_mlhs_pre), B), list2(atom(ATOM_mlhs_rest), C));
              }
mlhs_basic(A) ::= mlhs_list(B) STAR mlhs_node(C) COMMA mlhs_post(D).
              {
                A = list3(cons(atom(ATOM_mlhs_pre), B), list2(atom(ATOM_mlhs_rest), C), D);
              }
mlhs_basic(A) ::= mlhs_list(B) STAR.
              {
                A = list2(cons(atom(ATOM_mlhs_pre), B), list2(atom(ATOM_mlhs_rest), new_nil(p)));
              }
mlhs_basic(A) ::= mlhs_list(B) STAR COMMA mlhs_post(C).
              {
                A = list3(cons(atom(ATOM_mlhs_pre), B), list2(atom(ATOM_mlhs_rest), new_nil(p)), C);
              }
mlhs_basic(A) ::= STAR mlhs_node(C) COMMA mlhs_post(D).
              {
                A = list3(0, list2(atom(ATOM_mlhs_rest), C), D);
              }
mlhs_basic(A) ::= STAR mlhs_node(C).
              {
                A = list2(0, list2(atom(ATOM_mlhs_rest), C));
              }
mlhs_basic(A) ::= STAR.
              {
                A = list2(0, list2(atom(ATOM_mlhs_rest), new_nil(p)));
              }
mlhs_basic(A) ::= STAR COMMA mlhs_post(D).
              {
                A = list3(0, list2(atom(ATOM_mlhs_rest), new_nil(p)), D);
              }

mlhs_item    ::= mlhs_node.
mlhs_item(A) ::= LPAREN mlhs_inner(B) RPAREN.
                {
                  A = new_masgn(p, B, NULL);
                }

mlhs_list(A) ::= mlhs_item(B) COMMA.
                {
                  A = list1(B);
                }
mlhs_list(A) ::= mlhs_list(B) mlhs_item(C) COMMA.
                {
                  A = push(B, C);
                }

mlhs_post(A) ::= mlhs_item(B).
                {
                  A = list2(atom(ATOM_mlhs_post), B);
                }
mlhs_post(A) ::= mlhs_list(B) mlhs_item(C).
                {
                  A = cons(atom(ATOM_mlhs_post), push(B, C));
                }

mlhs_node    ::= variable(VAR).
                {
                  generate_lvar(p->scope, VAR);
                }
mlhs_node(A) ::= primary_value(B) LBRACKET opt_call_args(C) RBRACKET.
                {
                  A = new_call(p, B, STRING_ARY, C, '.');
                }
mlhs_node(A) ::= primary_value(B) call_op(C) IDENTIFIER(D).
                {
                  A = new_call(p, B, D, 0, C);
                }
mlhs_node(A) ::= primary_value(B) COLON2 IDENTIFIER(D).
                {
                  A = new_call(p, B, D, 0, COLON2);
                }
mlhs_node(A) ::= primary_value(B) call_op(C) CONSTANT(D).
                {
                  A = new_call(p, B, D, 0, C);
                }
mlhs_node(A) ::= primary_value(B) COLON2 CONSTANT(C).
                {
                  //if (p->in_def || p->in_single)
                  //  yyerror(p, "dynamic constant assignment")
                  A = new_colon2(p, B, C);
                }
mlhs_node(A) ::= COLON3 CONSTANT(B).
                {
                  //if (p->in_def || p->in_single)
                  //  yyerror(p, "dynamic constant assignment")
                  A = new_colon3(p, B);
                }

block_arg(A) ::= AMPER arg(B).
              {
                A = new_block_arg(p, B);
              }

opt_block_arg(A) ::= comma block_arg(B).
              {
                //A = list2(atom(ATOM_args_add), B);
                A = B;
              }
opt_block_arg(A) ::= none. { A = 0; }

comma ::= COMMA opt_nl.

args(A) ::= arg(B).
              {
                A = new_first_arg(p, B);
              }
args(A) ::= STAR arg(B).
            {
              A = new_first_arg(p, new_splat(p, B));
            }
args(A) ::= args(B) comma arg(C). {
              A = list3(atom(ATOM_args_add), B, C);
            }
args(A) ::= args(B) comma STAR arg(C). {
              A = list3(atom(ATOM_args_add), B, new_splat(p, C));
            }

mrhs(A) ::= args(B) comma arg(C). {
              A = list3(atom(ATOM_args_add), B, C);
            }
mrhs(A) ::= args(B) comma STAR arg(C). {
              A = list3(atom(ATOM_args_add), B, new_splat(p, C));
            }
mrhs(A) ::= STAR arg(B). {
              A = new_first_arg(p, new_splat(p, B));
            }

arg(A) ::= lhs(B) E arg_rhs(C). { A = new_asgn(p, B, C); }
arg(A) ::= var_lhs(B) OP_ASGN(C) arg_rhs(D). { A = new_op_asgn(p, B, C, D); }
arg(A) ::= primary_value(B) LBRACKET opt_call_args(C) RBRACKET OP_ASGN(D) arg_rhs(E).
  { A = new_op_asgn(p, new_call(p, B, STRING_ARY, C, '.'), D, E); }
arg(A) ::= primary_value(B) call_op(C) IDENTIFIER(D) OP_ASGN(E) arg_rhs(F).
  { A = new_op_asgn(p, new_call(p, B, D, 0, C), E, F); }
arg(A) ::= arg(B) DOT2 arg(C). {
  A = new_dot2(p, B, C);
}
arg(A) ::= arg(B) DOT2. {
  A = new_dot2(p, B, new_nil(p));
}
arg(A) ::= BDOT2 arg(B). {
  A = new_dot2(p, new_nil(p), B);
}
arg(A) ::= arg(B) DOT3 arg(C). {
  A = new_dot3(p, B, C);
}
arg(A) ::= arg(B) DOT3. {
  A = new_dot3(p, B, new_nil(p));
}
arg(A) ::= BDOT3 arg(B). {
  A = new_dot3(p, new_nil(p), B);
}
arg(A) ::= arg(B) PLUS arg(C).   { A = call_bin_op(B, "+" ,C); }
arg(A) ::= arg(B) MINUS arg(C).  { A = call_bin_op(B, "-", C); }
arg(A) ::= arg(B) TIMES arg(C).  { A = call_bin_op(B, "*", C); }
arg(A) ::= arg(B) DIVIDE arg(C). { A = call_bin_op(B, "/", C); }
arg(A) ::= arg(B) SURPLUS arg(C). { A = call_bin_op(B, "%", C); }
arg(A) ::= arg(B) POW arg(C). { A = call_bin_op(B, "**", C); }
arg(A) ::= UPLUS arg(B). { A = call_uni_op(p, B, "+@"); }
arg(A) ::= UMINUS arg(B). { A = call_uni_op(p, B, "-@"); }
arg(A) ::= arg(B) OR arg(C). { A = call_bin_op(B, "|", C); }
arg(A) ::= arg(B) XOR arg(C). { A = call_bin_op(B, "^", C); }
arg(A) ::= arg(B) AND arg(C). { A = call_bin_op(B, "&", C); }
arg(A) ::= arg(B) CMP arg(C). { A = call_bin_op(B, "<=>", C); }
arg(A) ::= arg(B) GT arg(C). { A = call_bin_op(B, ">", C); }
arg(A) ::= arg(B) GEQ arg(C). { A = call_bin_op(B, ">=", C); }
arg(A) ::= arg(B) LT arg(C). { A = call_bin_op(B, "<", C); }
arg(A) ::= arg(B) LEQ arg(C). { A = call_bin_op(B, "<=", C); }
arg(A) ::= arg(B) EQ arg(C). { A = call_bin_op(B, "==", C); }
arg(A) ::= arg(B) EQQ arg(C). { A = call_bin_op(B, "===", C); }
arg(A) ::= arg(B) NEQ arg(C). { A = call_bin_op(B, "!=", C); }
arg(A) ::= arg(B) MATCH arg(C). { A = call_bin_op(B, "=~", C); }
arg(A) ::= arg(B) NMATCH arg(C). { A = call_bin_op(B, "!~", C); }
arg(A) ::= UNEG arg(B). { A = call_uni_op(p, B, "!"); }
arg(A) ::= UNOT arg(B). { A = call_uni_op(p, B, "~"); }
arg(A) ::= arg(B) LSHIFT arg(C). { A = call_bin_op(B, "<<", C); }
arg(A) ::= arg(B) RSHIFT arg(C). { A = call_bin_op(B, ">>", C); }
arg(A) ::= arg(B) ANDOP arg(C). { A = new_and(p, B, C); }
arg(A) ::= arg(B) OROP arg(C). { A = new_or(p, B, C); }
arg(A) ::= arg(B) QUESTION arg(C) COLON arg(D). { A = new_if(p, B, C, D); }
arg(A) ::= defn_head(B) f_opt_arglist_paren(C) E arg(D). {
                  A = defn_setup(p, B, C, D);
                  scope_unnest(p);
                  p->in_def--;
                }
arg(A) ::= defn_head(B) f_opt_arglist_paren(C) E arg(D) KW_modifier_rescue arg(E). {
                  A = defn_setup(p, B, C, new_mod_rescue(p, D, E));
                  scope_unnest(p);
                  p->in_def--;
                }
arg(A) ::= defs_head(B) f_opt_arglist_paren(C) E arg(D). {
                  A = defs_setup(p, B, C, D);
                  scope_unnest(p);
                  p->in_def--;
                  p->in_single--;
                }
arg(A) ::= defs_head(B) f_opt_arglist_paren(C) E arg(D) KW_modifier_rescue arg(E). {
                  A = defs_setup(p, B, C, new_mod_rescue(p, D, E));
                  scope_unnest(p);
                  p->in_def--;
                  p->in_single--;
                }

f_opt_arglist_paren ::= f_arglist_paren.
f_opt_arglist_paren ::= none.

arg ::= primary.

arg_rhs    ::= arg. [OP_ASGN]
arg_rhs(A) ::= arg(B) KW_modifier_rescue arg(C).
                {
                  A = new_mod_rescue(p, B, C);
                }

lhs ::= variable.
lhs(A) ::= primary_value(B) LBRACKET opt_call_args(C) RBRACKET.
                {
                  A = new_call(p, B, STRING_ARY, C, '.');
                }
lhs(A) ::= primary_value(B) call_op(C) IDENTIFIER(D).
                {
                  A = new_call(p, B, D, 0, C);
                }

cname ::= CONSTANT.

cpath(A) ::= COLON3 cname(B). {
  A = new_colon3(p, B);
}
cpath(A) ::= cname(B). {
  A = list2(NULL, literal(B));
}
cpath(A) ::= primary_value(B) COLON2 cname(C). {
  A = new_colon2(p, B, C);
}

var_lhs ::= variable.

primary     ::= literal.
primary     ::= string.
primary     ::= var_ref.
primary(A)  ::= KW_begin
                preserve_cmdarg_stack(STACK)
                bodystmt(B)
                KW_end. {
                  p->cmdarg_stack = STACK;
                  A = B;
                }
preserve_cmdarg_stack(STACK) ::= .
                {
                  STACK = p->cmdarg_stack;
                  p->cmdarg_stack = 0;
                }
primary(A)  ::= LPAREN_ARG stmt(B) rparen. { A = B; }
primary(A)  ::= LPAREN compstmt(B) rparen. {
                  A = B;
                }
primary(A)  ::= LBRACKET_ARRAY aref_args(B) RBRACKET. { A = new_array(p, B); }
primary(A)  ::= primary_value(B) COLON2 CONSTANT(C). { A = new_colon2(p, B, C); }
primary(A)  ::= COLON3 CONSTANT(B). { A = new_colon3(p, B); }
primary(A)  ::= LBRACE assoc_list(B) RBRACE. { A = new_hash(p, B); }
primary(A)  ::= KW_return. { A = new_return(p, 0); }
primary(A)  ::= KW_yield opt_paren_args(B). { A = new_yield(p, B); }
primary(A)  ::= KW_not LPAREN_EXPR expr(B) rparen. { A = call_uni_op(p, B, "!"); }
primary(A)  ::= KW_not LPAREN_EXPR rparen. { A = call_uni_op(p, new_nil(p), "!"); }
primary(A)  ::= operation(B) brace_block(C). {
                  //A = new_fcall(p, B, list2(atom(ATOM_args_add), list2(atom(ATOM_args_new), C)));
                  A = new_fcall(p, B, new_callargs(p, 0, 0, C));
                }
primary     ::= method_call.
primary(A)  ::= method_call(B) brace_block(C). {
                  call_with_block(p, B, C);
                  A = B;
                }
primary(A)  ::= lambda_head(NUM) LAMBDA f_larglist(B) preserve_cmdarg_stack(STACK) lambda_body(C).
                    {
                      p->lpar_beg = NUM;
                      A = new_lambda(p, B, C);
                      scope_unnest(p);
                      p->cmdarg_stack = STACK;
                      CMDARG_LEXPOP();
                    }
lambda_head(NUM) ::= .
                    {
                      scope_nest(p, false);
                      NUM = p->lpar_beg;
                      p->lpar_beg = ++p->paren_stack_num;
                    }
primary(A)  ::= KW_if expr_value(B) then
                compstmt(C)
                if_tail(D)
                KW_end. {
                  A = new_if(p, B, C, D);
                }
primary(A)  ::= KW_unless expr_value(B) then
                compstmt(C)
                opt_else(D)
                KW_end. {
                  A = new_if(p, B, D, C); /* NOTE: `D, C` in inverse order */
                }
primary(A) ::=  cond_push_while expr_value(B) do_pop compstmt(C) KW_end. {
                  A = new_while(p, B, C);
                }
cond_push_while ::= KW_while. { COND_PUSH(1); }
do_pop ::= do. { COND_POP(); }
primary(A) ::=  cond_push_until expr_value(B) do_pop compstmt(C) KW_end. {
                  A = new_until(p, B, C);
                }
cond_push_until ::= KW_until. { COND_PUSH(1); }
primary(A) ::=  KW_case expr_value(B) opt_terms
                case_body(C)
                KW_end. {
                  A = new_case(p, B, C);
                }
primary(A) ::=  KW_case opt_terms case_body(C) KW_end. {
                  A = new_case(p, 0, C);
                }
class_head(A) ::= KW_class
                  cpath(B) superclass(C). {
                    // TODO: raise error if (p->in_def || p->in_single)
                    A = cons(B, C);
                    scope_nest(p, true);
                  }
primary(A) ::=  class_head(B)
                bodystmt(C)
                KW_end.
                {
                  A = new_class(p, B, C);
                  scope_unnest(p);
                }
primary(A) ::=  KW_class LSHIFT preserve_in_def(NUM) expr(B) preserve_in_single(ND) term
                bodystmt(C)
                KW_end.
                {
                  A = new_sclass(p, B, C);
                  scope_unnest(p);
                  p->in_def = NUM;
                  p->in_single = ND;
                }
preserve_in_def(NUM) ::= .
                {
                  NUM = p->in_def;
                  p->in_def = 0;
                }
preserve_in_single(ND) ::= .
                {
                  ND = p->in_single;
                  scope_nest(p, true);
                  p->in_single = 0;
                }
module_head(A) ::= KW_module cpath(B). {
                    // TODO: raise error if (p->in_def || p->in_single)
                    A = cons(B, NULL);
                    scope_nest(p, true);
                  }
primary(A) ::=  module_head(B)
                bodystmt(C)
                KW_end. {
                  A = new_module(p, B, C);
                  scope_unnest(p);
                }
primary(A) ::=  defn_head(B) f_arglist(C)
                  bodystmt(D)
                KW_end.
                {
                  A = defn_setup(p, B, C, D);
                  scope_unnest(p);
                  p->in_def--;
                }
primary(A) ::=  defs_head(B) f_arglist(C)
                  bodystmt(D)
                KW_end.
                {
                  A = defs_setup(p, B, C, D);
                  scope_unnest(p);
                  p->in_def--;
                  p->in_single--;
                }
primary(A) ::=  KW_break. {
                  A = new_break(p, 0);
                }
primary(A) ::=  KW_next. {
                  A = new_next(p, 0);
                }
primary(A) ::=  KW_redo. {
                  A = list1(atom(ATOM_redo));
                }
primary(A) ::=  KW_retry. {
                  A = list1(atom(ATOM_retry));
                }

primary_value(A) ::= primary(B). { A = B; }

then ::= term.
then ::= KW_then.
then ::= term KW_then.

do ::= term.
do ::= KW_do_cond.
do ::= KW_do. // TODO remove after definingbrace_block

if_tail     ::= opt_else.
if_tail(A)  ::= KW_elsif expr_value(B) then
                compstmt(C)
                if_tail(D). {
                  A = new_if(p, B, C, D);
                }

opt_else    ::= none.
opt_else(A) ::= KW_else compstmt(B). { A = B; }

assoc_list ::= none.
assoc_list(A) ::= assocs(B) trailer. { A = B; }

assocs(A) ::= assoc(B). { A = list1(B); }
assocs(A) ::= assocs(B) comma assoc(C). { A = push(B, C); }

assoc(A) ::= arg(B) ASSOC arg(C).
              {
                A = list3(
                  atom(ATOM_assoc_new),
                  list2(atom(ATOM_assoc_key), B),
                  list2(atom(ATOM_assoc_value), C)
                );
              }
assoc(A) ::= LABEL(B) LABEL_TAG arg(C).
              {
                A = list3(
                  atom(ATOM_assoc_new),
                  list2(atom(ATOM_assoc_key), new_sym(p, B)),
                  list2(atom(ATOM_assoc_value), C)
                );
              }
assoc(A) ::= DSTAR arg(B).
              {
                A = B;
              }


aref_args     ::= none.
aref_args(A)  ::= args(B) trailer.
              {
                A = B;
              }
aref_args(A)  ::= args(B) comma assocs(C) trailer.
              {
                A = push(B, new_hash(p, C));
              }

f_label ::= LABEL LABEL_TAG.
                {
                //  local_nest(p);
                }

f_block_kw(A) ::= f_label(B) primary_value(C).
                {
                  A = new_kw_arg(p, B, C);
                  //local_unnest(p);
                }
f_block_kw(A) ::= f_label(B).
                {
                  A = new_kw_arg(p, B, 0);
                  //local_unnest(p);
                }

f_block_kwarg(A) ::= f_block_kw(B).
                {
                  A = list1(B);
                }
f_block_kwarg(A) ::= f_block_kwarg(B) COMMA f_block_kw(C).
                {
                  A = push(B, C);
                }

f_kwrest(A) ::= kwrest_mark IDENTIFIER(B).
                {
                  local_add_f(p, B);
                  A = literal(B);
                }
f_kwrest(A) ::= kwrest_mark.
                {
                  A = literal("**");
                }

kwrest_mark ::= POW.
kwrest_mark ::= DSTAR.

block_args_tail(A) ::= f_block_kwarg(B) COMMA f_kwrest(C) opt_f_block_arg(D).
                      {
                        A = new_args_tail(p, B, C, D);
                      }
block_args_tail(A) ::= f_block_kwarg(B) opt_f_block_arg(D).
                      {
                        A = new_args_tail(p, B, 0, D);
                      }
block_args_tail(A) ::= f_kwrest(C) opt_f_block_arg(D).
                      {
                        A = new_args_tail(p, 0, C, D);
                      }
block_args_tail(A) ::= f_block_arg(D).
                      {
                        A = new_args_tail(p, 0, 0, D);
                      }

opt_block_args_tail(A) ::= COMMA block_args_tail(B).
                      {
                        A = B;
                      }
opt_block_args_tail(A) ::= none.
                      {
                        A = new_args_tail(p, 0, 0, 0);
                      }

block_param(A) ::= f_arg(B) COMMA f_block_optarg(C) COMMA f_rest_arg(D) opt_block_args_tail(F).
                    {
                      A = new_args(p, B, C, D, 0, F);
                    }
block_param(A) ::= f_arg(B) COMMA f_block_optarg(C) COMMA f_rest_arg(D) COMMA f_arg(E) opt_block_args_tail(F).
                    {
                      A = new_args(p, B, C, D, E, F);
                    }
block_param(A) ::= f_arg(B) COMMA f_block_optarg(C) COMMA opt_block_args_tail(F).
                    {
                      A = new_args(p, B, C, 0, 0, F);
                    }
block_param(A) ::= f_arg(B) COMMA f_block_optarg(C) COMMA f_arg(E) opt_block_args_tail(F).
                    {
                      A = new_args(p, B, C, 0, E, F);
                    }
block_param(A) ::= f_arg(B) COMMA f_rest_arg(D) opt_block_args_tail(F).
                    {
                      A = new_args(p, B, 0, D, 0, F);
                    }
block_param(A) ::= f_arg(B) COMMA opt_block_args_tail(F).
                    {
                      A = new_args(p, B, 0, 0, 0, F);
                    }
block_param(A) ::= f_arg(B) COMMA f_rest_arg(D) COMMA f_arg(E) opt_block_args_tail(F).
                    {
                      A = new_args(p, B, 0, D, E, F);
                    }
block_param(A) ::= f_arg(B) opt_block_args_tail(F).
                    {
                      A = new_args(p, B, 0, 0, 0, F);
                    }
block_param(A) ::= f_block_optarg(C) COMMA f_rest_arg(D) COMMA f_arg(E) opt_block_args_tail(F).
                    {
                      A = new_args(p, 0, C, D, E, F);
                    }
block_param(A) ::= f_block_optarg(C) opt_block_args_tail(F).
                    {
                      A = new_args(p, 0, C, 0, 0, F);
                    }
block_param(A) ::= f_block_optarg(C) COMMA f_arg(E) opt_block_args_tail(F).
                    {
                      A = new_args(p, 0, C, 0, E, F);
                    }
block_param(A) ::= f_rest_arg(D) opt_block_args_tail(F).
                    {
                      A = new_args(p, 0, 0, D, 0, F);
                    }
block_param(A) ::= f_rest_arg(D) COMMA f_arg(E) opt_block_args_tail(F).
                    {
                      A = new_args(p, 0, 0, D, E, F);
                    }
block_param(A) ::= block_args_tail(F).
                    {
                      A = new_args(p, 0, 0, 0, 0, F);
                    }
opt_block_param(A)  ::= none. {
                          local_add_blk(p, 0);
                          A = 0;
                        }
opt_block_param(A)  ::= block_param_def(B). {
                          p->cmd_start = true;
                          A = B;
                        }

block_param_def(A)  ::= OR opt_bv_decl OR. {
                          A = 0;
                        }
block_param_def(A)  ::= OR block_param(B) opt_bv_decl OR. {
                          A = B;
                        }

opt_bv_decl(A) ::= opt_nl. { A = 0; }
opt_bv_decl(A) ::= opt_nl SEMICOLON bv_decls opt_nl. { A = 0; }

bv_decls ::= bvar.
bv_decls ::= bv_decls COMMA bvar.

bvar ::= IDENTIFIER(B). {
           local_add_f(p, B);
           //new_bv(p, B);
         }

f_larglist(A) ::= LPAREN_EXPR f_args(B) opt_bv_decl RPAREN.
                  {
                    A = B;
                  }
f_larglist(A) ::= f_args(B).
                  {
                    A = B;
                  }

f_block_opt(A) ::= f_opt_asgn(B) primary_value(C).
                  {
                    A = push(B, C);
                  }
f_block_optarg(A) ::= f_block_opt(B).
                  {
                    A = new_first_arg(p, B);
                  }
f_block_optarg(A) ::= f_block_optarg(B) COMMA f_block_opt(C).
                  {
                    A = list3(atom(ATOM_args_add), B, C);
                  }

lambda_body(A) ::= LAMBEG compstmt(B) RBRACE.
                  {
                    A = B;
                  }
lambda_body(A) ::= KW_do_lambda bodystmt(B) KW_end.
                  {
                    A = B;
                  }

scope_nest_KW_do_block ::= KW_do_block. { scope_nest(p, false); }
do_block(A) ::= scope_nest_KW_do_block
                opt_block_param(B)
                bodystmt(C)
                KW_end. {
                  A = new_block(p, B, C);
                  scope_unnest(p);
                }

block_call(A) ::= command(B) do_block(C). {
                    call_with_block(p, B, C);
                    A = B;
                  }

method_call(A)  ::= operation(B) paren_args(C). {
                      A = new_fcall(p, B, C);
                    }
method_call(A)  ::= primary_value(B) call_op(C) operation2(D) opt_paren_args(E). {
                      A = new_call(p, B, D, E, C);
                    }
method_call(A)  ::= primary_value(B) call_op(C) paren_args(D).
                    {
                      A = new_call(p, B, "call", D, C);
                    }
method_call(A)  ::= KW_super paren_args(B).
                    {
                      A = new_super(p, B);
                    }
method_call(A)  ::= KW_super.
                    {
                      A = new_zsuper(p);
                    }
method_call(A)  ::= primary_value(B) LBRACKET opt_call_args(C) RBRACKET. {
                      A = new_call(p, B, STRING_ARY, C, '.');
                    }

scope_nest_brace ::= LBRACE_BLOCK_PRIMARY. { scope_nest(p, false); }
scope_nest_KW_do ::= KW_do. { scope_nest(p, false); }
brace_block(A) ::= scope_nest_brace
                   opt_block_param(B)
                   bodystmt(C)
                   RBRACE. {
                     A = new_block(p, B, C);
                     scope_unnest(p);
                   }
brace_block(A) ::= scope_nest_KW_do
                   opt_block_param(B)
                   bodystmt(C)
                   KW_end. {
                     A = new_block(p, B, C);
                     scope_unnest(p);
                   }

call_op(A) ::= PERIOD. { A = '.'; }
call_op(A) ::= ANDDOT. { A = 0; }

opt_paren_args ::= none.
opt_paren_args ::= paren_args.

paren_args(A) ::= LPAREN_EXPR opt_call_args(B) rparen. { A = B; }

opt_call_args ::= none.
opt_call_args ::= call_args opt_terms.
opt_call_args(A) ::= args(B) comma.
                {
                  A = list2(B, 0);
                }
opt_call_args(A) ::= args(B) comma assocs(C) comma.
                {
                  A = new_callargs(p, B, new_kw_hash(p, C), 0);
                }
opt_call_args(A) ::= assocs(B) comma.
                {
                  A = new_callargs(p, 0, new_kw_hash(p, B), 0);
                }

call_args(A) ::= command(B).
                {
                  A = new_callargs(p, new_first_arg(p, B), 0, 0);
                }
call_args(A) ::= args(B) opt_block_arg(D).
                {
                  A = new_callargs(p, B, 0, D);
                }
call_args(A) ::= assocs(C) opt_block_arg(D).
                {
                  A = new_callargs(p, 0, new_kw_hash(p, C), D);
                }
call_args(A) ::= args(B) comma assocs(C) opt_block_arg(D).
                {
                  A = new_callargs(p, B, new_kw_hash(p, C), D);
                }
call_args(A) ::= block_arg(D).
                {
                  A = new_callargs(p, 0, 0, D);
                }

case_body(A) ::= KW_when args(B) then
                 compstmt(C)
                 cases(D).
                {
                  A = append(list2(B, C), D);
                }
cases(A) ::= opt_else(B). {
               if (B) {
                 A = list1(B);
               } else {
                 A = 0;
               }
             }
cases ::= case_body.

opt_rescue(A) ::= KW_rescue exc_list(B) exc_var(C) then
                  compstmt(D)
                  opt_rescue(E).
                {
                  if (!B) B = new_first_arg(p, new_const(p, "StandardError"));
                  A = list3(
                    list2(atom(ATOM_exc_list), B),
                    list2(atom(ATOM_exc_var), C),
                    D
                  );
                  if (E) A = append(A, E);
                }
opt_rescue ::= none.

exc_list(A) ::= arg(B).
                {
                  A = new_first_arg(p, B);
                }
exc_list    ::= mrhs.
exc_list    ::= none.

exc_var(A) ::= ASSOC lhs(B).
                {
                  A = new_exc_var(p, B);
                }
exc_var    ::= none.

opt_ensure(A) ::= KW_ensure compstmt(B).
                {
                  A = B;
                }
opt_ensure    ::= none.

f_norm_arg(A) ::= IDENTIFIER(B). {
                    local_add_f(p, B);
                    A = B;
                  }
f_arg_item(A) ::= f_norm_arg(B). {
                    A = new_arg(p, B);
                  }

f_arg(A) ::= f_arg_item(B). {
              A = new_first_arg(p, B);
            }
f_arg(A) ::= f_arg(B) COMMA f_arg_item(C). {
              A = list3(atom(ATOM_args_add), B, C);
            }

literal ::= numeric.
literal ::= symbol.
literal ::= words.
literal ::= symbols.

words(A) ::= WORDS_BEG opt_sep word_list(B) opt_sep. { A = new_array(p, B); }
word_list(A) ::= word(B). { A = new_first_arg(p, B); }
word_list(A) ::= word_list(B) opt_sep word(C). { A = list3(atom(ATOM_args_add), B, C); }
word(A) ::= STRING(B). { A = new_str(p, B); }

symbols(A) ::= SYMBOLS_BEG opt_sep symbol_list(B) opt_sep. { A = new_array(p, B); }
symbol_list(A) ::= symbol_word(B). { A = new_first_arg(p, B); }
symbol_list(A) ::= symbol_list(B) opt_sep symbol_word(C). { A = list3(atom(ATOM_args_add), B, C); }
symbol_word(A) ::= STRING(B). { A = new_sym(p, B); }

opt_sep ::= none.
opt_sep ::= WORDS_SEP.
opt_sep ::= opt_sep WORDS_SEP.

superclass(A) ::= . { A = 0; }
superclass_head ::= LT. {
                  //p->state = EXPR_BEG;
                  p->cmd_start = true;
                }
superclass(A) ::= superclass_head expr_value(B) term. {
                    A = B;
                  }

numeric(A) ::= INTEGER(B). { A = new_lit(p, B, ATOM_at_int); }
numeric(A) ::= FLOAT(B).   { A = new_lit(p, B, ATOM_at_float); }
numeric(A) ::= UMINUS_NUM INTEGER(B). [LOWEST] { A = new_neglit(p, B, ATOM_at_int); }
numeric(A) ::= UMINUS_NUM FLOAT(B). [LOWEST]   { A = new_neglit(p, B, ATOM_at_float); }

variable(A) ::= IDENTIFIER(B). { A = new_lvar(p, B); }
variable(A) ::= IVAR(B).       { A = new_ivar(p, B); }
variable(A) ::= GVAR(B).       { A = new_gvar(p, B); }
variable(A) ::= CONSTANT(B).   { A = new_const(p, B); }

var_ref(A) ::= variable(B). { A = var_reference(p, B); }
var_ref(A) ::= KW_nil. { A = new_nil(p); }
var_ref(A) ::= KW_self. { A = new_self(p); }
var_ref(A) ::= KW_true. { A = list1(atom(ATOM_kw_true)); }
var_ref(A) ::= KW_false. { A = list1(atom(ATOM_kw_false)); }

symbol(A) ::= basic_symbol(B). { A = new_sym(p, B); }
symbol(A) ::= SYMBEG STRING_BEG STRING(C). {
                A = new_sym(p, C);
              }
symbol(A) ::= SYMBEG STRING_BEG string_rep(B) STRING(C). {
                A = new_dsym(
                  p, new_dstr(p, list3(atom(ATOM_dstr_add), B, new_str(p, C)))
                );
              }

basic_symbol(A) ::= SYMBEG sym(B). { A = B; }

sym ::= fname.

fname ::= IDENTIFIER.
fname ::= CONSTANT.
fname ::= FID.

fsym ::= fname.
fsym ::= basic_symbol.

f_arglist_paren(A) ::= LPAREN_EXPR f_args(B) rparen. {
                         A = B;
//                         p->state = EXPR_BEG;
                         p->cmd_start = true;
                        }

f_arglist     ::= f_arglist_paren.
f_arglist(A)  ::= f_args(B) term. {
                    A = B;
                  }

f_args(A) ::= f_arg(B) COMMA f_optarg(C) COMMA f_rest_arg(D) opt_args_tail(E).
              {
                A = new_args(p, B, C, D, 0, E);
              }
f_args(A) ::= f_arg(B) COMMA f_optarg(C) COMMA f_rest_arg(D) COMMA f_arg(E) opt_args_tail(F).
              {
                A = new_args(p, B, C, D, E, F);
              }
f_args(A) ::= f_arg(B) COMMA f_optarg(C) opt_args_tail(D).
              {
                A = new_args(p, B, C, 0, 0, D);
              }
f_args(A) ::= f_arg(B) COMMA f_optarg(C) COMMA f_arg(D) opt_args_tail(E).
              {
                A = new_args(p, B, C, 0, D, E);
              }
f_args(A) ::= f_arg(B) COMMA f_rest_arg(C) opt_args_tail(D).
              {
                A = new_args(p, B, 0, C, 0, D);
              }
f_args(A) ::= f_arg(B) COMMA f_rest_arg(C) COMMA f_arg(D) opt_args_tail(E).
              {
                A = new_args(p, B, 0, C, D, E);
              }
f_args(A) ::= f_arg(B) opt_args_tail(C).
              {
                A = new_args(p, B, 0, 0, 0, C);
              }
f_args(A) ::= f_optarg(B) COMMA f_rest_arg(C) opt_args_tail(D).
              {
                A = new_args(p, 0, B, C, 0, D);
              }
f_args(A) ::= f_optarg(B) COMMA f_rest_arg(C) COMMA f_arg(D) opt_args_tail(E).
              {
                A = new_args(p, 0, B, C, D, E);
              }
f_args(A) ::= f_optarg(B) opt_args_tail(C).
              {
                A = new_args(p, 0, B, 0, 0, C);
              }
f_args(A) ::= f_optarg(B) COMMA f_arg(C) opt_args_tail(D).
              {
                A = new_args(p, 0, B, 0, C, D);
              }
f_args(A) ::= f_rest_arg(B) opt_args_tail(C).
              {
                A = new_args(p, 0, 0, B, 0, C);
              }
f_args(A) ::= f_rest_arg(B) COMMA f_arg(C) opt_args_tail(D).
              {
                A = new_args(p, 0, 0, B, C, D);
              }
f_args(A) ::= args_tail(B).
              {
                A = new_args(p, 0, 0, 0, 0, B);
              }
f_args(A) ::= . {
                local_add_f(p, "&");
                A = new_args(p, 0, 0, 0, 0, 0);
              }

f_opt_asgn(A) ::= IDENTIFIER(B) E. {
                    local_add_f(p, B);
                    A = list2(atom(ATOM_assign_backpatch), new_lvar(p, B));
                  }

f_opt(A) ::= f_opt_asgn(B) arg(C). {
               A = push(B, C);
             }

f_optarg(A) ::= f_opt(B). {
                  A = new_first_arg(p, B);
                }
f_optarg(A) ::= f_optarg(B) COMMA f_opt(C). {
                  A = list3(atom(ATOM_args_add), B, C);
                }

restarg_mark ::= STAR.

f_rest_arg(A) ::= restarg_mark IDENTIFIER(B). {
  local_add_f(p, B);
  A = literal(B);
}
f_rest_arg(A) ::= restarg_mark. {
  local_add_f(p, "*");
  A = literal("*");
}

opt_f_block_arg(A) ::= COMMA f_block_arg(B).
              {
                A = B;
              }
opt_f_block_arg(A) ::= none.
              {
                A = 0;
              }

f_kw(A) ::= f_label(B) arg(C).
              {
                A = new_kw_arg(p, B, C);
              }
f_kw(A) ::= f_label(B).
              {
                A = new_kw_arg(p, B, 0);
              }

f_kwarg(A) ::= f_kw(B).
              {
                A = list1(B);
              }
f_kwarg(A) ::= f_kwarg(B) COMMA f_kw(C).
              {
                A = push(B, C);
              }

args_tail(A) ::= f_kwarg(B) COMMA f_kwrest(C) opt_f_block_arg(D).
              {
                A = new_args_tail(p, B, C, D);
              }
args_tail(A) ::= f_kwarg(B) opt_f_block_arg(D).
              {
                A = new_args_tail(p, B, 0, D);
              }
args_tail(A) ::= f_kwrest(C) opt_f_block_arg(D).
              {
                A = new_args_tail(p, 0, C, D);
              }
args_tail(A) ::= f_block_arg(D).
              {
                A = new_args_tail(p, 0, 0, D);
              }

blkarg_mark ::= AMPER.

f_block_arg(A) ::= blkarg_mark IDENTIFIER(B). {
                    A = B;
                  }

opt_args_tail(A) ::= COMMA args_tail(B). {
                      A = B;
                    }
opt_args_tail(A) ::= . {
                      A = new_args_tail(p, 0, 0, 0);
                    }

string ::= string_fragment.
string(A) ::= string(B) string_fragment(C). { A = concat_string(p, B, C); }

string_fragment(A) ::= STRING(B). { A = new_str(p, B); }
string_fragment(A) ::= STRING_BEG STRING(B). { A = new_str(p, B); }
string_fragment(A) ::= STRING_BEG string_rep(B) STRING(C).
  { A = new_dstr(p, list3(atom(ATOM_dstr_add), B, new_str(p, C))); }

string_rep ::= string_interp.
string_rep(A) ::= string_rep(B) string_interp(C).
  { A = list3(atom(ATOM_dstr_add), B, C); }

string_interp(A) ::= DSTRING_TOP(B). { A = list3(atom(ATOM_dstr_add), list1(atom(ATOM_dstr_new)), new_str(p, B)); }
string_interp(A) ::= DSTRING_MID(B). { A = new_str(p, B); }
string_interp(A) ::= DSTRING_BEG compstmt(B) DSTRING_END.
  { A = B; }

operation(A) ::= IDENTIFIER(B). { A = list2(atom(ATOM_at_ident), literal(B)); }
operation(A) ::= CONSTANT(B). { A = list2(atom(ATOM_at_const), literal(B)); }
operation ::= FID.

operation2 ::= IDENTIFIER.
operation2 ::= CONSTANT.
operation2 ::= FID.

opt_nl ::= .
opt_nl ::= nl.

rparen ::= opt_terms RPAREN.

opt_terms ::= .
opt_terms ::= terms.

trailer ::= .
trailer ::= terms.
trailer ::= comma.

term ::= SEMICOLON.
term ::= nl.
term ::= COMMENT.

nl ::= NL.

terms ::= term.
terms ::= terms term.

none(A) ::= . { A = 0; }

%code {

  StringPool *stringPool_new(StringPool *prev, uint16_t size)
  {
    StringPool *pool = (StringPool *)LEMON_ALLOC(STRING_POOL_HEADER_SIZE + size);
    pool->prev = prev;
    pool->size = size;
    pool->index = 0;
    return pool;
  }

  const char *findFromStringPool(ParserState *p, char *s)
  {
    if (s[0] == '\0') return STRING_NULL;
    if (s[0] == '-' && s[1] == '\0') return STRING_NEG;
    if (strcmp(s, "[]") == 0) return STRING_ARY;
    StringPool *pool = p->current_string_pool;
    uint16_t index;
    while (pool) {
      index = 0;
      for (;;) {
        if (strcmp(s, &pool->strings[index]) == 0)
          return &pool->strings[index];
        while (index < pool->index) {
          if (pool->strings[index] == '\0') break;
          index++;
        }
        index++;
        if (index >= pool->index) break;
      }
      pool = pool->prev;
    }
    return NULL;
  }

  const char *ParsePushStringPool(ParserState *p, char *s)
  {
    const char *result = findFromStringPool(p, s);
    if (result) return result;
    size_t length = strlen(s) + 1; /* including \0 */
    StringPool *pool = p->current_string_pool;
    if (pool->size < pool->index + length) {
      if (length > STRING_POOL_POOL_SIZE) {
        p->current_string_pool = stringPool_new(pool, length);
      } else {
        p->current_string_pool = stringPool_new(pool, STRING_POOL_POOL_SIZE);
      }
      pool = p->current_string_pool;
    }
    uint16_t index = pool->index;
    strcpy((char *)&pool->strings[index], s);
    pool->index += length; /* position of the next insertion */
    return (const char *)&pool->strings[index];
  }

  ParserState *ParseInitState(uint8_t node_box_size)
  {
    ParserState *p = LEMON_ALLOC(sizeof(ParserState));
    p->node_box_size = node_box_size;
    p->scope = Scope_new(NULL, true);
    p->current_node_box = NULL;
    p->root_node_box = Node_newBox(p);
    p->current_node_box = p->root_node_box;
    p->current_string_pool = stringPool_new(NULL, STRING_POOL_POOL_SIZE);
    strcpy(STRING_NULL, "");
    strcpy(STRING_NEG,  "-");
    strcpy(STRING_ARY, "[]");
    p->error_count = 0;
    p->cond_stack = 0;
    p->cmdarg_stack = 0;
    p->paren_stack_num = -1;
    p->lpar_beg = 0;
    p->in_def = 0;
    p->in_single = 0;
    return p;
  }

  void ParserStateFree(ParserState *p) {
    Scope *scope = p->scope;
    /* trace back to the up most scope in case of syntax error */
    while (1) {
      if (scope->upper == NULL) break;
      scope = scope->upper;
    }
    Scope_freeCodePool(scope);
    StringPool *prev_pool;
    while (p->current_string_pool) {
      prev_pool = p->current_string_pool->prev;
      LEMON_FREE(p->current_string_pool);
      p->current_string_pool = prev_pool;
    }
    Scope_free(scope);
    LEMON_FREE(p);
  }

  void freeNode(Node *n) {
    if (n == NULL) return;
    if (n->type == CONS) {
      freeNode(n->cons.car);
      freeNode(n->cons.cdr);
    } else if (n->type == LITERAL) {
      LEMON_FREE((char *)n->value.name);
    }
    LEMON_FREE(n);
  }

  void ParseFreeAllNode(yyParser *yyp) {
    Node_freeAllNode(yyp->p->root_node_box);
  }

  void showNode1(Node *n, bool isCar, int indent, bool isRightMost) {
    static int lb = 0;
    if (n == NULL) {
      if (lb > 0) {
        printf("\n");
        lb = 0;
        for (int i=0; i<indent; i++) printf("  ");
      }
      printf(" _");
      return;
    }
    switch (n->type) {
      case CONS:
        if (isCar) {
          printf("\n");
          lb = 0;
          for (int i=0; i<indent; i++) printf("  ");
          printf("[");
        } else {
          printf(",");
          lb++;
        }
        if (n->cons.car && n->cons.car->type != CONS && n->cons.cdr == NULL) {
          isRightMost = true;
        }
        break;
      case ATOM:
        printf("%s", atom_name(n->atom.type));
        if (isRightMost) {
          printf("]");
        }
        break;
      case LITERAL:
        printf(" \e[31;1m\"%s\"\e[m", Node_valueName(n));
        if (isRightMost) {
          printf("]");
        }
        break;
    }
    if (n->type == CONS) {
      showNode1(n->cons.car, true, indent+1, isRightMost);
      showNode1(n->cons.cdr, false, indent, isRightMost);
    }
  }

  void showNode2(Node *n) {
    if (n == NULL) return;
    switch (n->type) {
      case ATOM:
        printf("    atom:%p", n);
        printf("  value:%d\n", n->atom.type);
        break;
      case LITERAL:
        printf("    literal:%p", n);
        printf("  name:\"%s\"\n", Node_valueName(n));
        break;
      case CONS:
        printf("cons:%p\n", n);
        printf(" car:%p\n", n->cons.car);
        printf(" cdr:%p\n", n->cons.cdr);
        showNode2(n->cons.car);
        showNode2(n->cons.cdr);
    }
  }

  void ParseShowAllNode(yyParser *yyp, int way) {
    if (way == 1) {
      showNode1(yyp->p->root_node_box->nodes, true, 0, false);
    } else if (way == 2) {
      showNode2(yyp->p->root_node_box->nodes);
    }
    printf("\n");
  }

  const char *kind(Node *n){
    const char *type = NULL;
    switch (n->type) {
      case ATOM:
        type = "a";
        break;
      case LITERAL:
        type = "l";
        break;
      case CONS:
        type = "c";
        break;
    }
    return type;
  }

  int atom_type(Node *n) {
    if (n->type != ATOM) {
      return ATOM_NONE;
    }
    return n->atom.type;
  }

}
