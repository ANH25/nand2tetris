/* Jack Language Compiler */

/* TODO
- refactor duplicate code
- better error reporting
- check memory allocations
- test method calling from inside of functions using the syntax 'methodName()'
*/

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "n2t-common.h"
#include "jack-compiler.h"

#define jc_oom_report(jc) jc_error(jc, "out of memory")


int jc_init(struct j_compiler *jc) {
	
	j_tokenizer_init(&jc->tokenizer);
	jc->subroutine_symbol_table = NULL;
	jc->class_symbol_table = NULL;
	jc->cur_class.name = NULL;
	jc->cur_class.buf_size = 0;
	jc->cur_class.len = 0;
	jc->if_label_n = 0;
	jc->while_label_n = 0;
	return 1;
}

void jc_free(struct j_compiler *jc) {
	
	struct j_symbol_t *sym, *tmp;
	HASH_ITER(hh, jc->subroutine_symbol_table, sym, tmp) {
		HASH_DEL(jc->subroutine_symbol_table, sym);
		n2t_free(sym->name);
		n2t_free(sym->type);
		n2t_free(sym);
	}
	HASH_ITER(hh, jc->class_symbol_table, sym, tmp) {
		HASH_DEL(jc->class_symbol_table, sym);
		n2t_free(sym->name);
		n2t_free(sym->type);
		n2t_free(sym);
	}
	
	n2t_free(jc->cur_class.name);
	j_tokenizer_free(&jc->tokenizer);
	
}


static struct j_symbol_t* jc_write_push_pop_var(struct j_compiler *jc,
                                                const char *op,
												const char *var,
												bool warn) {
	static const char *vm_segments[] = {
		[j_var_unknown] = "UNKNOWN_VM_SEGMENT",
		[j_var_local] = "local",
		[j_var_argument] = "argument",
		[j_var_field] = "this",
		[j_var_static] = "static"
	};
	
	struct j_symbol_t *symbol;
	HASH_FIND_STR(jc->subroutine_symbol_table, var, symbol);
	if(symbol) {
		jc_write_vm_f(jc, "%s %s %" PRIu16 "\n", op, vm_segments[symbol->kind], symbol->index);
		return symbol;
	}
	HASH_FIND_STR(jc->class_symbol_table, var, symbol);
	if(symbol) {
		jc_write_vm_f(jc, "%s %s %"  PRIu16 "\n", op, vm_segments[symbol->kind], symbol->index);
		return symbol;
	}
	//fixme: since j_tokenizer_error_f report line number from the tokenizer
	// this can cause discrepancy between the reported number and the actual line number
	// 'var' was mentioned in if 'var' is not the current token
	if(warn)
		jc_error_f(jc, "undefined variable '%s'", var);
	return NULL;
}

