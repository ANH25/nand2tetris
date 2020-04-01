#include <string.h>
#include <stdlib.h>

#include "n2t-common.h"
#include "jack-compiler.h"


/*jc_func jc_functions[] = {
	
};*/

int jc_compiler_init(struct j_compiler *jc) {
	
	j_tokenizer_init(&jc->tokenizer);
	return 1;
}

int jc_compile_term(struct j_compiler *jc) {
	
	int ret = 1;
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens) {
		j_tokenizer_error(&jc->tokenizer,
		"expected a term");
		return 0;
	}
	
	fpos_t dest_pos;
	fgetpos(jc->dest, &dest_pos);
	fputs("<term>\n", jc->dest);
	
	while(jc->tokenizer.has_more_tokens) {
		
		//printf("jc->tokenizer.token.name = %s\n", jc->tokenizer.token.name);
		//printf("jc->tokenizer.token.type = %d\n", jc->tokenizer.token.type);	
	
		if(jc->tokenizer.token.type == j_symbol &&
		  (jc->tokenizer.token.name[0] == '-' || jc->tokenizer.token.name[0] == '~')) {
			// unary operator
			jc_write_xml_tag(jc);
			ret = jc_compile_term(jc);
			break;
		}
		else if(jc->tokenizer.token.type == j_keyword) {
			//printf("jc->tokenizer.token.keyword_type = %d\n", jc->tokenizer.token.keyword_type);
			if(jc->tokenizer.token.keyword_type >= j_keyword_true &&
			   jc->tokenizer.token.keyword_type <= j_keyword_this) {
					// keyword constant
					jc_write_xml_tag(jc);
					break;
			}
			else {
				j_tokenizer_error_f(&jc->tokenizer,
				"keyword '%s' is not meaningful in this context",
				jc->tokenizer.token.name);
				//ret = 0;
				//break;
				fsetpos(jc->dest, &dest_pos);
				return 0;
			}
		}
		else if(jc->tokenizer.token.type == j_str) {
			jc_write_xml_tag(jc);
			break;
		}
		else if(jc->tokenizer.token.type == j_int) {
			jc_write_xml_tag(jc);
			break;
		}
		else if(jc->tokenizer.token.type == j_identifier) {
			jc_write_xml_tag(jc);
			// the tricky part :(
			struct j_token prev_token;
			memcpy(&prev_token, &jc->tokenizer.token, sizeof(prev_token));
			prev_token.name = malloc(jc->tokenizer.token.name_len + 1);
			abort_if_oom(prev_token.name);
			strcpy(prev_token.name, jc->tokenizer.token.name);
			prev_token.name_buf_cap = jc->tokenizer.token.name_len + 1;
			
			j_tokenizer_advance(&jc->tokenizer);
			if(jc->tokenizer.has_more_tokens) {
				if(jc->tokenizer.token.type == j_symbol) {
					if(jc->tokenizer.token.name[0] == '[') {
						// array indexing
						// index is an expression. write a function to handle expressions
						jc_write_xml_tag(jc);
						ret = jc_compile_expression(jc);
						if(!ret) {
							j_tokenizer_error(&jc->tokenizer,
							"expected an expression inside []");
							ret = 0;
							break;
						}
						j_tokenizer_advance(&jc->tokenizer);
						if(!jc->tokenizer.has_more_tokens ||
						   jc->tokenizer.token.name[0] != ']') {
							j_tokenizer_error(&jc->tokenizer,
							"expected ']' to match '['");
							ret = 0;
							break;
						}
						jc_write_xml_tag(jc);
						break;
					}
					else if(jc->tokenizer.token.name[0] == '(') {
						// subroutine call. handle its arguments in a separate function
						jc_write_xml_tag(jc);
						
						ret = jc_compile_expression_list(jc);
						if(!ret) {
							j_tokenizer_error(&jc->tokenizer,
							"expected an argument list after subroutine name");
							ret = 0;
							break;
						}
						j_tokenizer_advance(&jc->tokenizer);
						if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != ')') {
							j_tokenizer_error(&jc->tokenizer,
							"expected ')' to match '('");
							ret = 0;
							break;
						}
						jc_write_xml_tag(jc);
						break;
					}
					else if(jc->tokenizer.token.name[0] == '.') {
						// previous token was a class/variable name.
						// next we should expect a subroutine name
						jc_write_xml_tag(jc);
						
						j_tokenizer_advance(&jc->tokenizer);
						if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.type != j_identifier) {
							
							j_tokenizer_error(&jc->tokenizer,
							"expected a subroutine name after '.'");
							ret = 0;
							break;
						}
						jc_write_xml_tag(jc);
						
						j_tokenizer_advance(&jc->tokenizer);
						if(jc->tokenizer.token.name[0] != '(') {
							j_tokenizer_error(&jc->tokenizer,
							"expected an argument list after subroutine name");
							ret = 0;
							break;
						}
						jc_write_xml_tag(jc);
						
						ret = jc_compile_expression_list(jc);
						if(!ret) {
							j_tokenizer_error(&jc->tokenizer,
							"expected an argument list after subroutine name");
							ret = 0;
							break;
						}
						j_tokenizer_advance(&jc->tokenizer);
						if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != ')') {
							j_tokenizer_error(&jc->tokenizer,
							"expected ')' to match '('");
							ret = 0;
							break;
						}
						jc_write_xml_tag(jc);
						break;
					}
					else {
						// previous token was a variable name.
						//we should retreat one token back some way
						 j_tokenizer_retreat(&jc->tokenizer);
						 break;
					}
				}
				else {
					 j_tokenizer_retreat(&jc->tokenizer);
					break;
				}
			}
			else {
				// error?
				j_tokenizer_error(&jc->tokenizer,
				"expected a term");
				ret = 0;
			}
			//return 1;
		}
		else {
			//fixme: this will result in empty <term> tags sometimes!
			j_tokenizer_retreat(&jc->tokenizer);
			//break;
			j_tokenizer_error(&jc->tokenizer,
			"expected a term");
			
			fsetpos(jc->dest, &dest_pos);
			return 0;
		}
		j_tokenizer_advance(&jc->tokenizer);
	}
	fputs("</term>\n", jc->dest);
	
	return ret;
}

