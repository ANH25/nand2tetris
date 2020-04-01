#if !defined(HVM_H)
#define HVM_H

#include <stdio.h>
#include <stdbool.h>

#include "n2t-common.h"

struct vm {
	/* name derived from user-provided source file or directory to be used for label generation */
	char *base_name;
	char *src_path;
	FILE *src;
	char *dest_path;
	FILE *dest;
	struct vm_line *line;
	char *cur_func;
	bool has_return;
	bool annotated;
	bool noinit;
	bool quiet;
};

#define hvm_error(hvm, msg) \
	((hvm)->quiet ? (void)0 : n2t_error((hvm)->src_path, (hvm)->line->line_num, msg))

#define hvm_error_f(hvm, fmt, ...) \
	((hvm)->quiet ? (void)0 : n2t_error_f((hvm)->src_path, (hvm)->line->line_num, fmt, __VA_ARGS__))

#define hvm_warn(hvm, msg) \
	((hvm)->quiet ? (void)0 : n2t_warn((hvm)->src_path, (hvm)->line->line_num, msg))

#define hvm_warn_f(hvm, fmt, ...) \
	((hvm)->quiet ? (void)0 : n2t_warn_f((hvm)->src_path, (hvm)->line->line_num, fmt, __VA_ARGS__))

#define hvm_func_error(hvm, msg) \
	((hvm)->quiet ? (void)0 : n2t_func_error(msg))
	
#define hvm_func_error_f(hvm, msg, ...) \
	((hvm)->quiet ? (void)0 : n2t_func_error_f(msg, __VA_ARGS__))

void vm_init(struct vm *vm);

void vm_free(struct vm *vm);

int vm_translate(struct vm *vm);

#endif
