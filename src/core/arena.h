#ifndef RATA_CORE_ARENA_H
#define RATA_CORE_ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/status.h"

/* Linear bump allocator with explicit ownership.
 * Memory allocated from an arena stays valid until rata_arena_restore(),
 * rata_arena_reset(), or rata_arena_destroy(). */
typedef struct RataArena {
    uint8_t *base;
    size_t cap;
    size_t offset;
    bool owns_buffer;
} RataArena;

/* Initializes arena over a caller-provided buffer.
 * Caller retains ownership of `buffer` and must keep it alive while arena is used. */
RataStatus rata_arena_init(RataArena *arena, uint8_t *buffer, size_t capacity);

/* Initializes arena with a heap-allocated buffer of `capacity` bytes.
 * Arena owns the buffer; caller must call rata_arena_destroy(). */
RataStatus rata_arena_init_heap(RataArena *arena, size_t capacity);

/* Releases heap-owned buffer if owns_buffer is true, and zeroes the arena struct. */
void rata_arena_destroy(RataArena *arena);

/* Allocates `size` bytes aligned to `align` (power of 2, or 0 for default 8).
 * Returned pointer is owned by `arena`. Returns NULL on invalid args or OOM. */
void *rata_arena_alloc(RataArena *arena, size_t size, size_t align);

/* Allocates zero-initialized array of `count` elements of `elem_size` bytes.
 * Returned pointer is owned by `arena`. Returns NULL on overflow or OOM. */
void *rata_arena_calloc(RataArena *arena, size_t count, size_t elem_size, size_t align);

/* Resets allocation offset to 0 without freeing the underlying buffer. */
void rata_arena_reset(RataArena *arena);

/* Saves current offset so temporary scratch allocations can be rolled back. */
size_t rata_arena_mark(const RataArena *arena);

/* Restores offset to a previously saved mark (clamped safely if mark > offset). */
void rata_arena_restore(RataArena *arena, size_t mark);

/* Returns bytes currently allocated. */
size_t rata_arena_used(const RataArena *arena);

/* Returns bytes remaining in the backing buffer. */
size_t rata_arena_remaining(const RataArena *arena);

#endif /* RATA_CORE_ARENA_H */
