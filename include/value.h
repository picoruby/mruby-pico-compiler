#ifndef PICORBC_VALUE_H_
#define PICORBC_VALUE_H_

#if defined _MSC_VER && _MSC_VER < 1800
# define PRIo64 "llo"
# define PRId64 "lld"
# define PRIu64 "llu"
# define PRIx64 "llx"
# define PRIo16 "ho"
# define PRId16 "hd"
# define PRIu16 "hu"
# define PRIx16 "hx"
# define PRIo32 "o"
# define PRId32 "d"
# define PRIu32 "u"
# define PRIx32 "x"
#else
# include <inttypes.h>
#endif

#if defined(MRB_INT64)
  typedef int64_t mrb_int;
  typedef uint64_t mrb_uint;
# define MRB_INT_BIT 64
# define MRB_INT_MIN INT64_MIN
# define MRB_INT_MAX INT64_MAX
# define MRB_PRIo PRIo64
# define MRB_PRId PRId64
# define MRB_PRIx PRIx64
#else
  typedef int32_t mrb_int;
  typedef uint32_t mrb_uint;
# define MRB_INT_BIT 32
# define MRB_INT_MIN INT32_MIN
# define MRB_INT_MAX INT32_MAX
# define MRB_PRIo PRIo32
# define MRB_PRId PRId32
# define MRB_PRIx PRIx32
#endif

#ifdef MRB_ENDIAN_BIG
# define MRB_ENDIAN_LOHI(a,b) a b
#else
# define MRB_ENDIAN_LOHI(a,b) b a
#endif
#endif /* PICORBC_VALUE_H_ */
