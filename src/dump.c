#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <value.h>
#include <common.h>
#include <dump.h>

#define PICORB_ASPEC_REQ(a)          (((a) >> 18) & 0x1f)
#define PICORB_ASPEC_OPT(a)          (((a) >> 13) & 0x1f)
#define PICORB_ASPEC_REST(a)         (((a) >> 12) & 0x1)
#define PICORB_ASPEC_POST(a)         (((a) >> 7) & 0x1f)
#define PICORB_ASPEC_KEY(a)          (((a) >> 2) & 0x1f)
#define PICORB_ASPEC_KDICT(a)        (((a) >> 1) & 0x1)
#define PICORB_ASPEC_BLOCK(a)        ((a) & 1)

#define PICOPEEK_B(irep) (*(irep))

#define PICOPEEK_S(irep) ((irep)[0]<<8|(irep)[1])
#define PICOPEEK_W(irep) ((irep)[0]<<16|(irep)[1]<<8|(irep)[2])

#define PICOREAD_B() PICOPEEK_B(irep++)
#define PICOREAD_S() (irep+=2, PICOPEEK_S(irep-2))
#define PICOREAD_W() (irep+=3, PICOPEEK_W(irep-3))

#define PICOFETCH_Z() /* nothing */
#define PICOFETCH_B()   do {a=PICOREAD_B();} while (0)
#define PICOFETCH_BB()  do {a=PICOREAD_B(); b=PICOREAD_B();} while (0)
#define PICOFETCH_BBB() do {a=PICOREAD_B(); b=PICOREAD_B(); c=PICOREAD_B();} while (0)
#define PICOFETCH_BS()  do {a=PICOREAD_B(); b=PICOREAD_S();} while (0)
#define PICOFETCH_BSS() do {a=PICOREAD_B(); b=PICOREAD_S(); c=PICOREAD_S();} while (0)
#define PICOFETCH_S()   do {a=PICOREAD_S();} while (0)
#define PICOFETCH_W()   do {a=PICOREAD_W();} while (0)

const char C_FORMAT_LINES[5][22] = {
  "#include <stdint.h>",
  "#ifdef __cplusplus",
  "extern const uint8_t ",
  "#endif",
  "const uint8_t ",
};

void
print_lv_a(void *dummy, uint8_t *irep, uint16_t a)
{
}
void
print_lv_ab(void *dummy, uint8_t *irep, uint16_t a, uint16_t b)
{
}

char*
pico_mrb_sym_dump(char *s, uint16_t b)
{
  sprintf(s, "%d", b);
  return s;
}

#define CASE(insn,ops) case insn: PICOFETCH_ ## ops ();