int jc_compile_term(struct j_compiler *jc) {
	
	int ret = 1;
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens) {
		jc_error(jc,
		"expected a term");
		return 0;
	}
	
	fpos_t dest_pos;
	fgetpos(jc->dest, &dest_pos);
	jc_write_xml(jc, "<term>\n");
	
	while(jc->tokenizer.has_more_tokens) {
		
		//printf("jc->tokenizer.token.name = %s\n", jc->tokenizer.token.name);
		//printf("jc->tokenizer.token.type = %d\n", jc->tokenizer.token.type);	
	
		if(jc->tokenizer.token.name[0] == '-' || jc->tokenizer.token.name[0] == '~') {
			// unary operator
			char op = jc->tokenizer.token.name[0];
			jc_write_xml_el(jc);
			ret = jc_compile_term(jc);
			if(op == '-')
				jc_write_vm(jc, "neg\n");
			else
				jc_write_vm(jc, "not\n");
			break;
		}
		else if(jc->tokenizer.token.name[0] == '(') {
			// parenthesized expression
			jc_write_xml_el(jc);
			
			ret = jc_compile_expression(jc);
			if(!ret) {
				jc_error(jc,
				"expected an expression after '('");
				return 0;
			}
			
			j_tokenizer_advance(&jc->tokenizer);
			if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != ')') {
				jc_error(jc,
				"expected ')' to match '(' after expression");
				return 0;
			}
			jc_write_xml_el(jc);
			break;
		}
		else if(jc->tokenizer.token.type == j_keyword) {
			//printf("jc->tokenizer.token.keyword_type = %d\n", jc->tokenizer.token.keyword_type);
			if(jc->tokenizer.token.keyword_type >= j_keyword_true &&
			   jc->tokenizer.token.keyword_type <= j_keyword_this) {
					// keyword constant
					jc_write_xml_el(jc);
					
					if(jc->tokenizer.token.keyword_type == j_keyword_this) {
						jc_write_vm(jc, "push pointer 0\n");
					}
					else if(jc->tokenizer.token.keyword_type == j_keyword_true) {
						jc_write_vm(jc, "push constant 0\nnot\n");
					}
					else if(jc->tokenizer.token.keyword_type == j_keyword_false) {
						jc_write_vm(jc, "push constant 0\n");
					}
					else if(jc->tokenizer.token.keyword_type == j_keyword_null) {
						//fixme: what is the numerical representation of null? is it 0?
						jc_write_vm(jc, "push constant 0\n");
					}
					
					break;
			}
			else {
				jc_error_f(jc,
				"keyword '%s' is not meaningful in this context",
				jc->tokenizer.token.name);
				//ret = 0;
				//break;
				fsetpos(jc->dest, &dest_pos);
				return 0;
			}
		}
		else if(jc->tokenizer.token.type == j_str) {
			jc_write_xml_el(jc);
			
			jc_write_vm_f(jc, "push constant %zu\ncall String.new 1\n",
			jc->tokenizer.token.name_len);
			for(size_t i = 0; i < jc->tokenizer.token.name_len; i++) {
				jc_write_vm_f(jc, "push constant %d\ncall String.appendChar 2\n",
				jc->tokenizer.token.name[i]);
			}
			
			break;
		}
		else if(jc->tokenizer.token.type == j_int) {
			//fixme: check integer range?
			jc_write_xml_el(jc);
			jc_write_vm_f(jc, "push constant %s\n", jc->tokenizer.token.name);
			break;
		}
		else if(jc->tokenizer.token.type == j_identifier) {
			
			fpos_t dest_pos;
			fgetpos(jc->dest, &dest_pos);
			jc_write_xml_el(jc);
			
			fpos_t ident_fpos = jc->tokenizer.token.fpos;
			
			j_tokenizer_advance(&jc->tokenizer);
			if(jc->tokenizer.has_more_tokens) {
				if(jc->tokenizer.token.name[0] == '[') {
					// array indexing
					// index is an expression
					jc_write_xml_el(jc);
					
					fsetpos(jc->tokenizer.src, &ident_fpos);
					j_tokenizer_advance(&jc->tokenizer);
					jc_write_push_pop_var(jc, "push", jc->tokenizer.token.name, true);
					j_tokenizer_advance(&jc->tokenizer);
					
					ret = jc_compile_expression(jc);
					if(!ret) {
						jc_error(jc,
						"expected an expression inside []");
						ret = 0;
						break;
					}
					j_tokenizer_advance(&jc->tokenizer);
					if(!jc->tokenizer.has_more_tokens ||
					   jc->tokenizer.token.name[0] != ']') {
						jc_error(jc,
						"expected ']' to match '['");
						ret = 0;
						break;
					}
					jc_write_xml_el(jc);
					
					jc_write_vm(jc, "add\npop pointer 1\npush that 0\n");
					
					break;
				}
				else if(jc->tokenizer.token.name[0] == '(') {
					// subroutine call. handle its arguments in a separate function
					//jc_write_xml_el(jc);
					j_tokenizer_retreat(&jc->tokenizer);
					/*ret = jc_compile_expression_list(jc);
					if(!ret) {
						jc_error(jc,
						"expected an argument list after subroutine name");
						ret = 0;
						break;
					}*/
					/*j_tokenizer_advance(&jc->tokenizer);
					if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != ')') {
						jc_error(jc,
						"expected ')' to match '('");
						ret = 0;
						break;
					}
					jc_write_xml_el(jc);
					*/
					ret = jc_compile_subroutine_call(jc);
					if(!ret) {
						//jc_error(jc,
						//"expected an argument list after subroutine name");
						ret = 0;
					}
					break;
				}
				else if(jc->tokenizer.token.name[0] == '.') {
					// previous token was a class/variable name.
					// next we should expect a subroutine name
					/*jc_write_xml_el(jc);
					j_tokenizer_advance(&jc->tokenizer);
					if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.type != j_identifier) {
						
						jc_error(jc,
						"expected a subroutine name after '.'");
						ret = 0;
						break;
					}
					jc_write_xml_el(jc);
					
					ret = jc_compile_expression_list(jc);
					if(!ret) {
						jc_error(jc,
						"expected an argument list after subroutine name");
						ret = 0;
						break;
					}
					*/
					fsetpos(jc->dest, &dest_pos);
					
					fsetpos(jc->tokenizer.src, &ident_fpos);
					ret = jc_compile_subroutine_call(jc);
					if(!ret) {
						//jc_error(jc,
						//"expected an argument list after subroutine name");
						ret = 0;
					}
					break;
				}
				else {
					//previous token was a variable name.
					//j_tokenizer_retreat(&jc->tokenizer);
					fsetpos(jc->tokenizer.src, &ident_fpos);
					j_tokenizer_advance(&jc->tokenizer);
					jc_write_push_pop_var(jc, "push", jc->tokenizer.token.name, true);
					//j_tokenizer_advance(&jc->tokenizer);
					break;
				}
			}
			else {
				// error?
				jc_error(jc,
				"expected a term");
				ret = 0;
			}
			//return 1;
		}
		else {
			//fixme: this will result in empty <term> tags sometimes!
			j_tokenizer_retreat(&jc->tokenizer);
			//break;
			jc_error(jc, "expected a term");
			
			fsetpos(jc->dest, &dest_pos);
			return 0;
		}
		j_tokenizer_advance(&jc->tokenizer);
	}
	jc_write_xml(jc, "</term>\n");
	
	return ret;
}


static const char* get_op_vm_code(char op) {
	
	static const char* ops_vm_code[] = {
"lt",
"eq",
NULL,
"add",
"or",
"sub",
"call Math.multiply 2",
"call Math.divide 2",
"and",
"gt",
};
	/* poor man's "hashing" */
	unsigned char i = op % 10;
	if(i == 2) {
		unsigned char tens = op / 10;
		if(tens / 6) {
			return ops_vm_code[9];
		}
		else {
			return ops_vm_code[6];
		}
	}
	return ops_vm_code[i];
}

int jc_compile_expression(struct j_compiler *jc) {
	
	int ret = 1;
	fpos_t dest_pos;
	fgetpos(jc->dest, &dest_pos);
	jc_write_xml(jc, "<expression>\n");
	
	ret = jc_compile_term(jc);
	if(!ret) {
		jc_error(jc, "expected an expression");
		fsetpos(jc->dest, &dest_pos);
		return 0;
	}
	/*if(!ret) {
		goto JC_COM_EXP_END;
	}*/
	//printf("jc_compile_expression: term_1 = %s\n", jc->tokenizer.token.name);
	
	j_tokenizer_advance(&jc->tokenizer);
	while(jc->tokenizer.has_more_tokens) {
		
		if(jc->tokenizer.token.type == j_symbol && 
		  jc->tokenizer.token.symbol_type == j_symbol_op) {
			  
			jc_write_xml_el(jc);
			char op = jc->tokenizer.token.name[0];
			ret = jc_compile_term(jc);
			if(!ret) {
				jc_error_f(jc,
				"expected a term after operator '%c'", op);
				ret = 0;
				break;
			}
			//printf("op = %c %d\n", op, op);
			//printf("jc_compile_expression: term_2 = %s\n", jc->tokenizer.token.name);
			
			jc_write_vm_f(jc, "%s\n", get_op_vm_code(op));
		}
		else {
			//no more (op term) tokens
			j_tokenizer_retreat(&jc->tokenizer);
			break;
		}
		
		j_tokenizer_advance(&jc->tokenizer);
	}
//JC_COM_EXP_END:
	jc_write_xml(jc, "</expression>\n");
	return ret;
}

