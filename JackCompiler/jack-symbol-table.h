#if !defined(J_SYMBOL_TABLE_H)
#define J_SYMBOL_TABLE_H

#include <stdint.h>
#include "uthash.h"

typedef enum j_var_kind {
	j_var_unknown = 0,
	j_var_local,
	j_var_argument,
	j_var_field,
	j_var_static
} j_var_kind;

struct j_symbol_t {
	char *name;
	char *type; /* 'int', 'char', 'boolean', or a class name */
	j_var_kind kind;
	uint16_t index;
	UT_hash_handle hh;
};

#endif