void
Dump_hexDump(FILE *fp, uint8_t *irep)
{
  char mrb[10];
  uint32_t len = 0;
  len += *(irep + 10) << 24;
  len += *(irep + 11) << 16;
  len += *(irep + 12) << 8;
  len += *(irep + 13);
  fprintf(fp, "pos: %d / len: %u\n", *irep, len);

  irep += 14; /* skip irep header */

  uint8_t *opstart = irep;

  uint8_t *irepend = irep + len;

  int i = 0; /*dummy*/

  while (irep < irepend) {
    fprintf(fp, "    1 %03d ", (int)(irep - opstart));
    uint32_t a;
    uint16_t b = 0;
    uint16_t c;
    uint8_t ins = *irep++;
    switch (ins) {
    CASE(OP_NOP, Z);
      fprintf(fp, "OP_NOP\n");
      return;
      break;
    CASE(OP_MOVE, BB);
      fprintf(fp, "OP_MOVE\tR%d\tR%d\t", a, b);
      print_lv_ab(mrb, irep, a, b);
      break;
    CASE(OP_LOADL16, BS);
      goto op_loadl;

    CASE(OP_LOADL, BB);
    op_loadl:
//      switch (irep->pool[b].tt) {
//      case IREP_TT_FLOAT:
//#ifndef PICORB_NO_FLOAT
//        fprintf(fp, "OP_LOADL\tR%d\tL(%d)\t; %f", a, b, (double)irep->pool[b].u.f);
//#endif
//        break;
//      case IREP_TT_INT32:
//        fprintf(fp, "OP_LOADL\tR%d\tL(%d)\t; %" PRId32, a, b, irep->pool[b].u.i32);
//        break;
//#ifdef PICORB_64BIT
//      case IREP_TT_INT64:
//        fprintf(fp, "OP_LOADL\tR%d\tL(%d)\t; %" PRId64, a, b, irep->pool[b].u.i64);
//        break;
//#endif
//      default:
        fprintf(fp, "OP_LOADL\tR%d\tL(%d)\t", a, b);
        break;
//      }
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADI, BB);
      fprintf(fp, "OP_LOADI\tR%d\t%d\t", a, b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADINEG, BB);
      fprintf(fp, "OP_LOADI\tR%d\t-%d\t", a, b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADI16, BS);
      fprintf(fp, "OP_LOADI16\tR%d\t%d\t", a, (int)(int16_t)b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADI32, BSS);
      fprintf(fp, "OP_LOADI32\tR%d\t%d\t", a, (int32_t)(((uint32_t)b<<16)+c));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADI__1, B);
      fprintf(fp, "OP_LOADI__1\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADI_0, B); goto L_LOADI;
    CASE(OP_LOADI_1, B); goto L_LOADI;
    CASE(OP_LOADI_2, B); goto L_LOADI;
    CASE(OP_LOADI_3, B); goto L_LOADI;
    CASE(OP_LOADI_4, B); goto L_LOADI;
    CASE(OP_LOADI_5, B); goto L_LOADI;
    CASE(OP_LOADI_6, B); goto L_LOADI;
    CASE(OP_LOADI_7, B);
    L_LOADI:
      fprintf(fp, "OP_LOADI_%d\tR%d\t\t", ins-(int)OP_LOADI_0, a);
      print_lv_a(mrb, irep, a);
      break;
//    CASE(OP_LOADSYM16, BS);
//      goto op_loadsym;
    CASE(OP_LOADSYM, BB);
//    op_loadsym:
      fprintf(fp, "OP_LOADSYM\tR%d\t:%s\t", a, pico_mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADNIL, B);
      fprintf(fp, "OP_LOADNIL\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADSELF, B);
      fprintf(fp, "OP_LOADSELF\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADT, B);
      fprintf(fp, "OP_LOADT\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LOADF, B);
      fprintf(fp, "OP_LOADF\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_GETGV, BB);
      fprintf(fp, "OP_GETGV\tR%d\t:%s", a, pico_mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SETGV, BB);
      fprintf(fp, "OP_SETGV\t:%s\tR%d", pico_mrb_sym_dump(mrb, b), a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_GETSV, BB);
      fprintf(fp, "OP_GETSV\tR%d\t:%s", a, pico_mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SETSV, BB);
      fprintf(fp, "OP_SETSV\t:%s\tR%d", pico_mrb_sym_dump(mrb, b), a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_GETCONST, BB);
      fprintf(fp, "OP_GETCONST\tR%d\t:%s", a, pico_mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SETCONST, BB);
      fprintf(fp, "OP_SETCONST\t:%s\tR%d", pico_mrb_sym_dump(mrb, b), a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_GETMCNST, BB);
      fprintf(fp, "OP_GETMCNST\tR%d\tR%d::%s", a, a, pico_mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SETMCNST, BB);
      fprintf(fp, "OP_SETMCNST\tR%d::%s\tR%d", a+1, pico_mrb_sym_dump(mrb, b), a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_GETIV, BB);
      fprintf(fp, "OP_GETIV\tR%d\t%s", a, pico_mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SETIV, BB);
      fprintf(fp, "OP_SETIV\t%s\tR%d", pico_mrb_sym_dump(mrb, b), a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_GETUPVAR, BBB);
      fprintf(fp, "OP_GETUPVAR\tR%d\t%d\t%d", a, b, c);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SETUPVAR, BBB);
      fprintf(fp, "OP_SETUPVAR\tR%d\t%d\t%d", a, b, c);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_GETCV, BB);
      fprintf(fp, "OP_GETCV\tR%d\t%s", a, pico_mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SETCV, BB);
      fprintf(fp, "OP_SETCV\t%s\tR%d", pico_mrb_sym_dump(mrb, b), a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_JMP, S);
//      i = pc - irep->iseq;
      fprintf(fp, "OP_JMP\t\t%03d\n", (int)i+(int16_t)a);
      break;
    CASE(OP_JMPUW, S);
////      i = pc - irep->iseq;
      fprintf(fp, "OP_JMPUW\t\t%03d\n", (int)i+(int16_t)a);
      break;
    CASE(OP_JMPIF, BS);
//      i = pc - irep->iseq;
      fprintf(fp, "OP_JMPIF\tR%d\t%03d\t", a, (int)i+(int16_t)b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_JMPNOT, BS);
//      i = pc - irep->iseq;
      fprintf(fp, "OP_JMPNOT\tR%d\t%03d\t", a, (int)i+(int16_t)b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_JMPNIL, BS);
//      i = pc - irep->iseq;
      fprintf(fp, "OP_JMPNIL\tR%d\t%03d\t", a, (int)i+(int16_t)b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SENDV, BB);
      fprintf(fp, "OP_SENDV\tR%d\t:%s\n", a, pico_mrb_sym_dump(mrb, b));
      break;
    CASE(OP_SENDVB, BB);
      fprintf(fp, "OP_SENDVB\tR%d\t:%s\n", a, pico_mrb_sym_dump(mrb, b));
      break;
    CASE(OP_SEND, BBB);
      fprintf(fp, "OP_SEND\tR%d\t:%s\t%d\n", a, pico_mrb_sym_dump(mrb, b), c);
      break;
    CASE(OP_SENDB, BBB);
      fprintf(fp, "OP_SENDB\tR%d\t:%s\t%d\n", a, pico_mrb_sym_dump(mrb, b), c);
      break;
    CASE(OP_CALL, Z);
      fprintf(fp, "OP_CALL\n");
      break;
    CASE(OP_SUPER, BB);
      fprintf(fp, "OP_SUPER\tR%d\t%d\n", a, b);
      break;
    CASE(OP_ARGARY, BS);
      fprintf(fp, "OP_ARGARY\tR%d\t%d:%d:%d:%d (%d)", a,
             (b>>11)&0x3f,
             (b>>10)&0x1,
             (b>>5)&0x1f,
             (b>>4)&0x1,
             (b>>0)&0xf);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_ENTER, W);
      fprintf(fp, "OP_ENTER\t%d:%d:%d:%d:%d:%d:%d\n",
             PICORB_ASPEC_REQ(a),
             PICORB_ASPEC_OPT(a),
             PICORB_ASPEC_REST(a),
             PICORB_ASPEC_POST(a),
             PICORB_ASPEC_KEY(a),
             PICORB_ASPEC_KDICT(a),
             PICORB_ASPEC_BLOCK(a));
      break;
    CASE(OP_KEY_P, BB);
      fprintf(fp, "OP_KEY_P\tR%d\t:%s\t", a, pico_mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_KEYEND, Z);
      fprintf(fp, "OP_KEYEND\n");
      break;
    CASE(OP_KARG, BB);
      fprintf(fp, "OP_KARG\tR%d\t:%s\t", a, pico_mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_RETURN, B);
      fprintf(fp, "OP_RETURN\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_RETURN_BLK, B);
      fprintf(fp, "OP_RETURN_BLK\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_BREAK, B);
      fprintf(fp, "OP_BREAK\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_BLKPUSH, BS);
      fprintf(fp, "OP_BLKPUSH\tR%d\t%d:%d:%d:%d (%d)", a,
             (b>>11)&0x3f,
             (b>>10)&0x1,
             (b>>5)&0x1f,
             (b>>4)&0x1,
             (b>>0)&0xf);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_LAMBDA, BB);
      fprintf(fp, "OP_LAMBDA\tR%d\tI(%d:%p)\n", a, b, NULL);
      break;
    CASE(OP_BLOCK, BB);
      fprintf(fp, "OP_BLOCK\tR%d\tI(%d:%p)\n", a, b, NULL);
      break;
    CASE(OP_METHOD, BB);
      fprintf(fp, "OP_METHOD\tR%d\tI(%d:%p)\n", a, b, NULL);
      break;
//    CASE(OP_LAMBDA16, BS);
//      fprintf(fp, "OP_LAMBDA\tR%d\tI(%d:%p)\n", a, b, NULL);
//      break;
//    CASE(OP_BLOCK16, BS);
//      fprintf(fp, "OP_BLOCK\tR%d\tI(%d:%p)\n", a, b, NULL);
//      break;
//    CASE(OP_METHOD16, BS);
//      fprintf(fp, "OP_METHOD\tR%d\tI(%d:%p)\n", a, b, NULL);
//      break;
    CASE(OP_RANGE_INC, B);
      fprintf(fp, "OP_RANGE_INC\tR%d\n", a);
      break;
    CASE(OP_RANGE_EXC, B);
      fprintf(fp, "OP_RANGE_EXC\tR%d\n", a);
      break;
    CASE(OP_DEF, BB);
      fprintf(fp, "OP_DEF\tR%d\t:%s\n", a, pico_mrb_sym_dump(mrb, b));
      break;
    CASE(OP_UNDEF, B);
      fprintf(fp, "OP_UNDEF\t:%s\n", pico_mrb_sym_dump(mrb, b));
      break;
    CASE(OP_ALIAS, BB);
      fprintf(fp, "OP_ALIAS\t:%s\t%s\n", pico_mrb_sym_dump(mrb, b), pico_mrb_sym_dump(mrb, b));
      break;
    CASE(OP_ADD, B);
      fprintf(fp, "OP_ADD\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_ADDI, BB);
      fprintf(fp, "OP_ADDI\tR%d\t%d\n", a, b);
      break;
    CASE(OP_SUB, B);
      fprintf(fp, "OP_SUB\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_SUBI, BB);
      fprintf(fp, "OP_SUBI\tR%d\t%d\n", a, b);
      break;
    CASE(OP_MUL, B);
      fprintf(fp, "OP_MUL\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_DIV, B);
      fprintf(fp, "OP_DIV\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_LT, B);
      fprintf(fp, "OP_LT\t\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_LE, B);
      fprintf(fp, "OP_LE\t\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_GT, B);
      fprintf(fp, "OP_GT\t\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_GE, B);
      fprintf(fp, "OP_GE\t\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_EQ, B);
      fprintf(fp, "OP_EQ\t\tR%d\tR%d\n", a, a+1);
      break;
    CASE(OP_ARRAY, BB);
      fprintf(fp, "OP_ARRAY\tR%d\t%d\t", a, b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_ARRAY2, BBB);
      fprintf(fp, "OP_ARRAY\tR%d\tR%d\t%d\t", a, b, c);
      print_lv_ab(mrb, irep, a, b);
      break;
    CASE(OP_ARYCAT, B);
      fprintf(fp, "OP_ARYCAT\tR%d\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_ARYPUSH, B);
      fprintf(fp, "OP_ARYPUSH\tR%d\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_ARYDUP, B);
      fprintf(fp, "OP_ARYDUP\tR%d\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_AREF, BBB);
      fprintf(fp, "OP_AREF\tR%d\tR%d\t%d", a, b, c);
      print_lv_ab(mrb, irep, a, b);
      break;
    CASE(OP_ASET, BBB);
      fprintf(fp, "OP_ASET\tR%d\tR%d\t%d", a, b, c);
      print_lv_ab(mrb, irep, a, b);
      break;
    CASE(OP_APOST, BBB);
      fprintf(fp, "OP_APOST\tR%d\t%d\t%d", a, b, c);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_INTERN, B);
      fprintf(fp, "OP_INTERN\tR%d", a);
      print_lv_a(mrb, irep, a);
      break;
//    CASE(OP_STRING16, BS);
//      goto op_string;
    CASE(OP_STRING, BB);
//    op_string:
//      if ((irep->pool[b].tt & IREP_TT_NFLAG) == 0) {
//        fprintf(fp, "OP_STRING\tR%d\tL(%d)\t; %s", a, b, irep->pool[b].u.str);
//      }
//      else {
        fprintf(fp, "OP_STRING\tR%d\tL(%d)\t", a, b);
//      }
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_STRCAT, B);
      fprintf(fp, "OP_STRCAT\tR%d\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_HASH, BB);
      fprintf(fp, "OP_HASH\tR%d\t%d\t", a, b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_HASHADD, BB);
      fprintf(fp, "OP_HASHADD\tR%d\t%d\t", a, b);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_HASHCAT, B);
      fprintf(fp, "OP_HASHCAT\tR%d\t", a);
      print_lv_a(mrb, irep, a);
      break;

    CASE(OP_OCLASS, B);
      fprintf(fp, "OP_OCLASS\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_CLASS, BB);
      fprintf(fp, "OP_CLASS\tR%d\t:%s", a, pico_mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_MODULE, BB);
      fprintf(fp, "OP_MODULE\tR%d\t:%s", a, pico_mrb_sym_dump(mrb, b));
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_EXEC, BB);
      fprintf(fp, "OP_EXEC\tR%d\tI(%d:%p)", a, b, NULL);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_SCLASS, B);
      fprintf(fp, "OP_SCLASS\tR%d\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_TCLASS, B);
      fprintf(fp, "OP_TCLASS\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_ERR, B);
 //     if ((irep->pool[a].tt & IREP_TT_NFLAG) == 0) {
 //       fprintf(fp, "OP_ERR\t%s\n", irep->pool[a].u.str);
 //     }
 //     else {
        fprintf(fp, "OP_ERR\tL(%d)\n", a);
//      }
      break;
    CASE(OP_EXCEPT, B);
      fprintf(fp, "OP_EXCEPT\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;
    CASE(OP_RESCUE, BB);
      fprintf(fp, "OP_RESCUE\tR%d\tR%d", a, b);
      print_lv_ab(mrb, irep, a, b);
      break;
    CASE(OP_RAISEIF, B);
      fprintf(fp, "OP_RAISEIF\tR%d\t\t", a);
      print_lv_a(mrb, irep, a);
      break;

    CASE(OP_DEBUG, BBB);
      fprintf(fp, "OP_DEBUG\t%d\t%d\t%d\n", a, b, c);
      break;

    CASE(OP_STOP, Z);
      fprintf(fp, "OP_STOP\n");
      break;

    default:
      fprintf(fp, "OP_unknown (0x%x)\n", ins);
 //     return;
      break;
    }
    fprintf(fp, "\n");
  }
}

int
Dump_mrbDump(FILE *fp, Scope *scope, const char *initname)
{
  if (initname[0] == '\0') {
    fwrite(scope->vm_code, scope->vm_code_size, 1, fp);
  } else {
    int i;
    for (i=0; i < 5; i++) {
      fwrite(C_FORMAT_LINES[i], strlen(C_FORMAT_LINES[i]), 1, fp);
      if (i == 2) {
        fwrite(initname, strlen(initname), 1, fp);
        fwrite("[];", 3, 1, fp);
      }
      if (i < 4) fwrite("\n", 1, 1, fp);
    }
    fwrite(initname, strlen(initname), 1, fp);
    fwrite("[] = {", 6, 1, fp);
    char buf[6];
    for (i = 0; i < scope->vm_code_size; i++) {
      if (i % 16 == 0) fwrite("\n", 1, 1, fp);
      snprintf(buf, 6, "0x%02x,", scope->vm_code[i]);
      fwrite(buf, 5, 1, fp);
    }
    fwrite("\n};", 3, 1, fp);
  }
  return 0;
}

#define MRB_FLOAT_FMT "%.17g"

static int
cdump_pool(const Literal *p, FILE *fp)
{
  if (p->type & IREP_TT_NFLAG) {  /* number */
    switch (p->type) {
#ifdef MRB_64BIT
    case INT64_LITERAL:
      if (p->value < INT32_MIN || INT32_MAX < p->value) {
        fprintf(fp, "{IREP_TT_INT64, {.i64=%" PRId64 "}},\n", p->value);
      }
      else {
        /* Deprecated? INT32 */
        fprintf(fp, "{IREP_TT_INT32, {.i32=%" PRId32 "}},\n", (int32_t)p->value);
      }
      break;
#endif
    case INT32_LITERAL:
      /* Deprecated? INT32 */
      //fprintf(fp, "{IREP_TT_INT32, {.i32=%" PRId32 "}},\n", p->value);
      break;
    case FLOAT_LITERAL:
#ifndef MRB_NO_FLOAT
      if (p->value == 0) {
        fprintf(fp, "{IREP_TT_FLOAT, {.f=%#.1f}},\n", atof(p->value));
      }
      else {
        fprintf(fp, "{IREP_TT_FLOAT, {.f=" MRB_FLOAT_FMT "}},\n", atof(p->value));
      }
#endif
      break;
    case BIGINT_LITERAL:
      {
        const char *s = p->value;
        int len = strlen(s);
        fputs("{IREP_TT_BIGINT, {\"", fp);
        for (int i=0; i<len; i++) {
          fprintf(fp, "\\x%02x", (int)s[i]&0xff);
        }
        fputs("\"}},\n", fp);
      }
      break;
    }
  }
  else {                        /* string */
    int i, len = p->type>>2;
    const char *s = p->value;
    fprintf(fp, "{IREP_TT_STR|(%d<<2), {\"", len);
    for (i=0; i<len; i++) {
      fprintf(fp, "\\x%02x", (int)s[i]&0xff);
    }
    fputs("\"}},\n", fp);
  }
  return PICORB_DUMP_OK;
}

static const char *
sym_var_name(const char *initname, const char *key, int n)
{
  char buf[32];
  size_t len = sizeof(buf)+strlen(initname)+strlen(key)+3;
  char *s = picorbc_alloc(len);
  strsafecpy(s, initname, len);
  strsafecat(s, "_", len);
  strsafecat(s, key, len);
  strsafecat(s, "_", len);
  snprintf(buf, sizeof(buf), "%d", n);
  strsafecat(s, buf, len);
  return s;
}

#define ISASCII(c) ((unsigned)(c) <= 0x7f)
#define ISPRINT(c) (((unsigned)(c) - 0x20) < 0x5f)
#define ISSPACE(c) ((c) == ' ' || (unsigned)(c) - '\t' < 5)
#define ISUPPER(c) (((unsigned)(c) - 'A') < 26)
#define ISLOWER(c) (((unsigned)(c) - 'a') < 26)
#define ISALPHA(c) ((((unsigned)(c) | 0x20) - 'a') < 26)
#define ISDIGIT(c) (((unsigned)(c) - '0') < 10)
#define ISXDIGIT(c) (ISDIGIT(c) || ((unsigned)(c) | 0x20) - 'a' < 6)
#define ISALNUM(c) (ISALPHA(c) || ISDIGIT(c))
#define ISBLANK(c) ((c) == ' ' || (c) == '\t')
#define ISCNTRL(c) ((unsigned)(c) < 0x20 || (c) == 0x7f)
#define TOUPPER(c) (ISLOWER(c) ? ((c) & 0x5f) : (c))
#define TOLOWER(c) (ISUPPER(c) ? ((c) | 0x20) : (c))

static bool
sym_name_word_p(const char *name, int len)
{
  if (len == 0) return false;
  if (name[0] != '_' && !ISALPHA(name[0])) return false;
  for (int i = 1; i < len; i++) {
    if (name[i] != '_' && !ISALNUM(name[i])) return false;
  }
  return true;
}

static bool
sym_name_with_equal_p(const char *name, int len)
{
  return len >= 2 && name[len-1] == '=' && sym_name_word_p(name, len-1);
}

static bool
sym_name_with_question_mark_p(const char *name, int len)
{
  return len >= 2 && name[len-1] == '?' && sym_name_word_p(name, len-1);
}

static bool
sym_name_with_bang_p(const char *name, int len)
{
  return len >= 2 && name[len-1] == '!' && sym_name_word_p(name, len-1);
}

static bool
sym_name_ivar_p(const char *name, int len)
{
  return len >= 2 && name[0] == '@' && sym_name_word_p(name+1, len-1);
}

static bool
sym_name_cvar_p(const char *name, int len)
{
  return len >= 3 && name[0] == '@' && sym_name_ivar_p(name+1, len-1);
}

#define OPERATOR_SYMBOL(sym_name, name) {name, sym_name, sizeof(sym_name)-1}
struct operator_symbol {
  const char *name;
  const char *sym_name;
  uint16_t sym_name_len;
};
static const struct operator_symbol operator_table[] = {
  OPERATOR_SYMBOL("!", "not"),
  OPERATOR_SYMBOL("%", "mod"),
  OPERATOR_SYMBOL("&", "and"),
  OPERATOR_SYMBOL("*", "mul"),
  OPERATOR_SYMBOL("+", "add"),
  OPERATOR_SYMBOL("-", "sub"),
  OPERATOR_SYMBOL("/", "div"),
  OPERATOR_SYMBOL("<", "lt"),
  OPERATOR_SYMBOL(">", "gt"),
  OPERATOR_SYMBOL("^", "xor"),
  OPERATOR_SYMBOL("`", "tick"),
  OPERATOR_SYMBOL("|", "or"),
  OPERATOR_SYMBOL("~", "neg"),
  OPERATOR_SYMBOL("!=", "neq"),
  OPERATOR_SYMBOL("!~", "nmatch"),
  OPERATOR_SYMBOL("&&", "andand"),
  OPERATOR_SYMBOL("**", "pow"),
  OPERATOR_SYMBOL("+@", "plus"),
  OPERATOR_SYMBOL("-@", "minus"),
  OPERATOR_SYMBOL("<<", "lshift"),
  OPERATOR_SYMBOL("<=", "le"),
  OPERATOR_SYMBOL("==", "eq"),
  OPERATOR_SYMBOL("=~", "match"),
  OPERATOR_SYMBOL(">=", "ge"),
  OPERATOR_SYMBOL(">>", "rshift"),
  OPERATOR_SYMBOL("[]", "aref"),
  OPERATOR_SYMBOL("||", "oror"),
  OPERATOR_SYMBOL("<=>", "cmp"),
  OPERATOR_SYMBOL("===", "eqq"),
  OPERATOR_SYMBOL("[]=", "aset"),
};

static const char*
sym_operator_name(const char *sym_name, int len)
{
  uint32_t table_size = sizeof(operator_table)/sizeof(struct operator_symbol);
  if (operator_table[table_size-1].sym_name_len < len) return NULL;

  uint32_t start, idx;
  int cmp;
  const struct operator_symbol *op_sym;
  for (start = 0; table_size != 0; table_size/=2) {
    idx = start+table_size/2;
    op_sym = &operator_table[idx];
    cmp = (int)len-(int)op_sym->sym_name_len;
    if (cmp == 0) {
      cmp = memcmp(sym_name, op_sym->sym_name, len);
      if (cmp == 0) return op_sym->name;
    }
    if (0 < cmp) {
      start = ++idx;
      --table_size;
    }
  }
  return NULL;
}

#define IS_EVSTR(p,e) ((p) < (e) && (*(p) == '$' || *(p) == '@' || *(p) == '{'))
const char picorb_digitmap[] = "0123456789abcdefghijklmnopqrstuvwxyz";

static void
picorb_str_dump(char *result, const char *str, const uint32_t len)
{
  result[0] = '"';
  result[1] = '\0';
  const char *p, *pend;
  char buf[5];  /* `\x??` or UTF-8 character */

  p = str; pend = str + len;
  for (;p < pend; p++) {
    memset(buf, 0, 5);
    unsigned char c, cc;
    c = *p;
    if (c == '"'|| c == '\\' || (c == '#' && IS_EVSTR(p+1, pend))) {
      buf[0] = '\\'; buf[1] = c;
      strsafecat(result, buf, 1024);
      continue;
    }
    if (ISPRINT(c)) {
      buf[0] = c;
      strsafecat(result, buf, 1024);
      continue;
    }
    switch (c) {
      case '\n': cc = 'n'; break;
      case '\r': cc = 'r'; break;
      case '\t': cc = 't'; break;
      case '\f': cc = 'f'; break;
      case '\013': cc = 'v'; break;
      case '\010': cc = 'b'; break;
      case '\007': cc = 'a'; break;
      case 033: cc = 'e'; break;
      default: cc = 0; break;
    }
    if (cc) {
      buf[0] = '\\';
      buf[1] = (char)cc;
      strsafecat(result, buf, 1024);
      continue;
    }
    else {
      buf[0] = '\\';
      buf[1] = 'x';
      buf[3] = picorb_digitmap[c % 16]; c /= 16;
      buf[2] = picorb_digitmap[c % 16];
      strsafecat(result, buf, 1024);
      continue;
    }
  }
  strsafecat(result, "\"", 1024);
}
static int
cdump_sym(Symbol *sym, const char *var_name, int idx, char *init_syms_code, FILE *fp)
{
  if (sym == 0) return PICORB_DUMP_INVALID_ARGUMENT;

  int len = sym->len;
  const char *name = sym->value, *op_name;
  if (!name) return PICORB_DUMP_INVALID_ARGUMENT;
  if (sym_name_word_p(name, len)) {
    fprintf(fp, "MRB_SYM(%s)", name);
  }
  else if (sym_name_with_equal_p(name, len)) {
    fprintf(fp, "MRB_SYM_E(%.*s)", (int)(len-1), name);
  }
  else if (sym_name_with_question_mark_p(name, len)) {
    fprintf(fp, "MRB_SYM_Q(%.*s)", (int)(len-1), name);
  }
  else if (sym_name_with_bang_p(name, len)) {
    fprintf(fp, "MRB_SYM_B(%.*s)", (int)(len-1), name);
  }
  else if (sym_name_ivar_p(name, len)) {
    fprintf(fp, "MRB_IVSYM(%s)", name+1);
  }
  else if (sym_name_cvar_p(name, len)) {
    fprintf(fp, "MRB_CVSYM(%s)", name+2);
  }
  else if ((op_name = sym_operator_name(name, len))) {
    fprintf(fp, "MRB_OPSYM(%s)", op_name);
  }
  else {
    char buf[32];
    strsafecat(init_syms_code, "  ", 1024);
    strsafecat(init_syms_code, var_name, 1024);
    snprintf(buf, sizeof(buf), "[%d] = ", idx);
    strsafecat(init_syms_code, buf, 1024);
    strsafecat(init_syms_code, "mrb_intern_lit(mrb, ", 1024);
    char *result = picorbc_alloc(1024);
    picorb_str_dump(result, sym->value, sym->len);
    strsafecat(init_syms_code, result, 1024);
    picorbc_free(result);
    strsafecat(init_syms_code, ");\n", 1024);
    fputs("0", fp);
  }
  fputs(", ", fp);
  return PICORB_DUMP_OK;
}

static int
cdump_syms(const char *name, const char *key, int n, int syms_len, Symbol *syms, char *init_syms_code, FILE *fp)
{
//  int ai = mrb_gc_arena_save(mrb);
  int code_len = strlen(init_syms_code);
  const char *var_name = sym_var_name(name, key, n);
  fprintf(fp, "mrb_DEFINE_SYMS_VAR(%s, %d, (", var_name, syms_len);
  Symbol *sym = syms;
  for (int i=0; i<syms_len; i++) {
    cdump_sym(sym, var_name, i, init_syms_code, fp);
    sym = sym->next;
  }
  picorbc_free((void *)var_name);
  fputs("), ", fp);
  if (code_len == strlen(init_syms_code)) fputs("const", fp);
  fputs(");\n", fp);
//  mrb_gc_arena_restore(mrb, ai);
  return PICORB_DUMP_OK;
}

int
cdump_irep_struct(Scope *scope, uint8_t flags, FILE *fp, const char *name, int n, char *init_syms_code, int *mp)
{
  int i, len;
  int max = *mp;
  int debug_available = 0;
  /* dump reps */
  if (scope->first_lower) {
    Scope *next = scope->first_lower;
    for (i=0,len=scope->nlowers; i<len; i++) {
      *mp += len;
      if (cdump_irep_struct(next, flags, fp, name, max+i, init_syms_code, mp) != PICORB_DUMP_OK) {
        return PICORB_DUMP_INVALID_ARGUMENT;
      }
      next = next->next;
    }
    fprintf(fp,   "static const mrb_irep *%s_reps_%d[%d] = {\n", name, n, len);
    for (i=0,len=scope->nlowers; i<len; i++) {
      fprintf(fp,   "  &%s_irep_%d,\n", name, max+i);
    }
    fputs("};\n", fp);
  }
  /* dump pool */
  if (scope->literal) {
    len=scope->plen;
    fprintf(fp,   "static const mrb_pool_value %s_pool_%d[%d] = {\n", name, n, len);
    Literal *lit = scope->literal;
    for (i=0; i<len; i++) {
      if (cdump_pool(lit, fp) != PICORB_DUMP_OK)
        return PICORB_DUMP_INVALID_ARGUMENT;
      lit = lit->next;
    }
    fputs("};\n", fp);
  }
  /* dump syms */
  if (scope->symbol) {
    cdump_syms(name, "syms", n, scope->slen, scope->symbol, init_syms_code, fp);
  }
  /* dump iseq */
  len = scope->ilen + sizeof(ExcHandler) * scope->clen;
  fprintf(fp,   "static const mrb_code %s_iseq_%d[%d] = {", name, n, len);
  for (i=0; i<len; i++) {
    if (i%20 == 0) fputs("\n", fp);
    if (scope->upper == NULL) {
      fprintf(fp, "0x%02x,", scope->vm_code[i + MRB_HEADER_SIZE + IREP_HEADER_SIZE]);
    } else {
      if (scope->vm_code) {
      fprintf(fp, "0x%02x,", scope->vm_code[i + IREP_HEADER_SIZE]);
      }
    }
  }
  fputs("};\n", fp);
  /* dump lv */
  if (scope->lvar) {
    cdump_syms(name, "lv", n, scope->nlocals-1, (Symbol*)(scope->lvar), init_syms_code, fp);
  }
  /* dump debug */
  // TODO
  /* dump irep */
  fprintf(fp, "static const mrb_irep %s_irep_%d = {\n", name, n);
  fprintf(fp,   "  %d,%d,%d,\n", scope->nlocals, scope->sp, scope->clen);
  fprintf(fp,   "  MRB_IREP_STATIC,%s_iseq_%d,\n", name, n);
  if (scope->literal) {
    fprintf(fp, "  %s_pool_%d,", name, n);
  }
  else {
    fputs(      "  NULL,", fp);
  }
  if (scope->symbol) {
    fprintf(fp, "%s_syms_%d,", name, n);
  }
  else {
    fputs(      "NULL,", fp);
  }
  if (scope->first_lower) {
    fprintf(fp, "%s_reps_%d,\n", name, n);
  }
  else {
    fputs(      "NULL,\n", fp);
  }
  if (scope->lvar) {
    fprintf(fp, "  %s_lv_%d,\n", name, n);
  }
  else {
    fputs(      "  NULL,\t\t\t\t\t/* lv */\n", fp);
  }
  if(debug_available) {
    fprintf(fp, "  &%s_debug_%d,\n", name, n);
  }
  else {
    fputs("  NULL,\t\t\t\t\t/* debug_info */\n", fp);
  }
  fprintf(fp,   "  %d,%d,%d,%d,0\n};\n", scope->ilen, scope->plen, scope->slen, scope->nlowers);

  return PICORB_DUMP_OK;
}

int
Dump_cstructDump(FILE *fp, Scope *scope, uint8_t flags, const char *initname)
{
  if (fp == NULL || initname == NULL || initname[0] == '\0') {
    return PICORB_DUMP_INVALID_ARGUMENT;
  }
  if (fprintf(fp, "#include <mruby.h>\n"
                  "#include <mruby/proc.h>\n"
                  "#include <mruby/presym.h>\n"
                  "\n") < 0) {
    return PICORB_DUMP_WRITE_FAULT;
  }
  fputs("#define mrb_BRACED(...) {__VA_ARGS__}\n", fp);
  fputs("#define mrb_DEFINE_SYMS_VAR(name, len, syms, qualifier) \\\n", fp);
  fputs("  static qualifier mrb_sym name[len] = mrb_BRACED syms\n", fp);
  fputs("\n", fp);
  char *init_syms_code = (char *)picorbc_alloc(1024); // FIXME
  init_syms_code[0] = '\0';
  int max = 1;
  int n = cdump_irep_struct(scope, flags, fp, initname, 0, init_syms_code, &max);
  if (n != PICORB_DUMP_OK) return n;
  fprintf(fp,
          "%s\n"
          "const struct RProc %s[] = {{\n",
          (flags & PICORB_DUMP_STATIC) ? "static"
                                    : "#ifdef __cplusplus\n"
                                      "extern const struct RProc test[];\n"
                                      "#endif",
          initname);
  fprintf(fp, "NULL,NULL,MRB_TT_PROC,7,0,{&%s_irep_0},NULL,{NULL},\n}};\n", initname);
  fputs("static void\n", fp);
  fprintf(fp, "%s_init_syms(mrb_state *mrb)\n", initname);
  fputs("{\n", fp);
  fputs(init_syms_code, fp);
  picorbc_free(init_syms_code);
  fputs("}\n", fp);
  return PICORB_DUMP_OK;
}
