#ifndef BROCOPY_H
#define BROCOPY_H

#include <stdint.h>

//==================================================
// Inlined / Macros
//==================================================

// (double evaluation error prone)
#define IS_UPPER(ch) ((ch) >= 'A' && (ch) <= 'Z')
#define IS_LOWER(ch) ((ch) >= 'a' && (ch) <= 'z')

static inline uint64_t
str_len(char *s)
{
  char *p = s;
  while (*p) ++p;
  return p - s;
}

// 'A' = 0b0100 0001
// 'a' = 0b0110 0001
// Flip the bit 5 to lower A-Z
static inline int32_t
to_lower(int32_t ch)
{
  uint32_t is_cap = (ch >= 'A' && ch <= 'Z');
  return ch | (is_cap << 5);
}

typedef struct Str8
{
  uint8_t *ptr;
  uint64_t size;
} Str8;

#define str8_from_buf(buf)    (Str8){ (uint8_t *)(buf), sizeof(buf) }
#define str8_from_lit(lit)    (Str8){ (uint8_t *)(lit), sizeof(lit) - 1 }
#define str8_from_chptr(ptr)  (Str8){ (uint8_t *)(ptr), str_len(ptr) } /* Assumes null terminated char* */



static inline int32_t
str8_equals(Str8 lhs, Str8 rhs)
{
  if (lhs.size != rhs.size) return 0;

  for (uint64_t i = 0; i < lhs.size; ++i)
  {
    if (lhs.ptr[i] != rhs.ptr[i]) return 0;
  }

  return 1;
}

static inline int32_t
str8_equals_insensitive(Str8 lhs, Str8 rhs)
{
  if (lhs.size != rhs.size) return 0;

  for (uint64_t i = 0; i < lhs.size; ++i)
  {
    if (to_lower(lhs.ptr[i]) != to_lower(rhs.ptr[i])) return 0;
  }

  return 1;
}

// Return 1 if the Str8 match up to `n` chars, zero otherwise
static inline int32_t
str8_match(Str8 lhs, Str8 rhs, uint64_t n)
{
  if (lhs.size < n || rhs.size < n) return 0;

  for (uint64_t i = 0; i < n; ++i)
  {
    if (lhs.ptr[i] != rhs.ptr[i]) return 0;
  }

  return 1;
}

static inline int32_t
str8_match_insensitive(Str8 lhs, Str8 rhs, uint64_t n)
{
  if (lhs.size < n || rhs.size < n) return 0;

  for (uint64_t i = 0; i < n; ++i)
  {
    if (to_lower(lhs.ptr[i]) != to_lower(rhs.ptr[i])) return 0;
  }

  return 1;
}

// Return `str.size` if `ch` not found
static inline uint64_t
str8_index(Str8 str, uint8_t ch)
{
  for (uint64_t i = 0; i < str.size; ++i)
  {
    if (str.ptr[i] == ch)
      return i;
  }

  return str.size;
}

static inline uint64_t
str8_index_last(Str8 str, uint8_t ch)
{
  for (uint64_t i = str.size - 1; i > 0; --i)
  {
    if (str.ptr[i - 1] == ch)
      return i - 1;
  }

  return str.size;
}

// Return `str.size` if substring not found
static inline uint64_t
str8_index_substr(Str8 str, Str8 sub)
{
  if (sub.size > str.size || sub.size == 0) return str.size;

  Str8 slice;
  for (uint64_t i = 0, last_idx = (str.size - sub.size); i <= last_idx; ++i)
  {
    slice = (Str8){ str.ptr + i, sub.size };
    if (str8_equals(slice, sub))
    {
      return i;
    }
  }

  return str.size;
}

static inline uint64_t
str8_index_substr_last(Str8 str, Str8 sub)
{
  if (sub.size > str.size || sub.size == 0) return str.size;

  Str8 slice;
  for (uint64_t i = (str.size - sub.size) + 1; i > 0; --i)
  {
    slice = (Str8){ str.ptr + (i-1), sub.size };
    if (str8_equals(slice, sub))
    {
      return i-1;
    }
  }

  return str.size;
}


//==================================================
// Prototypes
//==================================================

int32_t str_match(char *str0, char *str1, int32_t len, int32_t insensitive);
uint32_t str_append(char *buf, char *s, uint32_t size);
char *index(char *buf_p, char ch);

#endif // BROCOPY_H
