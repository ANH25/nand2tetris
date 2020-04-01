#if !defined(HVM_H)
#define HVM_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "n2t-common.h"
#include "hvm-defs.h"

struct vm {
	char *base_name;
	VEC(struct vm_src_range) src_ranges;
	size_t cur_src_i;
	char *src_path;
	FILE *src;
	char *dest_path;
	FILE *dest;
	size_t line_n;
	n2tstr cur_func;
	uint16_t call_i;
	uint16_t gt_i;
	uint16_t lt_i;
	uint16_t eq_i;
	VEC(struct vm_cmd) vm_cmds;
	size_t cur_cmd_i;
	struct vm_func_t *functions;
	
	bool has_return;
	bool has_call;
	bool annotated;
	bool noinit;
	bool quiet;
	bool strict;
};

#define hvm_error(hvm, msg) \
	((hvm)->quiet ? (void)0 : n2t_error((hvm)->src_path, (hvm)->line_n, msg))

#define hvm_error_f(hvm, fmt, ...) \
	((hvm)->quiet ? (void)0 : n2t_error_f((hvm)->src_path, (hvm)->line_n, fmt, __VA_ARGS__))

#define hvm_warn(hvm, msg) \
	((hvm)->quiet ? (void)0 : n2t_warn((hvm)->src_path, (hvm)->line_n, msg))

#define hvm_warn_f(hvm, fmt, ...) \
	((hvm)->quiet ? (void)0 : n2t_warn_f((hvm)->src_path, (hvm)->line_n, fmt, __VA_ARGS__))

#define hvm_func_error(hvm, msg) \
	((hvm)->quiet ? (void)0 : n2t_func_error(msg))
	
#define hvm_func_error_f(hvm, fmt, ...) \
	((hvm)->quiet ? (void)0 : n2t_func_error_f(fmt, __VA_ARGS__))

#define hvm_func_warn(hvm, msg) \
	((hvm)->quiet ? (void)0 : n2t_func_warn(msg))
	
#define hvm_func_warn_f(hvm, fmt, ...) \
	((hvm)->quiet ? (void)0 : n2t_func_warn_f(fmt, __VA_ARGS__))
	


int vm_init(struct vm *vm);

int vm_analyze(struct vm *vm);

int vm_translate(struct vm *vm);

void vm_free(struct vm *vm);


#endif
