// Unity-Build
#ifndef BROCOPY_H
#define BROCOPY_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memset

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

static Arena arena_alloc(uint64_t size);
static Arena arena_from_buffer(uint8_t *buffer, uint64_t size);
static void * arena_push(Arena *arena, uint64_t size);
static void arena_clear(Arena *arena);
static void arena_free(Arena *arena);

//==================================================
// Scratch
//==================================================

typedef struct Scratch
{
  Arena *arena;
  uint64_t origin_pos;
} Scratch;

static Scratch scratch_start(Arena *arena);
static void scratch_end(Scratch scratch);


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

static Str8 str8_push(Arena *arena, uint64_t size);
static Str8 str8_pushf(Arena *arena, char *fmt, ...);
static uint64_t str8_snprintf(Str8 str, char *fmt, ...);
static Str8 str8_append(Arena *arena, Str8 lhs, Str8 rhs);

static int32_t str8_equals(Str8 lhs, Str8 rhs);
static int32_t str8_equals_insensitive(Str8 lhs, Str8 rhs);
static int32_t str8_match(Str8 lhs, Str8 rhs, uint64_t n);
static int32_t str8_match_insensitive(Str8 lhs, Str8 rhs, uint64_t n);

static uint64_t str8_index(Str8 str, uint8_t ch);
static uint64_t str8_index_last(Str8 str, uint8_t ch);
static uint64_t str8_index_substr(Str8 str, Str8 sub);
static uint64_t str8_index_substr_last(Str8 str, Str8 sub);

static Str8 str8_skip(Str8 str, uint64_t n);
static Str8 str8_prefix(Str8 str, uint64_t n);
static Str8 str8_postfix(Str8 str, uint64_t n);

static Str8 str8_buffer_file(Arena *arena, Str8 path);

//==================================================
// Prototypes
//==================================================

int32_t str_match(char *str0, char *str1, int32_t len, int32_t insensitive);
uint32_t str_append(char *buf, char *s, uint32_t size);
char *index(char *buf_p, char ch);

#endif // BROCOPY_H

