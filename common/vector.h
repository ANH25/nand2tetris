#if !defined(VEC_H)
#define VEC_H

#include <stddef.h>
#include <stdlib.h>

#if !defined(VEC_INIT_CAP)
#define VEC_INIT_CAP 8
#endif

#if !defined(VEC_GROWTH_FACTOR)
#define VEC_GROWTH_FACTOR 2
#endif

#define VEC(T) struct { \
	T *data; \
	T *tmp; \
	size_t length; \
	size_t capacity; \
	unsigned char state; \
}

#define VEC_DEF(name, T) \
typedef struct name { \
	T *data; \
	T *tmp; \
	size_t length; \
	size_t capacity; \
	unsigned char state; \
} name

#define VEC_SUCCESS 0
#define VEC_OVERFLOW 1
#define VEC_NOMEM 2

#define VEC_CHECK_MUL_OVERFLOW_(op1, op2) ((op1) <= ((size_t)-1 / (op2)))
#define VEC_CHECK_SIZE_(vec, cap) VEC_CHECK_MUL_OVERFLOW_(cap, sizeof((vec).data[0]))
#define VEC_CHECK_ADD_OVERFLOW_(op1, op2) ((op1) <= ((size_t)-1 - (op2)))

#ifndef vec_malloc
#define vec_malloc(size) malloc(size)
#endif
#ifndef vec_realloc
#define vec_realloc(p, size) realloc(p, size)
#endif
#ifndef vec_free
#define vec_free(p) free(p)
#endif


#define VEC_INIT_N_(vec, cap) \
	( \
	 ((vec).data = vec_malloc((cap) * sizeof((vec).data[0]))) ? \
	  ((vec).length = 0, (vec).capacity = (cap), (vec).state = VEC_SUCCESS) :  \
	  ((vec).state = VEC_NOMEM) \
	)

#define VEC_INIT_N_S(vec, cap) \
		(VEC_CHECK_SIZE_((vec), (cap)) ? \
		VEC_INIT_N_((vec), (cap)) : \
		((vec).state = VEC_OVERFLOW) \
		)

#if !defined(VEC_NO_CHECKS)
#define VEC_INIT_N(vec, cap) VEC_INIT_N_S(vec, cap)
#else
#define VEC_INIT_N(vec, cap) VEC_INIT_N_(vec, cap)
#endif

#define VEC_INIT(vec) VEC_INIT_N((vec), VEC_INIT_CAP)

#define VEC_RESIZE_(vec, new_cap) \
	(((vec).tmp = vec_realloc((vec).data, (new_cap) * sizeof((vec).data[0]))) ? \
	 ((vec).data = (vec).tmp, (vec).capacity = (new_cap), (vec).state = VEC_SUCCESS) : \
	 ((vec).state = VEC_NOMEM) \
	)

#define VEC_RESIZE_S(vec, new_cap) \
		(VEC_CHECK_SIZE_((vec), (new_cap)) ? \
		VEC_RESIZE_((vec), (new_cap)) : \
		((vec).state = VEC_OVERFLOW))

#if !defined(VEC_NO_CHECKS)
#define VEC_RESIZE(vec, new_cap) VEC_RESIZE_S(vec, new_cap)
#else
#define VEC_RESIZE(vec, new_cap) VEC_RESIZE_(vec, new_cap)
#endif


#define VEC_GROW_IF_NEEDED_(vec, gfactor) \
	((vec).length >= (vec).capacity ? \
	 VEC_RESIZE((vec), (vec).capacity * (gfactor)) : \
	 ((vec).state = VEC_SUCCESS) \
	)

#define VEC_GROW_IF_NEEDED_S(vec, gfactor) \
	(VEC_CHECK_MUL_OVERFLOW_((vec).capacity, (gfactor)) ? \
	VEC_GROW_IF_NEEDED_((vec), (gfactor)) : \
	((vec).state = VEC_OVERFLOW) \
	)

#if !defined(VEC_NO_CHECKS)
#define VEC_GROW_IF_NEEDED(vec, gfactor) VEC_GROW_IF_NEEDED_S(vec, gfactor)
#else
#define VEC_GROW_IF_NEEDED(vec, gfactor) VEC_GROW_IF_NEEDED_(vec, gfactor)
#endif