uint16_t jc_compile_expression_list(struct j_compiler *jc) {
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != '(') {
		jc_error(jc, "expected an argument list");
		return -1;
	}
	jc_write_xml_el(jc);
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens) {
		jc_error(jc, "expected an argument list");
		return -1;
	}
	
	int ret;
	uint16_t exp_count = 0;
	jc_write_xml(jc, "<expressionList>\n");
	/*
	if(jc->tokenizer.token.name[0] == ')') {
		// to avoid generating empty tags in jc_compile_expression, jc_compile_expression_list
		j_tokenizer_retreat(&jc->tokenizer);
		goto JC_COM_EXPLIST_END;
	}
	*/
	
	while(jc->tokenizer.has_more_tokens) {
		
		if(jc->tokenizer.token.name[0] == ')') {
			j_tokenizer_retreat(&jc->tokenizer);
			break;
		}
		//printf("jc->tokenizer.token.name = %s\n", jc->tokenizer.token.name);
		j_tokenizer_retreat(&jc->tokenizer);
		ret = jc_compile_expression(jc);
		if(!ret) {
			jc_error(jc, "expected an expression inside argument list");
			break;
		}
		exp_count++;
		
		j_tokenizer_advance(&jc->tokenizer);
		if(!jc->tokenizer.has_more_tokens) break;
		//printf("jc->tokenizer.token.name = %s\n", jc->tokenizer.token.name);
		
		if(jc->tokenizer.token.name[0] == ')') {
			j_tokenizer_retreat(&jc->tokenizer);
			break;
		}
		else if(jc->tokenizer.token.name[0] != ',') {
			jc_error(jc, "expected a comma to delimit arguments");
			exp_count = -1;
			break;
		}
		jc_write_xml_el(jc);
		j_tokenizer_advance(&jc->tokenizer);
	}

//JC_COM_EXPLIST_END:
	jc_write_xml(jc, "</expressionList>\n");
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != ')') {
		jc_error(jc, "expected ')' to close argument list");
		//todo: reset fpos?
		return -1;
	}
	jc_write_xml_el(jc);
	return exp_count;
}

int jc_compile_return(struct j_compiler *jc) {
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens ||
	   !(jc->tokenizer.token.type == j_keyword &&
	     jc->tokenizer.token.keyword_type == j_keyword_return)) {
		
		jc_error(jc, "expected a return statement");
		return 0;
	}
	
	int ret = 1;
	fpos_t dest_pos;
	fgetpos(jc->dest, &dest_pos);
	jc_write_xml(jc, "<returnStatement>\n");
	jc_write_xml_el(jc);
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens) {
		jc_error(jc, "expected a statement or ';' character after 'return' keyword");
		fsetpos(jc->dest, &dest_pos);
		return 0;
	}
	if(jc->tokenizer.token.name[0] == ';') {
		jc_write_xml_el(jc);
		/* return 0 for void subroutines */
		jc_write_vm(jc, "push constant 0\n");
	}
	else {
		j_tokenizer_retreat(&jc->tokenizer);
		ret = jc_compile_expression(jc);
		if(!ret) {
			jc_error(jc,
			"expected a statement or ';' character after 'return' keyword");
			fsetpos(jc->dest, &dest_pos);
			return 0;
		}
		j_tokenizer_advance(&jc->tokenizer);
		if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != ';') {
			jc_error(jc,
			"expected ';' character after return statement");
			fsetpos(jc->dest, &dest_pos);
			return 0;
		}
		jc_write_xml_el(jc);
	}
	jc_write_xml(jc, "</returnStatement>\n");
	jc_write_vm(jc, "return\n");
	return ret;
}

int jc_compile_subroutine_call(struct j_compiler *jc) {
	
	int ret = 1;
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.type != j_identifier) {
		jc_error(jc,
		"expected an identifier for subroutine call");
		return 0;
	}
	jc_write_xml_el(jc);
	char *ident_1 = n2t_strdup_n(jc->tokenizer.token.name, jc->tokenizer.token.name_len);
	if(!ident_1) {
		jc_oom_report(jc);
		return 0;
	}
	char *ident_2;
	uint16_t additional_arg = 0;
	
	j_tokenizer_advance(&jc->tokenizer);
	if(jc->tokenizer.has_more_tokens && jc->tokenizer.token.name[0] == '.') {
		jc_write_xml_el(jc);
		j_tokenizer_advance(&jc->tokenizer);
		if(!jc->tokenizer.has_more_tokens && jc->tokenizer.token.type != j_identifier) {
			jc_error(jc,
			"expected a subroutine identifier after '.'");
			ret = 0;
			goto CLEAN_1;
		}
		
		ident_2 = n2t_strdup_n(jc->tokenizer.token.name, jc->tokenizer.token.name_len);
		if(!ident_2) {
			n2t_free(ident_1);
			jc_oom_report(jc);
			return 0;
		}
		jc_write_xml_el(jc);
		//printf("ident_2 = '%s'\n", ident_2);
		// if an object instance, push it as an argument to its called method
		struct j_symbol_t *symbol = jc_write_push_pop_var(jc, "push", ident_1, false);
		if(symbol) {
			// replace variable name with class name
			n2t_free(ident_1);
			ident_1 = n2t_strdup_n(symbol->type, strlen(symbol->type));
			if(!ident_1) {
				n2t_free(ident_2);
				jc_oom_report(jc);
				return 0;
			}
			additional_arg = 1;
		}
	}
	else {
		j_tokenizer_retreat(&jc->tokenizer);
		// if subroutine call is in the form 'SubroutineName()' assume it's
		// a subroutine from the current class
		ident_2 = ident_1;
		ident_1 = n2t_strdup_n(jc->cur_class.name, jc->cur_class.len);
		if(!ident_1) {
			n2t_free(ident_2);
			jc_oom_report(jc);
			return 0;
		}
		// push class object reference
		// fixme: this will be written for functions also, not just methods
		jc_write_vm(jc, "push pointer 0\n");
		additional_arg = 1;
	}
	uint16_t exp_count = additional_arg + jc_compile_expression_list(jc);
	if(exp_count == (uint16_t)-1) {
		jc_error(jc, "expected an argument list after subroutine name");
		ret = 0;
		goto CLEAN_2;
	}
	jc_write_vm_f(jc, "call %s.%s %" PRIu16 "\n", ident_1, ident_2, exp_count);
	
