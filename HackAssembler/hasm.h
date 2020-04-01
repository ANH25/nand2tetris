#if !defined(HASM_H)
#define HASM_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>


struct hasm {
	FILE *src;
	char *src_path;
	FILE *dest;
	char *dest_path;
	char *line;
	size_t line_buf_size;
	size_t line_len;
	size_t line_n;
	uint16_t inst_count;
	bool strict;
	bool quiet;
};

typedef enum hasm_state {
	hasm_success=0,
	hasm_oom,
	hasm_syntax_error,
	hasm_invalid_address,
	hasm_invalid_label,
	hasm_redefined_label,
	hasm_src_too_big,
	hasm_io_error,
	hasm_unknown
} hasm_state;

#define hasm_error(hasm, msg) \
	((hasm)->quiet ? (void)0 : n2t_error((hasm)->src_path, (hasm)->line_n, msg))

#define hasm_error_f(hasm, fmt, ...) \
	((hasm)->quiet ? (void)0 : n2t_error_f((hasm)->src_path, (hasm)->line_n, fmt, __VA_ARGS__))

#define hasm_warn(hasm, msg) \
	((hasm)->quiet ? (void)0 : n2t_warn((hasm)->src_path, (hasm)->line_n, msg))

#define hasm_warn_f(hasm, fmt, ...) \
	((hasm)->quiet ? (void)0 : n2t_warn_f((hasm)->src_path, (hasm)->line_n, fmt, __VA_ARGS__))

#define hasm_func_error(hasm, msg) \
	((hasm)->quiet ? (void)0 : n2t_func_error(msg))
	
#define hasm_func_error_f(hasm, msg, ...) \
	((hasm)->quiet ? (void)0 : n2t_func_error_f(msg, __VA_ARGS__))


void hasm_init(struct hasm *hasm);

void hasm_free(struct hasm *hasm);

hasm_state hasm_assemble_n(struct hasm *hasm, uint16_t max_inst_count);

hasm_state hasm_assemble(struct hasm *hasm);

hasm_state hasm_assemble_line(struct hasm *hasm);

#endif
