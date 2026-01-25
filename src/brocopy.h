#ifndef BROCOPY_H
#define BROCOPY_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

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


//==================================================
// Arena (Simple arena implementation)
//==================================================

typedef struct Arena
{
  uint8_t *base;
  uint64_t size;
  uint64_t pos;
} Arena;

static inline Arena
arena_alloc(uint64_t size)
{
  Arena arena;

  arena.base = (uint8_t*)malloc(size);
  if (arena.base)
  {
    arena.size = size;
  }
  else
  {
    arena.size = 0;
  }
  arena.pos = 0;

  return arena;
}

// `size` must be sizeof(buffer) or previous allocated size
static inline Arena
arena_from_buffer(uint8_t *buffer, uint64_t size)
{
  Arena arena;

  arena.base = buffer;
  arena.size = size;
  arena.pos = 0;

  return arena;
}

static inline void *
arena_push(Arena *arena, uint64_t size)
{
  // calculate new position - 8 byte aligned
  uint64_t aligned = (arena->pos + 7u) & ~7u;
  if (aligned + size <= arena->size)
  {
    void *ptr = arena->base + aligned;
    arena->pos = aligned + size;
    return ptr;
  }

  return NULL; // Arena out of space
}

static inline void
arena_clear(Arena *arena)
{
  arena->pos = 0;
}

static inline void
arena_free(Arena *arena)
{
  if (arena->base)
  {
    free(arena->base);
    arena->base = NULL;
    arena->size = 0;
    arena->pos = 0;
  }
}

//==================================================
// Scratch
//==================================================

typedef struct Scratch
{
  Arena *arena;
  uint64_t origin_pos;
} Scratch;

static inline Scratch
scratch_start(Arena *arena)
{
  return (Scratch){ arena, arena->pos };
}

static inline void
scratch_end(Scratch scratch)
{
  scratch.arena->pos = scratch.origin_pos;
}


//==================================================
// Str8
//==================================================
typedef struct Str8
{
  uint8_t *ptr;
  uint64_t size;
} Str8;

#define str8_from_buf(buf)  (Str8){ (uint8_t *)(buf), sizeof(buf) }
#define str8_from_lit(lit)  (Str8){ (uint8_t *)(lit), sizeof(lit) - 1 }
#define str8_from_cstr(ptr) (Str8){ (uint8_t *)(ptr), str_len(ptr) } /* Assumes null terminated char* */

static inline Str8
str8_push(Arena *arena, uint64_t size)
{
  return (Str8){ arena_push(arena, size), size };
}

static inline Str8
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
static inline uint64_t
str8_snprintf(Str8 str, char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  int32_t written = vsnprintf((char*)str.ptr, str.size, fmt, args);
  va_end(args);

  return (written < 0) ? 0 : (uint64_t)written;
}

static inline Str8
str8_append(Arena *arena, Str8 lhs, Str8 rhs)
{
  return str8_pushf(arena, "%.*s%.*s", (int)lhs.size, lhs.ptr, (int)rhs.size, rhs.ptr);
}

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

static inline uint64_t
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


//==================================================
// Prototypes
//==================================================

int32_t str_match(char *str0, char *str1, int32_t len, int32_t insensitive);
uint32_t str_append(char *buf, char *s, uint32_t size);
char *index(char *buf_p, char ch);

#endif // BROCOPY_H