CLEAN_2:
	n2t_free(ident_2);
CLEAN_1:
	n2t_free(ident_1);
	return ret;
}


int jc_compile_do(struct j_compiler *jc) {
		
	j_tokenizer_advance(&jc->tokenizer);
	//printf("jc->tokenizer.token.name = %s\n", jc->tokenizer.token.name);
	if(!jc->tokenizer.has_more_tokens) {
		jc_error(jc,
		"expected a 'do' statement");
		return 0;
	}
	int ret = 1;
	jc_write_xml(jc, "<doStatement>\n");
	jc_write_xml_el(jc);
	
	ret = jc_compile_subroutine_call(jc);
	if(!ret) {
		jc_error(jc,
		"expected a subroutine call after 'do' keyword");
		return 0;
	}
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != ';') {
		jc_error(jc,
		"expected ';' to end do statement");
		return 0;
	}
	jc_write_xml_el(jc);
	/* discard return value */
	jc_write_vm(jc, "pop temp 0\n");
	jc_write_xml(jc, "</doStatement>\n");
	return ret;
}



int jc_compile_let(struct j_compiler *jc) {
	
	int ret = 1;
	jc_write_xml(jc, "<letStatement>\n");
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens ||
	   !(jc->tokenizer.token.type == j_keyword &&
	     jc->tokenizer.token.keyword_type == j_keyword_let)) {
		
		jc_error(jc, "expected 'let' keyword");
		return 0;
	}
	jc_write_xml_el(jc);
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens ||
	   jc->tokenizer.token.type != j_identifier) {
		
		jc_error(jc, "expected identifier after 'let' keyword");
		return 0;
	}
	jc_write_xml_el(jc);
	char *dest_ident = n2t_strdup_n(jc->tokenizer.token.name, jc->tokenizer.token.name_len);
	if(!dest_ident) {
		jc_oom_report(jc);
		return 0;
	}
	fpos_t dest_fpos = jc->tokenizer.token.fpos;
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens) {
		jc_error(jc,
		"expected '=' or array indexing notation '[]' after identifier in a let statement");
		ret = 0;
		goto CLEAN;
	}
	bool is_array = false;
	if(jc->tokenizer.token.name[0] == '[') {
		jc_write_xml_el(jc);
		
		fsetpos(jc->tokenizer.src, &dest_fpos);
		j_tokenizer_advance(&jc->tokenizer);
		jc_write_push_pop_var(jc, "push", jc->tokenizer.token.name, true);
		j_tokenizer_advance(&jc->tokenizer);
		
		ret = jc_compile_expression(jc);
		if(!ret) {
			jc_error(jc, "expected expression after '[' in a let statement");
			ret = 0;
			goto CLEAN;
		}
		
		j_tokenizer_advance(&jc->tokenizer);
		if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != ']') {
			jc_error(jc,
			"expected ']' to match '[' in a let statement");
			ret = 0;
			goto CLEAN;
		}
		jc_write_xml_el(jc);
		
		jc_write_vm(jc, "add\n");
		is_array = true;
		j_tokenizer_advance(&jc->tokenizer);
	}
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != '=') {
		jc_error(jc,
		"expected '=' after identifier in a let statement");
		ret = 0;
		goto CLEAN;
	}
	jc_write_xml_el(jc);
	
	ret = jc_compile_expression(jc);
	if(!ret) {
		jc_error(jc,
		"expected expression after '=' in a let statement");
		ret = 0;
		goto CLEAN;
	}
	
	if(is_array) {
		jc_write_vm(jc, "pop temp 0\n\
pop pointer 1\n\
push temp 0\n\
pop that 0\n");
	}
	else {
		jc_write_push_pop_var(jc, "pop", dest_ident, true);
	}
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != ';') {
		jc_error(jc,
		"expected ';' after let statement");
		ret = 0;
		goto CLEAN;
	}
	jc_write_xml_el(jc);
	
CLEAN:
	n2t_free(dest_ident);
	jc_write_xml(jc, "</letStatement>\n");
	return ret;
}

