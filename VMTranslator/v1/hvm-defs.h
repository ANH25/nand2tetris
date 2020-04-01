#if !defined(VM_DEFS_H)
#define VM_DEFS_H

#include <stdio.h>
#include <stdbool.h>

#include "hvm.h"

struct vm_line {
	/* the Hack virtual machine language commands has three tokens at most.
	   for each token we store its start and end indices */
	size_t tokens_indices[6];
	unsigned char token_num;
	const char *line;
	size_t line_num;
};

struct vm_segment_extra {
	const char *asm_name;
	uint16_t address;
};


struct vm_segment {
	const char *name; 
	int (*push_op)(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra);
	int (*pop_op)(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra);
	struct vm_segment_extra extra;
};


int vm_cmd_push_pop_dispatch(struct vm_line *vmline, struct vm *vm);

int vm_segment_translate_push_constant(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra);

/* 'basic' just means the local/argument/this/that segments since they have similar implementations */
int vm_segment_translate_push_basic(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_pop_basic(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_push_static(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_push_pointer(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_push_temp(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_pop_pointer(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_pop_temp(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_error_pop_constant(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_pop_basic(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_pop_static(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra);

int vm_cmd_translate_add(struct vm_line *vmline, struct vm *vm);

int vm_cmd_translate_sub(struct vm_line *vmline, struct vm *vm);

int vm_cmd_translate_eq(struct vm_line *vmline, struct vm *vm);

int vm_cmd_translate_neg(struct vm_line *vmline, struct vm *vm);

int vm_cmd_translate_gt(struct vm_line *vmline, struct vm *vm);

int vm_cmd_translate_lt(struct vm_line *vmline, struct vm *vm);

int vm_cmd_translate_and(struct vm_line *vmline, struct vm *vm);

int vm_cmd_translate_or(struct vm_line *vmline, struct vm *vm);

int vm_cmd_translate_not(struct vm_line *vmline, struct vm *vm);

int vm_cmd_translate_label(struct vm_line *vmline, struct vm *vm);

int vm_cmd_translate_goto(struct vm_line *vmline, struct vm *vm);

int vm_cmd_translate_ifgoto(struct vm_line *vmline, struct vm *vm);

int vm_cmd_translate_call(struct vm_line *vmline, struct vm *vm);

int vm_cmd_translate_function(struct vm_line *vmline, struct vm *vm);

int vm_cmd_translate_return(struct vm_line *vmline, struct vm *vm);

#endif