int jc_compile_expression(struct j_compiler *jc) {
	
	int ret = 1;
	fpos_t dest_pos;
	fgetpos(jc->dest, &dest_pos);
	fputs("<expression>\n", jc->dest);
	
	ret = jc_compile_term(jc);
	if(!ret) {
		j_tokenizer_error(&jc->tokenizer,
		"expected an expression");
		fsetpos(jc->dest, &dest_pos);
		return 0;
	}
	/*if(!ret) {
		goto JC_COM_EXP_END;
	}*/
	
	j_tokenizer_advance(&jc->tokenizer);
	while(jc->tokenizer.has_more_tokens) {
		
		if(jc->tokenizer.token.type == j_symbol && 
		  jc->tokenizer.token.symbol_type == j_symbol_op) {
			  
			jc_write_xml_tag(jc);
			char op = jc->tokenizer.token.name[0];
			ret = jc_compile_term(jc);
			if(!ret) {
				j_tokenizer_error_f(&jc->tokenizer,
				"expected a term after operator '%c'", op);
				ret = 0;
				break;
			}
		}
		else {
			//no more (op term) tokens
			//todo: 'retreat' the tokenizer and break
			j_tokenizer_retreat(&jc->tokenizer);
			break;
		}
		
		j_tokenizer_advance(&jc->tokenizer);
	}
//JC_COM_EXP_END:
	fputs("</expression>\n", jc->dest);
	return ret;
}

