/* Hack Assembler */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>

#include "n2t-common.h"
#include "vector.h"

#define HASH_NONFATAL_OOM 1
#include "uthash.h"

#include "predefined-symbols.h"
#include "comp-symbols.h"
#include "jump-symbols.h"
#include "hasm.h"


static void fprintbin(FILE *stream, uint16_t num) {
	size_t size = sizeof((num)) * CHAR_BIT;
	size --;
	for(; size != (size_t)-1; size--) {
		fprintf(stream, "%d", (int)(1 & ((num) >> size)));
	}
}

union instruction {
	
	uint16_t inst;
	struct {
		uint16_t j3: 1;
		uint16_t j2: 1;
		uint16_t j1: 1;
		uint16_t d3: 1;
		uint16_t d2: 1;
		uint16_t d1: 1;
		uint16_t c6: 1;
		uint16_t c5: 1;
		uint16_t c4: 1;
		uint16_t c3: 1;
		uint16_t c2: 1;
		uint16_t c1: 1;
		uint16_t a: 1;
		uint16_t op3: 1;
		uint16_t op2: 1;
		uint16_t op1: 1;
	};
};

inline static int dest(union instruction *inst, char *line, size_t *i) {
	
	unsigned char d1 = 0;
	unsigned char d2 = 0;
	unsigned char d3 = 0;
	size_t j;
	for(j = *i; j < *i+3; j++) {
		if(line[j] == 'A') {
			d1 = 1;
		}
		if(line[j] == 'D') {
			d2 = 1;
		}
		if(line[j] == 'M') {
			d3 = 1;
		}
		if(line[j] == '=') {
			break;
		}
	}
	while(isspace(line[j]) && line[j] != '=') {
		j++;
	}
	if(line[j] != '=') return 0;
	inst->d1 = d1;
	inst->d2 = d2;
	inst->d3 = d3;
	*i = j+1;
	return 1;
}

inline static int comp(union instruction *inst, char *line, size_t *i) {
	
	size_t j = *i;
	size_t k;
	char op[4];
	for(k = 0; k < 3; k++) {
		while(isspace(line[j]) && line[j] != ';') {
			j++;
		}
		if(line[j] == ';' || !line[j] || 
		  (line[j] == line[j+1] && line[j] == '/')) {
			break;
		}
		op[k] = line[j];
		j++;
	}
	op[k] = '\0';
	if(line[j] == ';') *i = j+1;
	else *i = j;
	
	const struct comp *comp_struct = get_comp(op, k);
	if(!comp_struct) return 0;
	inst->a = comp_struct->a;
	inst->c1 = comp_struct->c1;
	inst->c2 = comp_struct->c2;
	inst->c3 = comp_struct->c3;
	inst->c4 = comp_struct->c4;
	inst->c5 = comp_struct->c5;
	inst->c6 = comp_struct->c6;
	
	return 1;
}

inline static int jump(union instruction *inst, char *line, size_t *i) {
	
	size_t j = *i;
	size_t k;
	char op[4];
	for(k = 0; k < 3; k++) {
		while(isspace(line[j])) {
			j++;
		}
		if(!line[j]) break;
		op[k] = line[j];
		j++;
	}
	op[k] = '\0';
	*i = j;
	const struct jump *jump_struct = get_jump(op, k);
	if(!jump_struct) return 0;
	inst->j1 = jump_struct->j1;
	inst->j2 = jump_struct->j2;
	inst->j3 = jump_struct->j3;
	return 1;
}

struct user_symbol {
	char *name;
	/* array of file positions in destination file to insert the symbol's numerical address
	   later on when it's assigned
	*/
	VEC(fpos_t) pos_vec;
	UT_hash_handle hh;
};

struct label {
	char *name;
	uint16_t address;
	UT_hash_handle hh;
};

void hasm_init(struct hasm *hasm) {
	
	hasm->src = NULL;
	hasm->src_path = NULL;
	hasm->dest = NULL;
	hasm->dest_path = NULL;
	hasm->line = NULL;
	hasm->line_buf_size = 0;
	//hasm->line_len = 0;
	//hasm->line_n = 0;
	hasm->strict = false;
	hasm->quiet = false;
}

