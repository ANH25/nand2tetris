#if !defined(J_TOKENIZER_H)
#define J_TOKENIZER_H

#include <stdio.h>
#include <stdbool.h>

#include "jack-token.h"
#include "n2t-common.h"

struct j_tokenizer {
	FILE *src;
	char *src_name;
	size_t line_n;
	struct j_token token; // current token
	bool has_more_tokens;
};


#define j_tokenizer_error(jtk, msg) n2t_error((jtk)->src_name, (jtk)->line_n, msg)
#define j_tokenizer_error_f(jtk, fmt, ...) n2t_error_f((jtk)->src_name, (jtk)->line_n, fmt, __VA_ARGS__)
#define j_tokenizer_warn(jtk, msg) n2t_warn((jtk)->src_name, (jtk)->line_n, msg)
#define j_tokenizer_warn_f(jtk, fmt, ...) n2t_warn((jtk)->src_name, (jtk)->line_n, fmt, __VA_ARGS__)


void j_tokenizer_write_xml_el(struct j_tokenizer *jtk, FILE *dest);

void j_tokenizer_init(struct j_tokenizer *jtk);

int j_tokenizer_advance(struct j_tokenizer *jtk);

int j_tokenizer_retreat(struct j_tokenizer *jtk);

void j_tokenizer_free(struct j_tokenizer *jtk);

#endif