int jc_compile_while(struct j_compiler *jc) {
	
	int ret = 1;
	jc_write_xml(jc, "<whileStatement>\n");
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens ||
	   !(jc->tokenizer.token.type == j_keyword &&
	     jc->tokenizer.token.keyword_type == j_keyword_while)) {
		
		jc_error(jc,
		"expected 'while' keyword");
		return 0;
	}
	jc_write_xml_el(jc);
	uint16_t while_label_n = jc->while_label_n;
	jc->while_label_n++;
	jc_write_vm_f(jc, "label WHILE_EXP%" PRIu16 "\n", while_label_n);
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != '(') {
		jc_error(jc,
		"expected '(' after 'while' keyword");
		return 0;
	}
	jc_write_xml_el(jc);
	
	ret = jc_compile_expression(jc);
	if(!ret) {
		jc_error(jc,
		"expected an expression after '(' in a while statement");
		return 0;
	}
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != ')') {
		jc_error(jc,
		"expected ')' to match '(' after expression in a while statement");
		return 0;
	}
	jc_write_xml_el(jc);
	
	jc_write_vm_f(jc, "not\nif-goto WHILE_END%" PRIu16 "\n", while_label_n);
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != '{') {
		jc_error(jc,
		"expected '{' after while(expression)");
		return 0;
	}
	jc_write_xml_el(jc);
	
	ret = jc_compile_statements(jc);
	if(!ret) {
		return 0;
	}
	jc_write_vm_f(jc, "goto WHILE_EXP%" PRIu16 "\n", while_label_n);
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != '}') {
		jc_error(jc,
		"expected '}' to match '{' in a while statement");
		return 0;
	}
	jc_write_xml_el(jc);
	
	jc_write_vm_f(jc, "label WHILE_END%" PRIu16 "\n", while_label_n);
	
	jc_write_xml(jc, "</whileStatement>\n");
	return ret;
}

int jc_compile_if_else(struct j_compiler *jc) {
	
	int ret = 1;
	jc_write_xml(jc, "<ifStatement>\n");
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens ||
	   !(jc->tokenizer.token.type == j_keyword &&
	     jc->tokenizer.token.keyword_type == j_keyword_if)) {
		
		jc_error(jc,
		"expected 'if' keyword");
		return 0;
	}
	jc_write_xml_el(jc);
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != '(') {
		jc_error(jc,
		"expected '(' after 'if' keyword");
		return 0;
	}
	jc_write_xml_el(jc);
	
	ret = jc_compile_expression(jc);
	if(!ret) {
		jc_error(jc,
		"expected an expression after '(' in a if statement");
		return 0;
	}
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != ')') {
		jc_error(jc,
		"expected ')' to match '(' after expression in a if statement");
		return 0;
	}
	jc_write_xml_el(jc);
	uint16_t if_label_n = jc->if_label_n;
	jc->if_label_n++;
	jc_write_vm_f(jc, "if-goto IF_TRUE%" PRIu16
	"\ngoto IF_FALSE%" PRIu16 "\nlabel IF_TRUE%" PRIu16 "\n",
	if_label_n, if_label_n, if_label_n);
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != '{') {
		jc_error(jc,
		"expected '{' after if(expression)");
		return 0;
	}
	jc_write_xml_el(jc);
	
	ret = jc_compile_statements(jc);
	if(!ret) {
		return 0;
	}
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != '}') {
		jc_error(jc,
		"expected '}' to match '{' in a if statement");
		return 0;
	}
	jc_write_xml_el(jc);
	jc_write_vm_f(jc, "goto IF_END%" PRIu16 "\nlabel IF_FALSE%" PRIu16 "\n",
	if_label_n, if_label_n);
	
	j_tokenizer_advance(&jc->tokenizer);
	if(jc->tokenizer.has_more_tokens &&
	   jc->tokenizer.token.type == j_keyword &&
	   jc->tokenizer.token.keyword_type == j_keyword_else) {
		
		jc_write_xml_el(jc);
		j_tokenizer_advance(&jc->tokenizer);
		if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != '{') {
			jc_error(jc,
			"expected '{' after else(expression)");
			return 0;
		}
		jc_write_xml_el(jc);
		ret = jc_compile_statements(jc);
		if(!ret) {
			return 0;
		}
		j_tokenizer_advance(&jc->tokenizer);
		if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != '}') {
			jc_error(jc,
			"expected '}' to match '{' in a else statement");
			return 0;
		}
		jc_write_xml_el(jc);
		
	}
	else {
		j_tokenizer_retreat(&jc->tokenizer);
	}
	
	jc_write_vm_f(jc, "label IF_END%" PRIu16 "\n", if_label_n);
	jc_write_xml(jc, "</ifStatement>\n");
	return ret;
}

int jc_compile_statements(struct j_compiler *jc) {
	
	int ret = 1;
	jc_write_xml(jc, "<statements>\n");
	
static const struct {
	jc_func compile;
	j_keyword_type keyword_type;
	} statements[5] = {
		{jc_compile_return, j_keyword_return},
		{jc_compile_do, j_keyword_do},
		{jc_compile_if_else, j_keyword_if},
		{jc_compile_while, j_keyword_while},
		{jc_compile_let, j_keyword_let}
	};

	j_tokenizer_advance(&jc->tokenizer);
	while(jc->tokenizer.has_more_tokens) {
		/*
		if(jc->tokenizer.token.type == j_keyword &&
		   jc->tokenizer.token.keyword_type == j_keyword_let) {
			j_tokenizer_retreat(&jc->tokenizer);
			ret = jc_compile_let(jc);
			if(!ret) {
				return 0;
			}
		}*/
		if(jc->tokenizer.token.name[0] == '}') {
			j_tokenizer_retreat(&jc->tokenizer);
			break;
		}
		
		if(jc->tokenizer.token.type != j_keyword) {
			jc_error(jc,
			"expected a statement");
			return 0;
		}
		bool is_statement = false;
		for(unsigned char i = 0; i < 5; i++) {
			if(jc->tokenizer.token.keyword_type == statements[i].keyword_type) {
				j_tokenizer_retreat(&jc->tokenizer);
				ret = statements[i].compile(jc);
				if(!ret) return 0;
				is_statement = true;
				break;
			}
		}
		if(!is_statement) {
			jc_error(jc,
			"expected a statement");
			return 0;
		}
		
		j_tokenizer_advance(&jc->tokenizer);
	}
	
	jc_write_xml(jc, "</statements>\n");
	return ret;
}

