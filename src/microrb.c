#ifndef DISABLE_MRUBY

#include <string.h>
#include <mrb_state/microrb.h>
#include <debug.h>

MRB_API mrb_value
microrb_load_file_cxt(mrb_state *mrb, FILE *fp, picorbc_context *c)
{
  mrb_value ret;
  StreamInterface *si = StreamInterface_new(fp, NULL, STREAM_TYPE_FILE);
  ParserState *p = Compiler_parseInitState(si->node_box_size);
  if (Compiler_compile(p, si, c)) {
    mrb_load_irep(mrb, p->scope->vm_code);
  } else {
  }
  return ret;
}

/* binary header */
#define RITE_BINARY_IDENT              "RITE"
struct rite_binary_header {
  uint8_t binary_ident[4];    /* Binary Identifier */
  uint8_t major_version[2];   /* Binary Format Major Version */
  uint8_t minor_version[2];   /* Binary Format Minor Version */
  uint8_t binary_size[4];     /* Binary Size */
  uint8_t compiler_name[4];   /* Compiler name */
  uint8_t compiler_version[4];
};

#define DETECT_SIZE 64

static inline uint32_t
bin_to_uint32(const uint8_t *bin)
{
  return (uint32_t)bin[0] << 24 |
         (uint32_t)bin[1] << 16 |
         (uint32_t)bin[2] << 8  |
         (uint32_t)bin[3];
}

mrb_value
microrb_load_detect_file_cxt(mrb_state *mrb, FILE *fp, picorbc_context *c)
{
  union {
    char b[DETECT_SIZE];
    struct rite_binary_header h;
  } leading;
  size_t bufsize;

  if (fp == NULL) {
    return mrb_nil_value();
  }

  bufsize = fread(leading.b, sizeof(char), sizeof(leading), fp);
  if (bufsize < sizeof(leading.h) ||
      memcmp(leading.h.binary_ident, RITE_BINARY_IDENT, sizeof(leading.h.binary_ident)) != 0 ||
      memchr(leading.b, '\0', bufsize) == NULL) {
    //return mrb_load_exec(mrb, mrb_parse_file_continue(mrb, fp, leading.b, bufsize, c), c);
    rewind(fp);
    return microrb_load_file_cxt(mrb, fp, c);
  }
  else {
//    size_t binsize;
//    uint8_t *bin;
//    mrb_value bin_obj = mrb_nil_value(); /* temporary string object */
//    mrb_value result;
//
//    binsize = bin_to_uint32(leading.h.binary_size);
//    bin_obj = mrb_str_new(mrb, NULL, binsize);
//    bin = (uint8_t *)RSTRING_PTR(bin_obj);
//    memcpy(bin, leading.b, bufsize);
//    if (binsize > bufsize &&
//        fread(bin + bufsize, binsize - bufsize, 1, fp) == 0) {
//      binsize = bufsize;
//      /* The error is reported by mrb_load_irep_buf_cxt() */
//    }
//
//    result = mrb_load_irep_buf_cxt(mrb, bin, binsize, c);
//    if (mrb_string_p(bin_obj)) mrb_str_resize(mrb, bin_obj, 0);
//    return result;
    rewind(fp);
    return mrb_load_irep_file_cxt(mrb, fp, c);
  }
}

MRB_API mrb_value
microrb_load_string_cxt(mrb_state *mrb, const char *str, picorbc_context *c)
{
  mrb_value ret;
  StreamInterface *si = StreamInterface_new(NULL, (char *)str, STREAM_TYPE_MEMORY);
  ParserState *p = Compiler_parseInitState(si->node_box_size);
  if (Compiler_compile(p, si, c)) {
    mrb_load_irep(mrb, p->scope->vm_code);
  } else {
  }
  return ret;
}

#endif /* DISABLE_MRUBY */
