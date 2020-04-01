#if !defined(J_TOKEN_H)
#define J_TOKEN_H

#include <stddef.h>
#include <stdio.h>

typedef enum j_token_type {j_unknown = 0, j_keyword, j_symbol, j_int, j_str, j_identifier} j_token_type;

/* NOTE: DO NOT change the below enum identifiers positions carelessly!
   some code depends on their positions to work correctly */
typedef enum j_keyword_type {
	j_keyword_unknown = 0, 
	j_keyword_class,
	j_keyword_method,
	j_keyword_function,
	j_keyword_constructor,
	j_keyword_int, 
	j_keyword_boolean,
	j_keyword_char,
	j_keyword_void,
	j_keyword_var,
	j_keyword_static,
	j_keyword_field,
	j_keyword_let,
	j_keyword_do,
	j_keyword_if,
	j_keyword_else,
	j_keyword_while,
	j_keyword_return,
	j_keyword_true,
	j_keyword_false,
	j_keyword_null,
	j_keyword_this
} j_keyword_type;

typedef enum j_symbol_type {
	j_symbol_unknown = 0,
	j_symbol_op,
} j_symbol_type;

struct j_token {
	char *name;
	size_t name_len;
	size_t name_buf_cap;
	j_token_type type;
	union {
		j_keyword_type keyword_type;
		j_symbol_type symbol_type;
	};
	fpos_t fpos;
	size_t line_n;
};


#endif