/* A valid type is 'int', 'boolean', 'char', or a class name */
int jc_compile_type(struct j_compiler *jc) {
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens ||
	   !((jc->tokenizer.token.type == j_keyword &&
	    jc->tokenizer.token.keyword_type >= j_keyword_int &&
		jc->tokenizer.token.keyword_type <= j_keyword_char) ||
	    jc->tokenizer.token.type == j_identifier))
	   {
			return 0;
	   }
	jc_write_xml_el(jc);
	return 1;
}

int jc_compile_parameter_list(struct j_compiler *jc, j_keyword_type subroutine_type) {

	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != '(') {
		jc_error(jc,
		"expected '(' to start a parameter list");
		return 0;
	}
	jc_write_xml_el(jc);
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens) {
		jc_error(jc,
		"expected a parameter list after '('");
		return 0;
	}
	
	int ret = 1;
	jc_write_xml(jc, "<parameterList>\n");
	
	jc->var_kind_indices[j_var_argument] = (subroutine_type == j_keyword_method ? 1 : 0);
	jc->var_kind_indices[j_var_local] = 0;
	/* free previous subroutine symbol table */
	struct j_symbol_t *cur_sym, *tmp_sym;
	HASH_ITER(hh, jc->subroutine_symbol_table, cur_sym, tmp_sym) {
		HASH_DEL(jc->subroutine_symbol_table, cur_sym);
		n2t_free(cur_sym->name);
		n2t_free(cur_sym->type);
		n2t_free(cur_sym);
	}
	
	while(jc->tokenizer.has_more_tokens) {
		
		if(jc->tokenizer.token.name[0] == ')') {
			j_tokenizer_retreat(&jc->tokenizer);
			break;
		}
		
		struct j_symbol_t *symbol = n2t_malloc(sizeof(struct j_symbol_t));
		if(!symbol) {
			jc_oom_report(jc);
			return 0;
		}
		symbol->kind = j_var_argument;
		
		j_tokenizer_retreat(&jc->tokenizer);
		ret = jc_compile_type(jc);
		if(!ret) {
			jc_error(jc,
				"expected a type name");
			return 0;
		}
		
		symbol->type = n2t_strdup_n(jc->tokenizer.token.name, jc->tokenizer.token.name_len);
		if(!symbol->type) {
			n2t_free(symbol);
			jc_oom_report(jc);
			return 0;
		}
		symbol->index = jc->var_kind_indices[j_var_argument];
		jc->var_kind_indices[j_var_argument]++;
		
		j_tokenizer_advance(&jc->tokenizer);
		if(!jc->tokenizer.has_more_tokens ||
		   jc->tokenizer.token.type != j_identifier) {
			
			jc_error(jc,
				"expected parameter name after type name");
			return 0;
		}
		jc_write_xml_el(jc);
		symbol->name = n2t_strdup_n(jc->tokenizer.token.name, jc->tokenizer.token.name_len);
		if(!symbol->name) {
			n2t_free(symbol->type);
			n2t_free(symbol);
			jc_oom_report(jc);
			return 0;
		}
		
		#undef uthash_nonfatal_oom
		#define uthash_nonfatal_oom(obj) do { \
			n2t_free(symbol->type); \
			n2t_free(symbol->name); \
			n2t_free(symbol); \
			jc_oom_report(jc); \
			return 0; \
		} while(0)
		//fixme: check if symbol is already defined before adding it to table
		HASH_ADD_STR(jc->subroutine_symbol_table, name, symbol);
		
		j_tokenizer_advance(&jc->tokenizer);
		if(!jc->tokenizer.has_more_tokens) break;
		
		if(jc->tokenizer.token.name[0] == ')') {
			j_tokenizer_retreat(&jc->tokenizer);
			break;
		}
		else if(jc->tokenizer.token.name[0] != ',') {
			jc_error(jc,
				"expected a comma to delimit parameters");
			return 0;
		}
		jc_write_xml_el(jc);
		j_tokenizer_advance(&jc->tokenizer);
	}

	jc_write_xml(jc, "</parameterList>\n");
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != ')') {
		jc_error(jc,
		"expected ')' to close parameter list");
		return 0;
	}
	jc_write_xml_el(jc);
	return 1;
}

uint16_t jc_compile_var_dec(struct j_compiler *jc) {
	
	int ret;
	uint16_t var_count = 0;
	jc_write_xml(jc, "<varDec>\n");
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens ||
	   !(jc->tokenizer.token.type == j_keyword &&
	    jc->tokenizer.token.keyword_type == j_keyword_var))
	{
		jc_error(jc,
		"expected 'var' to start variable declaration");
		return -1;
	}
	jc_write_xml_el(jc);
	
	ret = jc_compile_type(jc);
	if(!ret) {
		jc_error(jc,
		"expected variable type name");
		return -1;
	}
	
	size_t var_type_len = jc->tokenizer.token.name_len;
	char *var_type = n2t_malloc(var_type_len + 1);
	if(!var_type) {
		jc_oom_report(jc);
		return -1;
	}
	strcpy(var_type, jc->tokenizer.token.name);
	
	j_tokenizer_advance(&jc->tokenizer);
	while(jc->tokenizer.has_more_tokens) {
		
		if(jc->tokenizer.token.type != j_identifier) {
			jc_error(jc,
			"expected variable name");
			var_count = -1;
			goto CLEAN;
		}
		jc_write_xml_el(jc);
		
		struct j_symbol_t *symbol = n2t_malloc(sizeof(struct j_symbol_t));
		if(!symbol) {
			jc_oom_report(jc);
			var_count = -1;
			goto CLEAN;
		}
		symbol->name = n2t_strdup_n(jc->tokenizer.token.name, jc->tokenizer.token.name_len);
		if(!symbol->name) {
			n2t_free(symbol);
			var_count = -1;
			goto CLEAN;
		}
		symbol->type = n2t_strdup_n(var_type, var_type_len);
		if(!symbol->type) {
			n2t_free(symbol->name);
			n2t_free(symbol);
			var_count = -1;
			goto CLEAN;
		}
		symbol->kind = j_var_local;
		symbol->index = jc->var_kind_indices[j_var_local];
		jc->var_kind_indices[j_var_local]++;
		
		
		#undef uthash_nonfatal_oom
		#define uthash_nonfatal_oom(obj) do { \
			n2t_free(symbol->type); \
			n2t_free(symbol->name); \
			n2t_free(symbol); \
			jc_oom_report(jc); \
			var_count = -1; \
			goto CLEAN; \
		} while(0)
		
		HASH_ADD_STR(jc->subroutine_symbol_table, name, symbol);
		var_count++;
		
		j_tokenizer_advance(&jc->tokenizer);
		if(jc->tokenizer.token.name[0] == ';') break;
		if(jc->tokenizer.token.name[0] != ',') {
			jc_error(jc,
			"expected ',' or ';' after variable name");
			var_count = -1;
			goto CLEAN;
		}
		jc_write_xml_el(jc);
		
		j_tokenizer_advance(&jc->tokenizer);
	}
	
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != ';') {
		jc_error(jc,
		"expected ';' after variable declaration");
		var_count = -1;
		goto CLEAN;
	}
	jc_write_xml_el(jc);