void hasm_free(struct hasm *hasm) {
	
	free(hasm->line);
	hasm->line = NULL;
	hasm->line_buf_size = 0;
}

hasm_state hasm_assemble_n(struct hasm *hasm, uint16_t max_inst_count) {
	
	struct user_symbol *user_symbols = NULL;
	struct label *labels = NULL;
	struct user_symbol *us_item, *us_tmp;
	struct label *l_item, *l_tmp;
	bool warned_about_big_src = false;
	hasm->inst_count = 0;
	hasm_state ret = hasm_success;
	hasm->line_n = 0;
	while((hasm->line_len = n2t_getline(&hasm->line, &hasm->line_buf_size, hasm->src)) != (size_t)-1) {
		
		hasm->line_n++;
		size_t i = 0;
		for(; isspace(hasm->line[i]); i++)
		;
		if(!hasm->line[i]) continue; // all-whitespace line
		if(hasm->line[i] == hasm->line[i+1] && hasm->line[i] == '/') continue; // comment
		
		if(hasm->line[i] == '@') {
			/* A-instruction */
			i++;
			while(isspace(hasm->line[i])) {
				i++;
			}
			
			if(isdigit(hasm->line[i]) || 
			   ( (hasm->line[i] == '-' || hasm->line[i] == '+') &&
			     isdigit(hasm->line[i+1])
			   )
			  ) {
				/* explicit address => easy-peasy! */
				errno = 0;
				char *end = &hasm->line[i];
				errno = 0;
				intmax_t addr = strtoimax(&hasm->line[i], &end, 10);
				if((errno == ERANGE && (addr == INTMAX_MAX || addr == INTMAX_MIN))
					|| addr > HACK_ADDRESS_MAX || addr < 0) {
					hasm_error_f(hasm, "expected a numerical address in the range [0, %u]",
					HACK_ADDRESS_MAX);
					ret = hasm_invalid_address;
					goto CLEAN;
				}
				if(end[0] && !isspace(end[0]) && !(end[0] == '/' && end[0] == end[1])) {
					
					if(hasm->strict) {
						hasm_error(hasm, "garbage character(s) after numerical address in A-instruction");
						ret = hasm_invalid_address;
						goto CLEAN;
					}
					else {
						hasm_warn(hasm, "garbage character(s) after numerical address in A-instruction. ignoring");
					}
				}
				fprintbin(hasm->dest, addr);
				putc('\n', hasm->dest);
			}
			else {
				/* symbolic address, possibly */
				size_t symbol_len = 0;
				for(;
				hasm->line[i + symbol_len] &&
				!isspace(hasm->line[i + symbol_len]) && 
				hasm->line[i + symbol_len] != '\n' &&
				!(hasm->line[i + symbol_len] == hasm->line[i + symbol_len + 1] &&
				  hasm->line[i + symbol_len] == '/'
				 );
				symbol_len++)
				;
				if(!symbol_len) {
					hasm_error(hasm, "expected an address after '@'");
					ret = hasm_syntax_error;
					goto CLEAN;
				}
				
				hasm->line[i + symbol_len] = '\0';
				const struct symbol *predef_sym = get_predefined_symbol(&hasm->line[i], symbol_len);
				if(predef_sym) {
					fprintbin(hasm->dest, predef_sym->value);
					putc('\n', hasm->dest);
				}
				else {
					/* Not a predefined symbol.
					could be a variable or a label */
					
					char *symbol_str = n2t_strdup_n(&hasm->line[i], symbol_len);
					if(!symbol_str) {
						hasm_func_error(hasm, "out of memory");
						ret = hasm_oom;
						goto CLEAN;
					}
					
					struct label *l;
					HASH_FIND_STR(labels, symbol_str, l);
					if(l) {
						/* This is a label that was defined before. Just get its address */
						fprintbin(hasm->dest, l->address);
						putc('\n', hasm->dest);
						n2t_free(symbol_str);
					}
					else {
						struct user_symbol *usr_sym;
						HASH_FIND_STR(user_symbols, symbol_str, usr_sym);
						if(!usr_sym) {
							usr_sym = n2t_malloc(sizeof(struct user_symbol));
							if(!usr_sym) {
								n2t_free(symbol_str);
								hasm_func_error(hasm, "out of memory");
								ret = hasm_oom;
								goto CLEAN;
							}
							
							usr_sym->name = symbol_str;
							if(VEC_INIT(usr_sym->pos_vec)) {
								n2t_free(symbol_str);
								hasm_func_error(hasm, "out of memory");
								ret = hasm_oom;
								goto CLEAN;
							}
							
							#undef uthash_nonfatal_oom
							#define uthash_nonfatal_oom(obj) do { \
								n2t_free(symbol_str); \
								n2t_free(usr_sym); \
								hasm_func_error(hasm, "out of memory"); \
								ret = hasm_oom; \
								goto CLEAN; \
							} while(0)
							
							HASH_ADD_KEYPTR( hh, user_symbols, symbol_str, symbol_len, usr_sym);
						}
						else {
							n2t_free(symbol_str);
						}
						fpos_t cur_pos;
						fgetpos(hasm->dest, &cur_pos);
						if(VEC_PUSH(usr_sym->pos_vec, cur_pos)) {
							hasm_func_error(hasm, "VEC_PUSH() failed. most likely an out-of-memory condition");
							ret = hasm_unknown;
							goto CLEAN;
						}
						/* make room for the instruction */
						fputs("XXXXXXXXXXXXXXXX\n", hasm->dest);
					}
				}
			}
			hasm->inst_count++;
		}
		else if(hasm->line[i] == '(') {
			/* Possibly a label */
			i++;
			if(isdigit(hasm->line[i]) || 
			   ((hasm->line[i] == '-' || hasm->line[i] == '+') &&
			     isdigit(hasm->line[i+1])
			   )
			  ) {
				hasm_error(hasm, "Label identifiers should not start with a digit or '-', '+' characters");
				ret = hasm_invalid_label;
				goto CLEAN;
			}
			
			size_t label_len;
			for(label_len = 0;
			hasm->line[i + label_len] &&
			hasm->line[i + label_len] != ')' &&
			!(hasm->line[i + label_len] == '/' && hasm->line[i + label_len + 1] == '/');
			label_len++)
			;
			if(hasm->line[i + label_len] != ')') {
				if(hasm->strict) {
					hasm_error(hasm, "Mismatched label parentheses");
					ret = hasm_syntax_error;
					goto CLEAN;
				}
				else {
					hasm_warn(hasm, "Mismatched label parentheses. proceeding anyway");
				}
			}
			char *label = n2t_strdup_n(&hasm->line[i], label_len);
			if(!label) {
				hasm_func_error(hasm, "out of memory");
				ret = hasm_oom;
				goto CLEAN;
			}
			
			const struct symbol *predef_sym = get_predefined_symbol(label, label_len);
			if(predef_sym) {
				n2t_free(label);
				if(hasm->strict) {
					hasm_error_f(hasm, "trying to redefine a predefined symbol '%s' as a label",
					predef_sym->name);
					ret = hasm_redefined_label;
					goto CLEAN;
				}
				else {
					hasm_warn_f(hasm, "trying to redefine a predefined symbol '%s' as a label. ignoring user definition and proceeding",
					predef_sym->name);
				}
				continue;
			}
			
			struct label *l;
			HASH_FIND_STR(labels, label, l);
			if(l) {
				if(hasm->strict) {
					hasm_error_f(hasm, "Label '%s' was defined multiple times", label);
					n2t_free(label);
					ret = hasm_redefined_label;
					goto CLEAN;
				}
				else {
					hasm_warn_f(hasm, "Label '%s' was defined multiple times. ignoring subsequent definitions", label);
					n2t_free(label);
				}
				continue;
			}
			l = n2t_malloc(sizeof(struct label));
			if(!l) {
				hasm_func_error(hasm, "out of memory");
				ret = hasm_oom;
				goto CLEAN;
			}
			l->name = label;
			l->address = hasm->inst_count;
			
			#undef uthash_nonfatal_oom
			#define uthash_nonfatal_oom(obj) do { \
				n2t_free(label); \
				n2t_free(l); \
				hasm_func_error(hasm, "out of memory"); \
				ret = hasm_oom; \
				goto CLEAN; \
			} while(0)
			
			HASH_ADD_KEYPTR( hh, labels, label, label_len, l);
		}
		else {
			/* C-instruction, possibly */
			union instruction inst;
			memset(&inst, 0, sizeof(inst));
			inst.op1 = 1;
			inst.op2 = 1;
			inst.op3 = 1;
			
			dest(&inst, hasm->line, &i);
			int comp_ret = comp(&inst, hasm->line, &i);
			if(!comp_ret) {
				/* the comp part is required */
				hasm_error_f(hasm, "Invalid Syntax '%s'", hasm->line);
				ret = hasm_syntax_error;
				goto CLEAN;
			}
			jump(&inst, hasm->line, &i);
			if(hasm->line[i] && !isspace(hasm->line[i]) && !(hasm->line[i] == '/' && hasm->line[i+1] == '/')) {
				if(hasm->strict) {
					hasm_error(hasm, "garbage character(s) after C-instruction");
					ret = hasm_syntax_error;
					goto CLEAN;
				}
				else {
					hasm_warn(hasm, "garbage character(s) after C-instruction. ignoring");
				}
			}
			
			fprintbin(hasm->dest, inst.inst);
			putc('\n', hasm->dest);
			
			hasm->inst_count++;
		}
		if(hasm->inst_count > HACK_ADDRESS_MAX && !warned_about_big_src) {
			if(hasm->strict) {
				hasm_error(hasm, "source is too big to fit in the Hack platform ROM");
				ret = hasm_src_too_big;
				goto CLEAN;
			}
			else {
				warned_about_big_src = true;
				hasm_warn(hasm, "source is too big to fit in the Hack platform ROM. proceeding at the risk of producing erroneous output");
			}
		}
		if(hasm->inst_count >= max_inst_count) break;
	}
	if(ferror(hasm->src)) {
		hasm_func_error(hasm, "error while processing source file");
		ret = hasm_io_error;
		goto CLEAN;
	}
	if(errno == ENOMEM) {
		hasm_func_error(hasm, "out of memory");
		ret = hasm_oom;
		goto CLEAN;
	}
	if(errno == ERANGE) {
		hasm_func_error(hasm, "encountered a very long line");
		ret = hasm_syntax_error;
		goto CLEAN;
	}
	
	uint16_t var_addr = 16;
	struct user_symbol *sym, *tmp;
    HASH_ITER(hh, user_symbols, sym, tmp) {
		struct label *l;
		HASH_FIND_STR(labels, sym->name, l);
		uint16_t addr;
		if(l) {
			addr = l->address;
		}
		else {
			/* if not a defined label, treat as a variable */
			addr = var_addr;
			var_addr++;
		}
		for(size_t i = 0; i < sym->pos_vec.length; i++) {
			fsetpos(hasm->dest, &sym->pos_vec.data[i]);
			fprintbin(hasm->dest, addr);
		}
    }
	
CLEAN:
    HASH_ITER(hh, user_symbols, us_item, us_tmp) {
      HASH_DEL(user_symbols, us_item);
	  n2t_free(us_item->name);
	  VEC_FREE(us_item->pos_vec);
      n2t_free(us_item);
    }
	
    HASH_ITER(hh, labels, l_item, l_tmp) {
      HASH_DEL(labels, l_item);
	  n2t_free(l_item->name);
      n2t_free(l_item);
    }
	
	return ret;
}

hasm_state hasm_assemble(struct hasm *hasm) {
	return hasm_assemble_n(hasm, -1);
}

hasm_state hasm_assemble_line(struct hasm *hasm) {
	return hasm_assemble_n(hasm, 1);
}
