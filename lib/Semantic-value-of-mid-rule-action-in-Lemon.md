# Semantic value of mid-rule action in Lemon

Unlike Yacc/Bison, Lemon doesn't have the mid-rule action (MRA) feature that allows you to write an action anywhere in the middle of a right-hand side (RHS).

## Case 1 - Basic

Let's say you are defining a grammar rule for Ruby's begin-end expression:

```ruby
begin
  do_something               # bodystmt
end
```

```c
/* Yacc/Bison */
primary : keyword_begin      /* $1 */
            {
              $<stack>$ = p->cmdarg_stack;
              p->cmdarg_stack = 0;
            }                /* $2 👀 */
          bodystmt           /* $3 */
          keyword_end        /* $4 */
            {
              p->cmdarg_stack = $<stack>2;
              $$ = $3;
            }
```

The MRA preserves a value of `p->cmdarg_stack` as a variable named `$<stack>$` because `p->cmdarg_stack` should be reset to zero so that `bodystmt` can begin a new statement.
Eventually, at the finalizing action of the rule, `p->cmdarg_stack` is restored by referring `$<stack>2` variable.
The last digit `2` of the variable is the semantic value number `2` as the MRA has its own semantic value number in Yacc/Bison.

In Lemon, you can write the equivalent rule as follows:

```c
/* Lemon */
primary(A) ::= keyword_begin
               preserve_cmdarg_stach(STACK)
               bodystmt(B)
               keyword_end.
            {
              p->cmdarg_stack = STACK;
              A = B;
            }
preserve_cmdarg_stach(STACK) ::= .
            {
              STACK = p->cmdarg_stack;
              p->cmdarg_stack = 0;
            }
```

Preserving `p->cmdarg_stack` can be written in `preserve_cmdarg_stach` with an empty RHS.
Actually, MRA of Yacc/Bison is just a syntax sugar of this.

## Case 2 - Semantic value right after a terminal

You are trying to write another rule for "arrow-style" lambda this time:

```ruby
-> (arg) { arg.do_someting }
```

In Yacc/Bison, you can write the rule like this:

```c
/* Yacc/Bison */
primary : tLAMBDA            /* $1 */
            {
              local_nest(p);
              nvars_nest(p);
              $<num>$ = p->lpar_beg;
              p->lpar_beg = ++p->paren_nest;
            }                /* $2 */
          f_larglist         /* $3 */
            {
              $<stack>$ = p->cmdarg_stack;
              p->cmdarg_stack = 0;
            }                /* $4 */
          lambda_body        /* $5 */
            {
              p->lpar_beg = $<num>2;
              $$ = new_lambda(p, $3, $5);
              local_unnest(p);
              nvars_unnest(p);
              p->cmdarg_stack = $<stack>4;
              CMDARG_LEXPOP();
            }
```

```c
/* Lemon */
primary(A) ::= lambda_head(NUM) LAMBDA f_larglist(B) preserve_cmdarg_stach(STACK) lambda_body(C).
            /* ^^^^^^^^^^^^^^^^^^^^^^^👀 */
            {
              p->lpar_beg = NUM;
              A = new_lambda(p, B, C);
              local_unnest(p);
              nvars_unnest(p);
              p->cmdarg_stack = STACK;
              CMDARG_LEXPOP();
            }
lambda_head(NUM) ::= .
            {
              local_nest(p);
              nvars_nest(p);
              NUM = p->lpar_beg;
              p->lpar_beg = ++p->paren_nest;
            }
preserve_cmdarg_stach(STACK) ::= .
            {
              STACK = p->cmdarg_stack;
              p->cmdarg_stack = 0;
            }
```

There is only one thing that you should pay attention, although this rule is intricate.
It is that, in Lemon, `lambda_head(NUM)` is located **before** `LAMBDA`.
But why?

Remember that Lemon is an LALR(1) parser.
A **Look-Ahead** LR parser looks ahead one more token before reduction.

If you write `LAMBDA lambda_head(NUM)` in the opposite order, there are two scenarios that don't work:

1. `-> (arg) { arg.do_something }`  
In this scenario, the action of `lambda_head` will happen after inputting `(` (left paren of `(args)`).
This causes inconsistency of `p->paren_nest` status at the point that our tokenizer finds a token `{` which starts `lambda_body`.

2. `-> { do_something }`  
In this scenario, invoking the action of `lambda_head` is delayed until inputting a token `{`.
This is neither what you expect because preserving `NUM = p->lpar_beg;` should happen before the tokenizer finds `{`.

The difference between Case 1 and 2 is what kind of effect you expect from MRA.
If you want the MRA to change the parser status of which the tokenizer takes advantage, you may have to put the MRA by taking the look-ahead behavior into consideration.

## Conclusion so far

Things are so timing-dependant that you have to be careful to use semantic values of MRA in Lemon.
The conclusion is that it would be better to see debug prints😅

----

This document likely contains some wrong points. Please tell twitter.com/hasumikin if you noticed something.
