/*! @file
  @brief
  Define operation codes and associated macros.

  <pre>
  Copyright (C) 2015-2020 Kyushu Institute of Technology.
  Copyright (C) 2015-2020 Shimane IT Open-Innovation Center.

  This file is distributed under BSD 3-Clause License.


  </pre>
*/

#ifndef MRBC_SRC_OPCODE_H_
#define MRBC_SRC_OPCODE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define FETCH_Z()
#define FETCH_B()		\
  uint32_t a = vm->inst[0];	\
  vm->inst += 1;		\
  (void)a

#define FETCH_BB()		\
  uint32_t a = vm->inst[0];	\
  uint16_t b = vm->inst[1];	\
  vm->inst += 2;		\
  (void)a, (void)b

#define FETCH_BBB()		\
  uint32_t a = vm->inst[0];	\
  uint16_t b = vm->inst[1];	\
  uint16_t c = vm->inst[2];	\
  vm->inst += 3;		\
  (void)a, (void)b, (void)c

#define FETCH_BS()						\
  uint32_t a = vm->inst[0];					\
  uint16_t b = (uint16_t)vm->inst[1] << 8 | vm->inst[2];	\
  vm->inst += 3;						\
  (void)a, (void)b

#define FETCH_BSS()						\
  uint32_t a = vm->inst[0];					\
  uint16_t b = (uint16_t)vm->inst[1] << 8 | vm->inst[2];	\
  uint16_t c = (uint16_t)vm->inst[3] << 8 | vm->inst[4];	\
  vm->inst += 5;						\
  (void)a, (void)b, (void)c

#define FETCH_S()						\
  uint32_t a = (uint32_t)vm->inst[0] << 8 | vm->inst[1];	\
  vm->inst += 2;						\
  (void)a

#define FETCH_W()						\
  uint32_t a = vm->inst[0];					\
  a = (a << 8) | vm->inst[1];					\
  a = (a << 8) | vm->inst[2];					\
  vm->inst += 3;						\
  (void)a