int jc_compile_expression_list(struct j_compiler *jc) {
	
	int ret = 1;
	fputs("<expressionList>\n", jc->dest);
	
	j_tokenizer_advance(&jc->tokenizer);
	if(jc->tokenizer.token.name[0] == ')') {
		// to avoid generating empty tags in jc_compile_expression, jc_compile_expression_list
		j_tokenizer_retreat(&jc->tokenizer);
		goto JC_COM_EXPLIST_END;
	}
	
	while(jc->tokenizer.has_more_tokens) {
		
		ret = jc_compile_expression(jc);
		if(!ret) {
			j_tokenizer_error(&jc->tokenizer,
				"expected an expression inside argument list");
			break;
		}
		
		j_tokenizer_advance(&jc->tokenizer);
		if(!jc->tokenizer.has_more_tokens) break;
		if(jc->tokenizer.token.name[0] == ')') {
			j_tokenizer_retreat(&jc->tokenizer);
			break;
		}
		else if(jc->tokenizer.token.name[0] != ',') {
			j_tokenizer_error(&jc->tokenizer,
				"expected a comma to delimit arguments");
			ret = 0;
			break;
		}
		jc_write_xml_tag(jc);
		
		//j_tokenizer_advance(&jc->tokenizer);
	}
JC_COM_EXPLIST_END:
	fputs("</expressionList>\n", jc->dest);
	return ret;
}

int jc_compile_return(struct j_compiler *jc) {
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens ||
	   !(jc->tokenizer.token.type == j_keyword &&
	     jc->tokenizer.token.keyword_type == j_keyword_return)) {
		
		j_tokenizer_error(&jc->tokenizer,
		"expected a return statement");
		return 0;
	}
	
	int ret = 1;
	fpos_t dest_pos;
	fgetpos(jc->dest, &dest_pos);
	fputs("<returnStatement>\n", jc->dest);
	jc_write_xml_tag(jc);
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens) {
		j_tokenizer_error(&jc->tokenizer,
		"expected a statement or ';' character after 'return' keyword");
		fsetpos(jc->dest, &dest_pos);
		return 0;
	}
	if(jc->tokenizer.token.name[0] == ';') {
		jc_write_xml_tag(jc);
	}
	else {
		j_tokenizer_retreat(&jc->tokenizer);
		ret = jc_compile_expression(jc);
		if(!ret) {
			j_tokenizer_error(&jc->tokenizer,
			"expected a statement or ';' character after 'return' keyword");
			fsetpos(jc->dest, &dest_pos);
			return 0;
		}
		j_tokenizer_advance(&jc->tokenizer);
		if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.name[0] != ';') {
			j_tokenizer_error(&jc->tokenizer,
			"expected ';' character after return statement");
			fsetpos(jc->dest, &dest_pos);
			return 0;
		}
		jc_write_xml_tag(jc);
	}
	fputs("</returnStatement>\n", jc->dest);
	return ret;
}

int jc_compile_do(struct j_compiler *jc) {
		
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens) {
		j_tokenizer_error(&jc->tokenizer,
		"expected a 'do' statement");
		return 0;
	}
	int ret = 1;
	fpos_t dest_pos;
	fgetpos(jc->dest, &dest_pos);
	fputs("<doStatement>\n", jc->dest);
	jc_write_xml_tag(jc);
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.type != j_identifier) {
		j_tokenizer_error(&jc->tokenizer,
		"expected an identifier/class name after 'do' keyword");
		fsetpos(jc->dest, &dest_pos);
		return 0;
	}
	jc_write_xml_tag(jc);
	
	j_tokenizer_advance(&jc->tokenizer);
	if(!jc->tokenizer.has_more_tokens) {
		j_tokenizer_error(&jc->tokenizer,
		"invalid do statement syntax");
		fsetpos(jc->dest, &dest_pos);
		return 0;
	}
	// previous token was a class/classVar name
	if(jc->tokenizer.token.name[0] == '.') {
		jc_write_xml_tag(jc);
		
		j_tokenizer_advance(&jc->tokenizer);
		if(!jc->tokenizer.has_more_tokens || jc->tokenizer.token.type != j_identifier) {
			"invalid do statement syntax");
			fsetpos(jc->dest, &dest_pos);
			return 0;
		}
		jc_write_xml_tag(jc);
		
		
	}
	
	fputs("</doStatement>\n", jc->dest);
	return ret;
}
