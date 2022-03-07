#include <stdio.h>

#include <dump.h>

#define MRB_ASPEC_REQ(a)          (((a) >> 18) & 0x1f)
#define MRB_ASPEC_OPT(a)          (((a) >> 13) & 0x1f)
#define MRB_ASPEC_REST(a)         (((a) >> 12) & 0x1)
#define MRB_ASPEC_POST(a)         (((a) >> 7) & 0x1f)
#define MRB_ASPEC_KEY(a)          (((a) >> 2) & 0x1f)
#define MRB_ASPEC_KDICT(a)        (((a) >> 1) & 0x1)
#define MRB_ASPEC_BLOCK(a)        ((a) & 1)

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
//#ifndef MRB_NO_FLOAT
//        fprintf(fp, "OP_LOADL\tR%d\tL(%d)\t; %f", a, b, (double)irep->pool[b].u.f);
//#endif
//        break;
//      case IREP_TT_INT32:
//        fprintf(fp, "OP_LOADL\tR%d\tL(%d)\t; %" PRId32, a, b, irep->pool[b].u.i32);
//        break;
//#ifdef MRB_64BIT
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
             MRB_ASPEC_REQ(a),
             MRB_ASPEC_OPT(a),
             MRB_ASPEC_REST(a),
             MRB_ASPEC_POST(a),
             MRB_ASPEC_KEY(a),
             MRB_ASPEC_KDICT(a),
             MRB_ASPEC_BLOCK(a));
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