//================================================================
/*!@brief
  Operation codes.

  operand types:
   Z:   no operand
   B:   8bit     (a)
   BB:  8+8bit   (a,b)
   BBB: 8+8+8bit (a,b,c)
   BS:  8+16bit  (a,b)
   S:   16bit    (a)
   W:   24bit    (a)
*/
enum OPCODE {
/*-----------------------------------------------------------------------
  operation code        operands  semantics
------------------------------------------------------------------------*/
  OP_NOP        = 0x00, //!< Z    no operation
  OP_MOVE       = 0x01, //!< BB   R(a) = R(b)
  OP_LOADL      = 0x02, //!< BB   R(a) = Pool(b)
  OP_LOADL16    = 0x03, //!< BS   R(a) = Pool(b)
  OP_LOADI      = 0x04, //!< BB   R(a) = mrb_int(b)
  OP_LOADINEG   = 0x05, //!< BB   R(a) = mrb_int(-b)
  OP_LOADI__1   = 0x06, //!< B    R(a) = mrb_int(-1)
  OP_LOADI_0    = 0x07, //!< B    R(a) = mrb_int(0)
  OP_LOADI_1    = 0x08, //!< B    R(a) = mrb_int(1)
  OP_LOADI_2    = 0x09, //!< B    R(a) = mrb_int(2)
  OP_LOADI_3    = 0x0a, //!< B    R(a) = mrb_int(3)
  OP_LOADI_4    = 0x0b, //!< B    R(a) = mrb_int(4)
  OP_LOADI_5    = 0x0c, //!< B    R(a) = mrb_int(5)
  OP_LOADI_6    = 0x0d, //!< B    R(a) = mrb_int(6)
  OP_LOADI_7    = 0x0e, //!< B    R(a) = mrb_int(7)
  OP_LOADI16    = 0x0f, //!< BS   R(a) = mrb_int(b)
  OP_LOADI32    = 0x10, //!< BSS  R(a) = mrb_int((b<<16)+c)
  OP_LOADSYM    = 0x11, //!< BB   R(a) = Syms(b)
  OP_LOADSYM16  = 0x12, //!< BS   R(a) = Syms(b)
  OP_LOADNIL    = 0x13, //!< B    R(a) = nil
  OP_LOADSELF   = 0x14, //!< B    R(a) = self
  OP_LOADT      = 0x15, //!< B    R(a) = true
  OP_LOADF      = 0x16, //!< B    R(a) = false
  OP_GETGV      = 0x17, //!< BB   R(a) = getglobal(Syms(b))
  OP_SETGV      = 0x18, //!< BB   setglobal(Syms(b), R(a))
  OP_GETSV      = 0x19, //!< BB   R(a) = Special[Syms(b)]
  OP_SETSV      = 0x1a, //!< BB   Special[Syms(b)] = R(a)
  OP_GETIV      = 0x1b, //!< BB   R(a) = ivget(Syms(b))
  OP_SETIV      = 0x1c, //!< BB   ivset(Syms(b),R(a))
  OP_GETCV      = 0x1d, //!< BB   R(a) = cvget(Syms(b))
  OP_SETCV      = 0x1e, //!< BB   cvset(Syms(b),R(a))
  OP_GETCONST   = 0x1f, //!< BB   R(a) = constget(Syms(b))
  OP_SETCONST   = 0x20, //!< BB   constset(Syms(b),R(a))
  OP_GETMCNST   = 0x21, //!< BB   R(a) = R(a)::Syms(b)
  OP_SETMCNST   = 0x22, //!< BB   R(a+1)::Syms(b) = R(a)
  OP_GETUPVAR   = 0x23, //!< BBB  R(a) = uvget(b,c)
  OP_SETUPVAR   = 0x24, //!< BBB  uvset(b,c,R(a))
  OP_JMP        = 0x25, //!< S    pc+=a
  OP_JMPIF      = 0x26, //!< BS   if R(a) pc+=b
  OP_JMPNOT     = 0x27, //!< BS   if !R(a) pc+=b
  OP_JMPNIL     = 0x28, //!< BS   if R(a)==nil pc+=b
  OP_JMPUW      = 0x29, //!< S    unwind_and_jump_to(a)
  OP_EXCEPT     = 0x2a, //!< B    R(a) = exc
  OP_RESCUE     = 0x2b, //!< BB   R(b) = R(a).isa?(R(b))
  OP_RAISEIF    = 0x2c, //!< B    raise(R(a)) if R(a)
  OP_SENDV      = 0x2d, //!< BB   R(a) = call(R(a),Syms(b),*R(a+1))
  OP_SENDVB     = 0x2e, //!< BB   R(a) = call(R(a),Syms(b),*R(a+1),&R(a+2))
  OP_SEND       = 0x2f, //!< BBB  R(a) = call(R(a),Syms(b),R(a+1),...,R(a+c))
  OP_SENDB      = 0x30, //!< BBB  R(a) = call(R(a),Syms(b),R(a+1),...,R(a+c),&R(a+c+1))
  OP_SENDVK     = 0x31, //!< BB   R(a) = call(R(a),Syms(b),*R(a+1),**(a+2),&R(a+3)) # todo
  OP_CALL       = 0x32, //!< Z    R(0) = self.call(frame.argc, frame.argv)
  OP_SUPER      = 0x33, //!< BB   R(a) = super(R(a+1),... ,R(a+b+1))
  OP_ARGARY     = 0x34, //!< BS   R(a) = argument array (16=m5:r1:m5:d1:lv4)
  OP_ENTER      = 0x35, //!< W    arg setup according to flags (23=m5:o5:r1:m5:k5:d1:b1)
  OP_KEY_P      = 0x36, //!< BB   R(a) = kdict.key?(Syms(b))
  OP_KEYEND     = 0x37, //!< Z    raise unless kdict.empty?
  OP_KARG       = 0x38, //!< BB   R(a) = kdict[Syms(b)]; kdict.delete(Syms(b))
  OP_RETURN     = 0x39, //!< B    return R(a) (normal)
  OP_RETURN_BLK = 0x3a, //!< B    return R(a) (in-block return)
  OP_BREAK      = 0x3b, //!< B    break R(a)
  OP_BLKPUSH    = 0x3c, //!< BS   R(a) = block (16=m5:r1:m5:d1:lv4)
  OP_ADD        = 0x3d, //!< B    R(a) = R(a)+R(a+1)
  OP_ADDI       = 0x3e, //!< BB   R(a) = R(a)+mrb_int(b)
  OP_SUB        = 0x3f, //!< B    R(a) = R(a)-R(a+1)
  OP_SUBI       = 0x40, //!< BB   R(a) = R(a)-mrb_int(b)
  OP_MUL        = 0x41, //!< B    R(a) = R(a)*R(a+1)
  OP_DIV        = 0x42, //!< B    R(a) = R(a)/R(a+1)
  OP_EQ         = 0x43, //!< B    R(a) = R(a)==R(a+1)
  OP_LT         = 0x44, //!< B    R(a) = R(a)<R(a+1)
  OP_LE         = 0x45, //!< B    R(a) = R(a)<=R(a+1)
  OP_GT         = 0x46, //!< B    R(a) = R(a)>R(a+1)
  OP_GE         = 0x47, //!< B    R(a) = R(a)>=R(a+1)
  OP_ARRAY      = 0x48, //!< BB   R(a) = ary_new(R(a),R(a+1)..R(a+b))
  OP_ARRAY2     = 0x49, //!< BBB  R(a) = ary_new(R(b),R(b+1)..R(b+c))
  OP_ARYCAT     = 0x4a, //!< B    ary_cat(R(a),R(a+1))
  OP_ARYPUSH    = 0x4b, //!< B    ary_push(R(a),R(a+1))
  OP_ARYDUP     = 0x4c, //!< B    R(a) = ary_dup(R(a))
  OP_AREF       = 0x4d, //!< BBB  R(a) = R(b)[c]
  OP_ASET       = 0x4e, //!< BBB  R(a)[c] = R(b)
  OP_APOST      = 0x4f, //!< BBB  *R(a),R(a+1)..R(a+c) = R(a)[b..]
  OP_INTERN     = 0x50, //!< B    R(a) = intern(R(a))
  OP_STRING     = 0x51, //!< BB   R(a) = str_dup(Lit(b))
  OP_STRING16   = 0x52, //!< BS   R(a) = str_dup(Lit(b))
  OP_STRCAT     = 0x53, //!< B    str_cat(R(a),R(a+1))
  OP_HASH       = 0x54, //!< BB   R(a) = hash_new(R(a),R(a+1)..R(a+b*2-1))
  OP_HASHADD    = 0x55, //!< BB   R(a) = hash_push(R(a),R(a+1)..R(a+b*2))
  OP_HASHCAT    = 0x56, //!< B    R(a) = hash_cat(R(a),R(a+1))
  OP_LAMBDA     = 0x57, //!< BB   R(a) = lambda(SEQ[b],L_LAMBDA)
  OP_LAMBDA16   = 0x58, //!< BS   R(a) = lambda(SEQ[b],L_LAMBDA)
  OP_BLOCK      = 0x59, //!< BB   R(a) = lambda(SEQ[b],L_BLOCK)
  OP_BLOCK16    = 0x5a, //!< BS   R(a) = lambda(SEQ[b],L_BLOCK)
  OP_METHOD     = 0x5b, //!< BB   R(a) = lambda(SEQ[b],L_METHOD)
  OP_METHOD16   = 0x5c, //!< BB   R(a) = lambda(SEQ[b],L_METHOD)
  OP_RANGE_INC  = 0x5d, //!< B    R(a) = range_new(R(a),R(a+1),FALSE)
  OP_RANGE_EXC  = 0x5e, //!< B    R(a) = range_new(R(a),R(a+1),TRUE)
  OP_OCLASS     = 0x5f, //!< B    R(a) = ::Object
  OP_CLASS      = 0x60, //!< BB   R(a) = newclass(R(a),Syms(b),R(a+1))
  OP_MODULE     = 0x61, //!< BB   R(a) = newmodule(R(a),Syms(b))
  OP_EXEC       = 0x62, //!< BB   R(a) = blockexec(R(a),SEQ[b])
  OP_EXEC16     = 0x63, //!< BB   R(a) = blockexec(R(a),SEQ[b])
  OP_DEF        = 0x64, //!< BB   R(a).newmethod(Syms(b),R(a+1))
  OP_ALIAS      = 0x65, //!< BB   alias_method(target_class,Syms(a),Syms(b))
  OP_UNDEF      = 0x66, //!< B    undef_method(target_class,Syms(a))
  OP_SCLASS     = 0x67, //!< B    R(a) = R(a).singleton_class
  OP_TCLASS     = 0x68, //!< B    R(a) = target_class
  OP_DEBUG      = 0x69, //!< BBB  print a,b,c
  OP_ERR        = 0x6a, //!< B    raise(LocalJumpError, Lit(a))
  OP_STOP       = 0x6b, //!< Z    stop VM

  OP_ABORT      = 0x6c, // only for mruby/c, TODO: remove
};

//================================================================
/*!@brief
  OP_RETURN parameter

*/
#define OP_R_NORMAL 0
#define OP_R_BREAK  1
#define OP_R_RETURN 2


#if defined(MRBC_LITTLE_ENDIAN)
#define MKOPCODE(op) (((uint32_t)(op) & 0x7f)<<24)
#define MKARG_A(x)   (((uint32_t)(x) & 0xff)<<1 | ((uint32_t)(x) & 0x01)>>8)
#define MKARG_B(x)   (((uint32_t)(x) & 0x1fc)<<6 | ((uint32_t)(x) & 0x03)<<22)
#define MKARG_C(x)   (((uint32_t)(x) & 0x7e)<<15 | ((uint32_t)(x) & 0x01)<<31)

#elif defined(MRBC_BIG_ENDIAN)
#define MKOPCODE(op) ((uint32_t)(op) & 0x7f)
#define MKARG_A(x)   ((uint32_t)(x) << 23)
#define MKARG_B(x)   ((uint32_t)(x) << 14)
#define MKARG_C(x)   ((uint32_t)(x) << 7)
#endif


#ifdef __cplusplus
}
#endif
#endif