#define VEC_PUSH_G_(vec, gfactor, item) \
	(VEC_GROW_IF_NEEDED((vec), (gfactor)) == VEC_SUCCESS ? \
	 ((vec).data[(vec).length] = (item), (vec).length++, VEC_SUCCESS) : \
	 (vec).state \
	)

#define VEC_PUSH_G_S(vec, gfactor, item) \
	(VEC_CHECK_ADD_OVERFLOW_((vec.length), 1) ? \
	VEC_PUSH_G_(vec, gfactor, item) : \
	((vec).state = VEC_OVERFLOW) \
	)

#if !defined(VEC_NO_CHECKS)
#define VEC_PUSH_G(vec, gfactor, item) VEC_PUSH_G_S(vec, gfactor, item)
#else
#define VEC_PUSH_G(vec, gfactor, item) VEC_PUSH_G_(vec, gfactor, item)
#endif


#if !defined(VEC_NO_CHECKS)
#define VEC_PUSH(vec, item) VEC_PUSH_G_S(vec, VEC_GROWTH_FACTOR, item)
#else
#define VEC_PUSH(vec, item) VEC_PUSH_G_(vec, VEC_GROWTH_FACTOR, item)
#endif


#define VEC_SHRINK_IF_NEEDED(vec, gfactor) \
	((vec).capacity >= (gfactor)*2 && (vec).length <= (vec).capacity/((gfactor)*2) ? \
	 VEC_RESIZE((vec), (vec).capacity / (gfactor)) : \
	 ((vec).state = VEC_SUCCESS) \
	)

#define VEC_POP_G_(vec, gfactor) \
	(VEC_SHRINK_IF_NEEDED((vec), (gfactor)) == VEC_SUCCESS ? \
	(&(vec).data[--(vec).length], VEC_SUCCESS) : \
	(vec).state)

#define VEC_POP_G_S(vec, gfactor) \
	((vec).length ? \
	 VEC_POP_G_(vec, gfactor) : \
	 NULL)

#if !defined(VEC_NO_CHECKS)
#define VEC_POP_G(vec, gfactor) VEC_POP_G_S(vec, gfactor)
#else
#define VEC_POP_G(vec, gfactor) VEC_POP_G_(vec, gfactor)
#endif


#define VEC_POP_(vec) (&(vec).data[--(vec).length])

#define VEC_POP_S(vec) ( \
	(vec).length ? \
	VEC_POP_(vec) : \
	NULL)


#if defined(VEC_SHRINK)
#define VEC_POP(vec) VEC_POP_G((vec), VEC_GROWTH_FACTOR)

#else

/* 
Popping items safely with VEC_POP_S versus VEC_POP_ require different usage patterns (see below)
unlike the other VEC_*_S vs VEC_*_ functions which have identical interfaces
so I define VEC_POP() as VEC_POP_S() always regardless of the VEC_NO_CHECKS macro   
   
	// when VEC_NO_CHECKS is not defined
	while((item = VEC_POP(vec))) {
	   // ...
	}
	
	// otherwise, to pop all elements safely we need to store the vector length first
	   since VEC_POP (aka VEC_POP_ ) in this case have no way of reporting that the vector is empty
	size_t vec_len = vec.length;
	for(size_t j = 0; j < vec_len; j++) {
		item = VEC_POP(vec);
	}
*/
#define VEC_POP(vec) VEC_POP_S(vec)

#endif


#define VEC_AT_(vec, i) (&(vec).data[(i)])

#define VEC_AT_S(vec, i) \
	((vec.length) > (i) ? \
	VEC_AT_(vec, i) : \
	NULL)

#if !defined(VEC_NO_CHECKS)
#define VEC_AT(vec, i) VEC_AT_S(vec, i)
#else
#define VEC_AT(vec, i) VEC_AT_(vec, i)
#endif

#define VEC_IS_EMPTY(vec) (!(vec).length)

#define VEC_FREE(vec) \
	(vec_free((vec).data), \
	(vec).data = NULL, \
	(vec).capacity = 0, \
	(vec).length = 0, \
	(vec).state = VEC_SUCCESS \
	)


#endif
