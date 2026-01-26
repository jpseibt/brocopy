#ifndef BROCOPY_H
#include "brocopy.h" // only to make it possible to use -fsyntax-only
#endif

//==================================================
// Arena (Simple arena implementation)
//==================================================

static Arena
arena_alloc(uint64_t size)
{
  Arena arena = {0};

  arena.base = (uint8_t*)malloc(size);
  if (arena.base)
  {
    arena.size = size;
    memset(arena.base, 0, arena.size);
  }
  else
  {
    arena.size = 0;
  }
  arena.pos = 0;

  return arena;
}

// `size` must be sizeof(buffer) or previous allocated size
static Arena
arena_from_buffer(uint8_t *buffer, uint64_t size)
{
  Arena arena;

  arena.base = buffer;
  arena.size = size;
  arena.pos = 0;

  return arena;
}

static void *
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

static void
arena_clear(Arena *arena)
{
  arena->pos = 0;
}

static void
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

static Scratch
scratch_start(Arena *arena)
{
  return (Scratch){ arena, arena->pos };
}

static void
scratch_end(Scratch scratch)
{
  scratch.arena->pos = scratch.origin_pos;
}
