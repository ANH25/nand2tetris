#if !defined(VM_DEFS_H)
#define VM_DEFS_H

#include <stdio.h>
#include <stdbool.h>


/* do not abort on oom conditions */
#define HASH_NONFATAL_OOM 1
/* makes sense only in vm_analyze but that's fine
   since we don't use the HASH_ADD_* functions in other places */
#define uthash_nonfatal_oom(obj) do {ret = 0; goto CLEAN;} while(0)
#include "uthash.h"

struct vm;

struct vm_line {
	/* the Hack virtual machine language commands has three tokens at most.
	   for each token we store its start and end indices */
	size_t tokens_indices[6];
	unsigned char token_num;
	char *line;
	size_t line_num;
};

struct vm_segment_extra {
	const char *asm_name;
};

struct vm_segment {
	const char *name; 
	int (*push_op)(struct vm *vm, struct vm_segment_extra *extra);
	int (*pop_op)(struct vm *vm, struct vm_segment_extra *extra);
	struct vm_segment_extra extra;
};


typedef enum vm_cmd_type {
	vm_cmd_push,
	vm_cmd_pop,
	vm_cmd_label,
	vm_cmd_add,
	vm_cmd_sub,
	vm_cmd_neg,
	vm_cmd_eq,
	vm_cmd_gt,
	vm_cmd_lt,
	vm_cmd_and,
	vm_cmd_or,
	vm_cmd_not,
	vm_cmd_goto,
	vm_cmd_ifgoto,
	vm_cmd_call,
	vm_cmd_function,
	vm_cmd_return
} vm_cmd_type;

struct vm_cmd {
	const char *name;
	vm_cmd_type type;
	union {
		char *segment;
		char *label;
		char *function;
	};
	union {
		uint16_t address;
		uint16_t arg_n;
		uint16_t local_n;
	};
	unsigned char token_num;
	struct vm_line vmline;
};

struct vm_func_t {
	struct vm_cmd func;
	uint16_t func_cmd_i;
	struct vm_cmd ret;
	uint16_t ret_cmd_i;
	bool scanned;
	UT_hash_handle hh;
} vm_func_t;


struct vm_src_range {
	size_t start;
	size_t end;
} vm_src_range;


int vm_cmd_push_pop_dispatch(struct vm *vm);

int vm_segment_translate_push_constant(struct vm *vm, struct vm_segment_extra *extra);

/* 'basic' just means the local/argument/this/that segments since they have similar implementations */
int vm_segment_translate_push_basic(struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_pop_basic(struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_push_static(struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_push_pointer(struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_push_temp(struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_pop_pointer(struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_pop_temp(struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_error_pop_constant(struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_pop_basic(struct vm *vm, struct vm_segment_extra *extra);

int vm_segment_translate_pop_static(struct vm *vm, struct vm_segment_extra *extra);

int vm_cmd_translate_add(struct vm *vm);

int vm_cmd_translate_sub(struct vm *vm);

int vm_cmd_translate_eq(struct vm *vm);

int vm_cmd_translate_neg(struct vm *vm);

int vm_cmd_translate_gt(struct vm *vm);

int vm_cmd_translate_lt(struct vm *vm);

int vm_cmd_translate_and(struct vm *vm);

int vm_cmd_translate_or(struct vm *vm);

int vm_cmd_translate_not(struct vm *vm);

int vm_cmd_translate_label(struct vm *vm);

int vm_cmd_translate_goto(struct vm *vm);

int vm_cmd_translate_ifgoto(struct vm *vm);

int vm_cmd_translate_call(struct vm *vm);

int vm_cmd_translate_function(struct vm *vm);

int vm_cmd_translate_return(struct vm *vm);

typedef int (*vm_cmd_func) (struct vm *vm);
const static vm_cmd_func vm_cmd_functions[] = {
	[vm_cmd_push] = vm_cmd_push_pop_dispatch,
	[vm_cmd_pop] = vm_cmd_push_pop_dispatch,
	[vm_cmd_label] = vm_cmd_translate_label,
	[vm_cmd_add] = vm_cmd_translate_add,
	[vm_cmd_sub] = vm_cmd_translate_sub,
	[vm_cmd_neg] = vm_cmd_translate_neg,
	[vm_cmd_eq] = vm_cmd_translate_eq,
	[vm_cmd_gt] = vm_cmd_translate_gt,
	[vm_cmd_lt] = vm_cmd_translate_lt,
	[vm_cmd_and] = vm_cmd_translate_and,
	[vm_cmd_or] = vm_cmd_translate_or,
	[vm_cmd_not] = vm_cmd_translate_not,
	[vm_cmd_goto] = vm_cmd_translate_goto,
	[vm_cmd_ifgoto] = vm_cmd_translate_ifgoto,
	[vm_cmd_call] = vm_cmd_translate_call,
	[vm_cmd_function] = vm_cmd_translate_function,
	[vm_cmd_return] = vm_cmd_translate_return
};

#endif
