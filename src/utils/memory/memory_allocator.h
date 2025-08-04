#ifndef ARISE_MEMORY_ALLOCATOR_H
#define ARISE_MEMORY_ALLOCATOR_H

#include <cstddef>
#include <new>

// Custom global operator new[] overloads for EASTL
// These overloads are defined globally to handle EASTL's custom memory
// allocation requirements. They are currently implemented to mirror the
// behavior of the standard global operator new[], but can be modified for
// custom allocation strategies or debugging purposes.

inline void* operator new[](std::size_t size,
                            const char* /*name*/,
                            int /*flags*/,
                            unsigned /*debugFlags*/,
                            const char* /*file*/,
                            int /*line*/) {
  return ::operator new[](size);
}

inline void* operator new[](std::size_t size,
                            std::size_t alignment,
                            std::size_t /*alignmentOffset*/,
                            const char* /*name*/,
                            int /*flags*/,
                            unsigned /*debugFlags*/,
                            const char* /*file*/,
                            int /*line*/) {
  return ::operator new[](size, std::align_val_t(alignment));

  // For older C++ standards, might be needed platform-specific alignment
  // handling. Example for Windows: return _aligned_malloc(size, alignment);
}

#endif  // ARISE_MEMORY_ALLOCATOR_H