CLEAN:	
	n2t_free(var_type);
	jc_write_xml(jc, "</varDec>\n");
	return var_count;
}

int jc_compile_subroutine_body(struct j_compiler *jc, j_keyword_type subroutine_type) {
	
	int ret = 1;
	jc_write_xml(jc, "<subroutineBody>\n");
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != '{') {
		jc_error(jc,
		"expected '{' before subroutine body");
		return 0;
	}
	jc_write_xml_el(jc);
	
	uint16_t var_count = 0;
	j_tokenizer_advance(&jc->tokenizer);
	while(jc->tokenizer.has_more_tokens) {
		
		if(jc->tokenizer.token.type == j_keyword &&
		   jc->tokenizer.token.keyword_type == j_keyword_var) {
			   j_tokenizer_retreat(&jc->tokenizer);
			   uint16_t tmp_count = jc_compile_var_dec(jc);
			   if(tmp_count == (uint16_t)-1) return 0;
			   var_count += tmp_count;
		}
		else {
			j_tokenizer_retreat(&jc->tokenizer);
			break;
		}
		j_tokenizer_advance(&jc->tokenizer);
	}
	jc_write_vm_f(jc, "%" PRIu16 "\n", var_count);
	
	if(subroutine_type == j_keyword_constructor) {
		jc_write_vm_f(jc, "\
push constant %" PRIu16 "\n\
call Memory.alloc 1\n\
pop pointer 0\n",
jc->var_kind_indices[j_var_field]);
	}
	else if(subroutine_type == j_keyword_method) {
		// set 'this' to point to the parent oject
		jc_write_vm(jc, "push argument 0\npop pointer 0\n");
		//jc->var_kind_indices[j_var_argument]++;
	}
	
	ret = jc_compile_statements(jc);
	if(!ret) return 0;
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != '}') {
		jc_error(jc,
		"expected '}' after subroutine body");
		return 0;
	}
	jc_write_xml_el(jc);
	
	jc_write_xml(jc, "</subroutineBody>\n");
	return 1;
}

int jc_compile_subroutine_dec(struct j_compiler *jc) {
	
	jc->if_label_n = 0;
	jc->while_label_n = 0;
	
	int ret = 1;
	jc_write_xml(jc, "<subroutineDec>\n");
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens ||
	   !(jc->tokenizer.token.type == j_keyword && 
	    (jc->tokenizer.token.keyword_type == j_keyword_constructor ||
		 jc->tokenizer.token.keyword_type == j_keyword_method ||
		 jc->tokenizer.token.keyword_type == j_keyword_function)
	   ))
	   {
		jc_error(jc,
		"expected subroutine declaration to start with 'constructor', 'method', or 'function' keywords");
		return 0;
	   }
	jc_write_xml_el(jc);
	j_keyword_type subroutine_type = jc->tokenizer.token.keyword_type;
	
	ret = jc_compile_type(jc);
	if(jc->tokenizer.token.type == j_keyword &&
	   jc->tokenizer.token.keyword_type == j_keyword_void) {
		   // 'void' return type
		   jc_write_xml_el(jc);
	}
	else if(!ret) {
		jc_error(jc,
		"expected return value type name after subroutine type");
		return 0;
	}
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens ||
	   jc->tokenizer.token.type != j_identifier)
	   {
		   jc_error(jc,
			"expected a valid subroutine name");
			return 0;
	   }
	jc_write_xml_el(jc);
	jc_write_vm_f(jc, "function %s.%s ", jc->cur_class.name, jc->tokenizer.token.name);
	
	ret = jc_compile_parameter_list(jc, subroutine_type);
	if(!ret) {
		jc_error(jc,
		"expected parameter list");
		return 0;
	}
	
	ret = jc_compile_subroutine_body(jc, subroutine_type);
	if(!ret) return 0;
	
	jc_write_xml(jc, "</subroutineDec>\n");
	return ret;
}

