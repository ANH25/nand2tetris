#if !defined(JACK_COMPILER_H)
#define JACK_COMPILER_H

#include <stdint.h>

#include "jack-tokenizer.h"
#include "jack-symbol-table.h"

//#define JC_XML


struct j_compiler {
	struct j_tokenizer tokenizer;
	FILE *dest;
//#ifndef JC_XML
	struct j_symbol_t *class_symbol_table;
	struct j_symbol_t *subroutine_symbol_table;
	struct {
		char *name;
		size_t buf_size;
		size_t len;
	} cur_class;
	uint16_t var_kind_indices[5];
	uint16_t if_label_n;
	uint16_t while_label_n;
//#endif
};

typedef int (*jc_func) (struct j_compiler *jc);


#ifdef JC_XML
#define jc_write_xml_el(jc) j_tokenizer_write_xml_el(&(jc)->tokenizer, (jc)->dest)
#define jc_write_xml(jc, outstr) fputs(outstr, (jc)->dest)
#define jc_write_vm(jc, outstr)
#define jc_write_vm_f(jc, fmt, ...)
#else
#define jc_write_xml_el(jc)
#define jc_write_xml(jc, outstr)
#define jc_write_vm(jc, outstr) fputs(outstr, (jc)->dest)
#define jc_write_vm_f(jc, fmt, ...) fprintf((jc)->dest, fmt, __VA_ARGS__)
#endif


#define jc_error(jc, msg) j_tokenizer_error(&(jc)->tokenizer, msg)
#define jc_error_f(jc, fmt, ...) j_tokenizer_error_f(&(jc)->tokenizer, fmt, __VA_ARGS__)
#define jc_warn(jc, msg) j_tokenizer_warn(&(jc)->tokenizer, msg)
#define jc_warn_f(jc, fmt, ...) j_tokenizer_warn_f(&(jc)->tokenizer, fmt, __VA_ARGS__)


int jc_init(struct j_compiler *jc);

void jc_free(struct j_compiler *jc);

int jc_compile_term(struct j_compiler *jc);

int jc_compile_expression(struct j_compiler *jc);

uint16_t jc_compile_expression_list(struct j_compiler *jc);

int jc_compile_return(struct j_compiler *jc);

int jc_compile_subroutine_call(struct j_compiler *jc);

int jc_compile_do(struct j_compiler *jc);

int jc_compile_let(struct j_compiler *jc);

int jc_compile_while(struct j_compiler *jc);

int jc_compile_if_else(struct j_compiler *jc);

int jc_compile_statements(struct j_compiler *jc);

int jc_compile_type(struct j_compiler *jc);

int jc_compile_parameter_list(struct j_compiler *jc, j_keyword_type subroutine_type);

uint16_t jc_compile_var_dec(struct j_compiler *jc);

int jc_compile_subroutine_dec(struct j_compiler *jc);

int jc_compile_class_var_dec(struct j_compiler *jc);

int jc_compile_class(struct j_compiler *jc);


#endif
