#ifndef BROCOPY_H
#include "brocopy.h" // only to make it possible to use -fsyntax-only
#endif

// Return 1 if the cstrings match up to `len` chars, zero otherwise
int32_t
static cstr_match(char *str0, char *str1, int32_t len, int32_t insensitive)
{
  int32_t i = 0;
  if (insensitive)
  {
    while (i < len && (*str0 && *str1) && to_lower(*str0) == to_lower(*str1))
    {
      ++i;
      ++str0;
      ++str1;
    }
  }
  else
  {
    while (i < len && (*str0 && *str1) && *str0 == *str1)
    {
      ++i;
      ++str0;
      ++str1;
    }
  }

  return i == len;
}

// Append `s` to `buf` and return new length -> Truncate `s` if length of `buf` + `s` exceeds `size`
static uint32_t
cstr_append(char *buf, char *s, uint32_t size)
{
  if (size == 0) return 0;

  // End of the buf string
  uint64_t idx_buf = str_len(buf);

  if (idx_buf >= size) return idx_buf;

  for (uint64_t i = 0; s[i] != '\0' && idx_buf < (size - 1); ++i)
  {
    buf[idx_buf++] = s[i];
  }

  buf[idx_buf] = '\0';
  return idx_buf;
}

static char *
cstr_index(char *buf_p, char ch)
{
  while (*buf_p != '\0')
  {
    if (*buf_p == ch)
      return buf_p;
    ++buf_p;
  }
  return NULL;
}