int jc_compile_class_var_dec(struct j_compiler *jc) {
	
	int ret = 1;
	jc_write_xml(jc, "<classVarDec>\n");
	
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens ||
	   !(jc->tokenizer.token.type == j_keyword &&
	     (jc->tokenizer.token.keyword_type == j_keyword_field ||
		 jc->tokenizer.token.keyword_type == j_keyword_static
		 )
		)
	  )
	{
		jc_error(jc,
		"expected 'field' or 'static' to start class variable declaration");
		return 0;
	}
	jc_write_xml_el(jc);
	
	//char symbol_kind[10];
	//strcpy(symbol_kind, jc->tokenizer.token.name);
	
	j_var_kind var_kind = 0;
	if(jc->tokenizer.token.keyword_type == j_keyword_field) {
		var_kind = j_var_field;
	}
	else if(jc->tokenizer.token.keyword_type == j_keyword_static) {
		var_kind = j_var_static;
	}
	
	ret = jc_compile_type(jc);
	if(!ret) {
		jc_error(jc,
		"expected variable type name");
		return 0;
	}
	
	size_t var_type_len = jc->tokenizer.token.name_len;
	char *var_type = n2t_malloc(var_type_len + 1);
	if(!var_type) {
		jc_oom_report(jc);
		return 0;
	}
	strcpy(var_type, jc->tokenizer.token.name);
	
	j_tokenizer_advance(&jc->tokenizer);
	while(jc->tokenizer.has_more_tokens) {
		
		if(jc->tokenizer.token.type != j_identifier) {
			jc_error(jc,
			"expected variable name");
			ret = 0;
			goto CLEAN;
		}
		jc_write_xml_el(jc);
		
		struct j_symbol_t *symbol = n2t_malloc(sizeof(struct j_symbol_t));
		if(!symbol) {
			jc_oom_report(jc);
			ret = 0;
			goto CLEAN;
		}
		symbol->kind = var_kind;
		symbol->type = n2t_strdup_n(var_type, var_type_len);
		if(!symbol->type) {
			n2t_free(symbol);
			jc_oom_report(jc);
			ret = 0;
			goto CLEAN;
		}
		symbol->name = n2t_strdup_n(jc->tokenizer.token.name, jc->tokenizer.token.name_len);
		if(!symbol->name) {
			n2t_free(symbol->type);
			n2t_free(symbol);
			jc_oom_report(jc);
			ret = 0;
			goto CLEAN;
		}
		symbol->index = jc->var_kind_indices[var_kind];
		jc->var_kind_indices[var_kind]++;
		
		#undef uthash_nonfatal_oom
		#define uthash_nonfatal_oom(obj) do { \
			n2t_free(symbol->type); \
			n2t_free(symbol->name); \
			n2t_free(symbol); \
			jc_oom_report(jc); \
			ret = 0; \
			goto CLEAN; \
		} while(0)
		//fixme: check if symbol is already defined before adding it to table
		HASH_ADD_STR(jc->class_symbol_table, name, symbol);
		
		j_tokenizer_advance(&jc->tokenizer);
		if(jc->tokenizer.token.name[0] == ';') break;
		if(jc->tokenizer.token.name[0] != ',') {
			jc_error(jc,
			"expected ',' or ';' after variable name");
			ret = 0;
			goto CLEAN;
		}
		jc_write_xml_el(jc);
		
		j_tokenizer_advance(&jc->tokenizer);
	}
	
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != ';') {
		jc_error(jc,
		"expected ';' after class variable declaration");
		ret = 0;
		goto CLEAN;
	}
	jc_write_xml_el(jc);
CLEAN:
	n2t_free(var_type);
	
	jc_write_xml(jc, "</classVarDec>\n");
	return ret;
}

int jc_compile_class(struct j_compiler *jc) {
	
	memset(jc->var_kind_indices, 0, sizeof(jc->var_kind_indices));
	struct j_symbol_t *sym, *tmp;
	HASH_ITER(hh, jc->class_symbol_table, sym, tmp) {
		HASH_DEL(jc->class_symbol_table, sym);
		n2t_free(sym->name);
		n2t_free(sym->type);
		n2t_free(sym);
	}
	
	int ret = 1;
	jc_write_xml(jc, "<class>\n");
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens ||
	   !(jc->tokenizer.token.type == j_keyword &&
	     jc->tokenizer.token.keyword_type == j_keyword_class
		)
	  )
	{
		jc_error(jc,
		"expected 'class' keyword");
		return 0;
	}
	jc_write_xml_el(jc);
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.type != j_identifier) {
		jc_error(jc,
		"expected class name");
		return 0;
	}
	jc_write_xml_el(jc);
	
	if(jc->cur_class.buf_size > jc->tokenizer.token.name_len) {
		strcpy(jc->cur_class.name, jc->tokenizer.token.name);
	}
	else {
		char *tmp = n2t_realloc(jc->cur_class.name, jc->tokenizer.token.name_len);
		if(!tmp) {
			jc_oom_report(jc);
			return 0;
		}
		jc->cur_class.name = tmp;
		strcpy(jc->cur_class.name, jc->tokenizer.token.name);
		jc->cur_class.buf_size = jc->tokenizer.token.name_buf_cap;
	}
	jc->cur_class.len = jc->tokenizer.token.name_len;
	
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != '{') {
		jc_error(jc,
		"expected '{' after class name");
		return 0;
	}
	jc_write_xml_el(jc);
	
	j_tokenizer_advance(&jc->tokenizer);
	while(jc->tokenizer.has_more_tokens) {
		
		if(jc->tokenizer.token.type == j_keyword &&
	       (jc->tokenizer.token.keyword_type == j_keyword_field ||
		    jc->tokenizer.token.keyword_type == j_keyword_static
		   )
		  ) 
		{
			j_tokenizer_retreat(&jc->tokenizer);
			ret = jc_compile_class_var_dec(jc);
			if(!ret) return 0;
		}
		else break;
		j_tokenizer_advance(&jc->tokenizer);
	}
	
	while(jc->tokenizer.has_more_tokens) {
		
		if(jc->tokenizer.token.type == j_keyword && 
	       jc->tokenizer.token.keyword_type >= j_keyword_method &&
		   jc->tokenizer.token.keyword_type <= j_keyword_constructor)
		{
			j_tokenizer_retreat(&jc->tokenizer);
			ret = jc_compile_subroutine_dec(jc);
			if(!ret) return 0;
		}
		else break;
		j_tokenizer_advance(&jc->tokenizer);
	}
	
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != '}') {
		jc_error(jc,
		"expected '}' after class body");
		return 0;
	}
	jc_write_xml_el(jc);
	
	jc_write_xml(jc, "</class>\n");
	return 1;
}
