#include "core/arena.h"

#include <stdlib.h>
#include <string.h>

static bool is_power_of_two(size_t x) {
    return x != 0u && (x & (x - 1u)) == 0u;
}

RataStatus rata_arena_init(RataArena *arena, uint8_t *buffer, size_t capacity) {
    if (arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    if (buffer == NULL && capacity > 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    arena->base = buffer;
    arena->cap = capacity;
    arena->offset = 0u;
    arena->owns_buffer = false;
    return RATA_OK;
}

RataStatus rata_arena_init_heap(RataArena *arena, size_t capacity) {
    if (arena == NULL || capacity == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    uint8_t *mem = (uint8_t *)malloc(capacity);
    if (mem == NULL) {
        arena->base = NULL;
        arena->cap = 0u;
        arena->offset = 0u;
        arena->owns_buffer = false;
        return RATA_ERR_OUT_OF_MEMORY;
    }
    arena->base = mem;
    arena->cap = capacity;
    arena->offset = 0u;
    arena->owns_buffer = true;
    return RATA_OK;
}

void rata_arena_destroy(RataArena *arena) {
    if (arena == NULL) {
        return;
    }
    if (arena->owns_buffer && arena->base != NULL) {
        free(arena->base);
    }
    arena->base = NULL;
    arena->cap = 0u;
    arena->offset = 0u;
    arena->owns_buffer = false;
}

void *rata_arena_alloc(RataArena *arena, size_t size, size_t align) {
    if (arena == NULL || arena->base == NULL) {
        return NULL;
    }
    if (align == 0u) {
        align = 8u;
    }
    if (!is_power_of_two(align)) {
        return NULL;
    }
    if (size == 0u) {
        size = 1u;
    }

    uintptr_t curr_addr = (uintptr_t)(arena->base + arena->offset);
    uintptr_t mask = (uintptr_t)(align - 1u);
    uintptr_t misalign = curr_addr & mask;
    size_t pad = (misalign == 0u) ? 0u : (size_t)(align - (size_t)misalign);

    if (pad > arena->cap - arena->offset) {
        return NULL;
    }
    size_t aligned_offset = arena->offset + pad;
    if (size > arena->cap - aligned_offset) {
        return NULL;
    }

    void *ptr = (void *)(arena->base + aligned_offset);
    arena->offset = aligned_offset + size;
    return ptr;
}

void *rata_arena_calloc(RataArena *arena, size_t count, size_t elem_size, size_t align) {
    if (count == 0u || elem_size == 0u) {
        return rata_arena_alloc(arena, 1u, align);
    }
    if (count > ((size_t)-1) / elem_size) {
        return NULL;
    }
    size_t total = count * elem_size;
    void *ptr = rata_arena_alloc(arena, total, align);
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void rata_arena_reset(RataArena *arena) {
    if (arena != NULL) {
        arena->offset = 0u;
    }
}

size_t rata_arena_mark(const RataArena *arena) {
    if (arena == NULL) {
        return 0u;
    }
    return arena->offset;
}

void rata_arena_restore(RataArena *arena, size_t mark) {
    if (arena != NULL && mark <= arena->offset) {
        arena->offset = mark;
    }
}

size_t rata_arena_used(const RataArena *arena) {
    if (arena == NULL) {
        return 0u;
    }
    return arena->offset;
}

size_t rata_arena_remaining(const RataArena *arena) {
    if (arena == NULL || arena->offset > arena->cap) {
        return 0u;
    }
    return arena->cap - arena->offset;
}
