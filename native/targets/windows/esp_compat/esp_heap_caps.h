#pragma once
// Ultima V Native for Windows: ESP-IDF's capability allocator. The T-Deck
// steers large buffers to PSRAM and small ones to internal RAM; a desktop has
// one heap, so every capability is the C heap. The free-size queries report
// the process's available virtual memory (capped), which the runtime only
// prints in diagnostics and compares against its own reserve thresholds.
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#define MALLOC_CAP_EXEC (1 << 0)
#define MALLOC_CAP_32BIT (1 << 1)
#define MALLOC_CAP_8BIT (1 << 2)
#define MALLOC_CAP_DMA (1 << 3)
#define MALLOC_CAP_SPIRAM (1 << 10)
#define MALLOC_CAP_INTERNAL (1 << 11)
#define MALLOC_CAP_DEFAULT (1 << 12)

inline void *heap_caps_malloc(size_t size, uint32_t) { return std::malloc(size ? size : 1); }
inline void *heap_caps_calloc(size_t n, size_t size, uint32_t) { return std::calloc(n ? n : 1, size ? size : 1); }
inline void *heap_caps_realloc(void *p, size_t size, uint32_t) { return std::realloc(p, size ? size : 1); }
inline void heap_caps_free(void *p) { std::free(p); }
size_t heap_caps_get_free_size(uint32_t caps);
size_t heap_caps_get_largest_free_block(uint32_t caps);
size_t heap_caps_get_minimum_free_size(uint32_t caps);
size_t heap_caps_get_total_size(uint32_t caps);
