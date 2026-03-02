#ifndef POS_META_H
#define POS_META_H

#include <stddef.h>
#define F_MEMORY_ARENA 1

#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef i32 bool32;
typedef i8 bool8;

typedef float real32;
typedef double real64;

#define internal static
#define global static
#define persist static

#define globalconst static const 

#define bytes(n) (n)
#define Kbytes(n) (bytes(n) * 1024)
#define Mbytes(n) (Kbytes(n) * 1024)
#define Gbytes(n) (Mbytes((u64)n) * 1024) 
#define Tbytes(n) (Gbytes((u64)n) * 1024) 

#define S(X) #X
#define S_(X) S(X)
#define S__LINE__ S_(__LINE__)

#define LOG(msg) \
    printf(__FILE__ ":" S__LINE__ ": " msg "\n")

#define Bit_0 (1 << 0)
#define Bit_1 (1 << 1)
#define Bit_2 (1 << 2)
#define Bit_3 (1 << 3)
#define Bit_4 (1 << 4)
#define Bit_5 (1 << 5)
#define Bit_6 (1 << 6)
#define Bit_7 (1 << 7)
#define Bit_8 (1 << 8)

#define Byte_0 (0xffu)
#define Byte_1 (0xffu << 8)
#define Byte_2 (0xffu << 16)
#define Byte_3 (0xffu << 24)
#define Byte_4 (0xffu << 32)
#define Byte_5 (0xffu << 40)
#define Byte_6 (0xffu << 48)
#define Byte_7 (0xffu << 56)

#ifdef F_MEMORY_DEBUG

/* ----- Debugging -----
If F_MEMORY_DEBUG  is enabled, the memory debugging system will create macros that replace malloc, free and realloc and allows the system to keppt track of and report where memory is beeing allocated, how much and if the memory is beeing freed. This is very useful for finding memory leaks in large applications. The system can also over allocate memory and fill it with a magic number and can therfor detect if the application writes outside of the allocated memory. if F_EXIT_CRASH is defined, then exit(); will be replaced with a funtion that writes to NULL. This will make it trivial ti find out where an application exits using any debugger., */

extern void f_debug_memory_init(void (*lock)(void *mutex), void (*unlock)(void *mutex), void *mutex); /* Required for memory debugger to be thread safe */
extern void *f_debug_mem_malloc(uint size, char *file, uint line); /* Replaces malloc and records the c file and line where it was called*/
extern void *f_debug_mem_realloc(void *pointer, uint size, char *file, uint line); /* Replaces realloc and records the c file and line where it was called*/
extern void f_debug_mem_free(void *buf); /* Replaces free and records the c file and line where it was called*/
extern void f_debug_mem_print(uint min_allocs); /* Prints out a list of all allocations made, their location, how much memorey each has allocated, freed, and how many allocations have been made. The min_allocs parameter can be set to avoid printing any allocations that have been made fewer times then min_allocs */
extern void f_debug_mem_reset(); /* f_debug_mem_reset allows you to clear all memory stored in the debugging system if you only want to record allocations after a specific point in your code*/
extern boolean f_debug_memory(); /*f_debug_memory checks if any of the bounds of any allocation has been over written and reports where to standard out. The function returns TRUE if any error was found*/

#define malloc(n) f_debug_mem_malloc(n, __FILE__, __LINE__) /* Replaces malloc. */
#define realloc(n, m) f_debug_mem_realloc(n, m, __FILE__, __LINE__) /* Replaces realloc. */
#define free(n) f_debug_mem_free(n) /* Replaces free. */

#endif

#ifdef F_MEMORY_ARENA

#ifndef ARENA_ASSERT
#include <assert.h>
#define ARENA_ASSERT assert
#endif

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2*sizeof(void *))
#endif

typedef struct {
    unsigned char *buff;
    size_t capacity;
    size_t current_offset;
    size_t previous_offset;
}Arena;

void  arena_init(Arena* arena, void* buf, size_t buf_lenght);
void* arena_alloc(Arena* arena, size_t size);
void  arena_release_all(Arena *arena);
/* arena_push: allocate without zeroing (raw bump) */
void* arena_push(Arena *arena, size_t size);
/* arena_push_zero: allocate and zero the memory */
void* arena_push_zero(Arena* arena, size_t size);
#endif

#endif
