#ifndef BROCOPY_H
#include "brocopy.h" // only to make it possible to use -fsyntax-only
#endif

//==================================================
// Str8
//==================================================

static Str8
str8_push(Arena *arena, uint64_t size)
{
  return (Str8){ (uint8_t*)arena_push(arena, size), size };
}

static Str8
str8_pushf(Arena *arena, char *fmt, ...)
{
  va_list args, args_cp;
  va_start(args, fmt);
  va_copy(args_cp, args);

  int32_t len = vsnprintf(0, 0, fmt, args_cp);
  va_end(args_cp);

  Str8 result = {0};
  if (len >= 0)
  {
    result.ptr = (uint8_t*)arena_push(arena, (uint64_t)len + 1); // vsnprintf writes `bufsz - 1` + NULL char
    result.size = len;
    if (result.ptr) vsnprintf((char*)result.ptr, result.size + 1, fmt, args);
  }
  va_end(args);

  return result;
}

// Change elements of Str8 -> vsnprintf truncates new format if it exceed Str8 size
static uint64_t
str8_snprintf(Str8 str, char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  int32_t written = vsnprintf((char*)str.ptr, str.size, fmt, args);
  va_end(args);

  return (written < 0) ? 0 : (uint64_t)written;
}

static Str8
str8_append(Arena *arena, Str8 lhs, Str8 rhs)
{
  return str8_pushf(arena, "%.*s%.*s", (int)lhs.size, lhs.ptr, (int)rhs.size, rhs.ptr);
}

static int32_t
str8_equals(Str8 lhs, Str8 rhs)
{
  if (lhs.size != rhs.size) return 0;

  for (uint64_t i = 0; i < lhs.size; ++i)
  {
    if (lhs.ptr[i] != rhs.ptr[i]) return 0;
  }

  return 1;
}

static int32_t
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
static int32_t
str8_match(Str8 lhs, Str8 rhs, uint64_t n)
{
  if (lhs.size < n || rhs.size < n) return 0;

  for (uint64_t i = 0; i < n; ++i)
  {
    if (lhs.ptr[i] != rhs.ptr[i]) return 0;
  }

  return 1;
}

static int32_t
str8_match_insensitive(Str8 lhs, Str8 rhs, uint64_t n)
{
  if (lhs.size < n || rhs.size < n) return 0;

  for (uint64_t i = 0; i < n; ++i)
  {
    if (to_lower(lhs.ptr[i]) != to_lower(rhs.ptr[i])) return 0;
  }

  return 1;
}

static uint64_t
str8_index(Str8 str, uint8_t ch)
{
  for (uint64_t i = 0; i < str.size; ++i)
  {
    if (str.ptr[i] == ch)
      return i;
  }

  return str.size;
}

static uint64_t
str8_index_last(Str8 str, uint8_t ch)
{
  for (uint64_t i = str.size - 1; i > 0; --i)
  {
    if (str.ptr[i - 1] == ch)
      return i - 1;
  }

  return str.size;
}

static uint64_t
str8_index_substr(Str8 str, Str8 sub)
{
  if (sub.size > str.size || sub.size == 0) return str.size;

  Str8 slice = {0};
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

static uint64_t
str8_index_substr_last(Str8 str, Str8 sub)
{
  if (sub.size > str.size || sub.size == 0) return str.size;

  Str8 slice = {0};
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

static Str8
str8_skip(Str8 str, uint64_t n)
{
  n = (n > str.size) ? str.size : n;
  str.ptr += n;
  str.size -= n;
  return str;
}

static Str8
str8_prefix(Str8 str, uint64_t size)
{
  size = (size > str.size) ? str.size : size;
  str.size = size;
  return str;
}

static Str8
str8_postfix(Str8 str, uint64_t size)
{
  size = (size > str.size) ? str.size : size;
  str.ptr = (str.ptr + str.size) - size;
  str.size = size;
  return str;
}

static Str8
str8_buffer_file(Arena *arena, Str8 path)
{
  Str8 result = {0};

  FILE *file = fopen((char*)path.ptr, "rb");
  if (!file) return result;

  fseek(file, 0, SEEK_END);
  int64_t file_size = ftell(file);
  rewind(file);

  if (file_size > 0) 
  {
    // Alocate file_size bytes on arena and buffer the stream
    result.ptr = (uint8_t*)arena_push(arena, (uint64_t)file_size);
    result.size = (uint64_t)file_size;
    fread(result.ptr, 1, result.size, file);
  }

  fclose(file);

  return result;
}

