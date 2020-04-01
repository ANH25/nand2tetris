/* TODO:
- number of lines counting needs testing (?)
- clean up
*/
 
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
//#include <stdint.h>
//#include <inttypes.h>

#include "n2t-common.h"
#include "jack-tokenizer.h"
#include "jack-literals.h"

static const char *j_token_tags[] = {
	[j_unknown] = "unknown",
	[j_keyword] = "keyword",
	[j_symbol] = "symbol",
	[j_int] = "integerConstant",
	[j_str] = "stringConstant",
	[j_identifier] = "identifier"
};

void j_tokenizer_write_xml_el(struct j_tokenizer *jtk, FILE *dest) {
	
	fprintf(dest, "<%s>", j_token_tags[jtk->token.type]);
	for(size_t i = 0; i < jtk->token.name_len; i++) {
		switch(jtk->token.name[i]) {
			case '<':
			fputs("&lt;", dest);
			break;
			case '>':
			fputs("&gt;", dest);
			break;
			case '"':
			fputs("&quot;", dest);
			break;
			case '&':
			fputs("&amp;", dest);
			break;
			default:
			fputc(jtk->token.name[i], dest);
		}
	}
	fprintf(dest, "</%s>\n", j_token_tags[jtk->token.type]);
}

void j_tokenizer_init(struct j_tokenizer *jtk) {
	//jtk->src = NULL;
	//jtk->src_name = NULL;
	jtk->line_n = 1;
	jtk->token.name = NULL;
	jtk->token.name_len = 0;
	jtk->token.name_buf_cap = 0;
	jtk->token.type = 0;
	jtk->token.keyword_type = 0;
	jtk->token.line_n = 1;
	jtk->has_more_tokens = true;
}

int j_tokenizer_advance(struct j_tokenizer *jtk) {
	
	int c, c1;
	fpos_t pos;
	while(1) {
		
		while((c = getc(jtk->src)) != EOF && isspace(c)) {
			if(c == '\n') jtk->line_n++;
		}
		
		fgetpos(jtk->src, &pos);
		
		if(c == '/') {
			c1 = getc(jtk->src);
			if(c == c1) { // one-line comment
				while((c = getc(jtk->src)) != EOF && c != '\n') ;
				if(c == '\n') jtk->line_n++;
				else {
					jtk->has_more_tokens = false;
					return 1;
				}
			}
			else if(c1 == '*') { // multi-line comment
				size_t tmp_line_n = jtk->line_n;
				while((c = getc(jtk->src)) != EOF) {
					c1 = getc(jtk->src);
					if(c == '*' && c1 == '/') break;
					else ungetc(c1, jtk->src);
					if(c == '\n') tmp_line_n++;
				}
				if(feof(jtk->src)) {
					jtk->has_more_tokens = false;
					if(!(c == '*' && c1 == '/')) {
						j_tokenizer_warn(jtk, "unclosed comment");
						return 0;
					}
					return 1;
				}
				jtk->line_n = tmp_line_n;
			}
			else {
				fsetpos(jtk->src, &pos);
				ungetc('/', jtk->src);
				break;
			}
		}
		else {
			ungetc(c, jtk->src);
			if(c == '\n') jtk->line_n++;
			if(feof(jtk->src) || ferror(jtk->src)) {
				jtk->has_more_tokens = false;
				return 1;
			}
			if(!isspace(c) && c != '/') {
				break;
			}
		}
	}
	
	fgetpos(jtk->src, &pos);
	bool token_is_str = false;
	size_t token_len = 0;
	c = getc(jtk->src);
	if(c != '"') {
		ungetc(c, jtk->src);
		while((c = getc(jtk->src)) != EOF && !isspace(c)) {
			char buf[2];
			buf[0] = c;
			buf[1] = '\0';
			const struct j_literal *symbol = j_get_literal(buf, 1);
			if(symbol && symbol->type == j_symbol) {
				if(!token_len) token_len = 1;
				break;
			}
			token_len++;
		};
	}
	else {
		token_is_str = true;
		//todo: support for multi-line strings?
		while((c = getc(jtk->src)) != EOF && c != '"' && c != '\n') {
			token_len++;
		};
		if(c != '"') {
			j_tokenizer_warn(jtk, "unclosed string");
		}
		if(c == '\n') {
			jtk->line_n++;
		}
	}
	
	fsetpos(jtk->src, &pos);
	jtk->token.fpos = pos;
	jtk->token.line_n = jtk->line_n;
	jtk->token.name_len = token_len;	
	
	if(jtk->token.name_buf_cap <= token_len) {
		char *tmp = n2t_realloc(jtk->token.name, token_len+1);
		if(!tmp) {
			n2t_func_error("out of memory");
			return 0;
		}
		jtk->token.name = tmp;
		jtk->token.name_buf_cap = token_len+1;
	}
	
	if(token_is_str) {
		getc(jtk->src); // discard first '"' character
	}
	for(size_t i = 0; i < token_len; i++) {
		jtk->token.name[i] = getc(jtk->src);
	}
	jtk->token.name[token_len] = '\0';
	if(token_is_str) {
		getc(jtk->src); // discard final '"' character
	}
	
	const struct j_literal *literal = j_get_literal(jtk->token.name, jtk->token.name_len);
	if(literal) {
		jtk->token.type = literal->type;
		jtk->token.keyword_type = literal->keyword_type;
	}
	else {
		jtk->token.keyword_type = 0;
		if(token_is_str) {
			jtk->token.type = j_str;
		}
		/*else if(isdigit(jtk->token.name[0])) {
			errno = 0;
			uintmax_t num = strtoumax(jtk->token.name, NULL, 10);
			if(errno || num > HACK_ADDRESS_MAX) {
				//fprintf(stderr, "%s:%zu: warning: out-of-range integer constant\n",
				//jtk->src_name, jtk->line_n);
				j_tokenizer_warn(jtk, "out of range integer constant");
			}
			jtk->token.type = j_int;
		}
		*/
		else if(isdigit(jtk->token.name[0])) {
			//fixme: should we check the range of the integer here or leave it to the parsing stage?
			size_t digits_n;
			for(digits_n = 1;
			digits_n < jtk->token.name_len && isdigit(jtk->token.name[digits_n]);
			digits_n++)
			;
			if(digits_n == jtk->token.name_len) {
				/* a valid integer constant */
				jtk->token.type = j_int;
			}
			else {
				/* can't be a valid integer constant or a valid identifier */
				jtk->token.type = j_unknown;
			}
		}
		else {
			/* NOTE:
			currently this allows for some kinds of identifiers not starting with letters
			or underscores like '?wtf', '^foo'
			*/
			jtk->token.type = j_identifier;
		}
	}
	if(feof(jtk->src)) jtk->has_more_tokens = false;
	if(ferror(jtk->src)) {
		n2t_func_error_f("error while processing source file %s", jtk->src_name);
		return 0;
	}
	return 1;
}

int j_tokenizer_retreat(struct j_tokenizer *jtk) {
	jtk->line_n = jtk->token.line_n;
	jtk->has_more_tokens = true;
	return fsetpos(jtk->src, &jtk->token.fpos);
}

void j_tokenizer_free(struct j_tokenizer *jtk) {
	
	free(jtk->token.name);
	jtk->token.name = NULL;
	jtk->token.name_len = 0;
	jtk->token.name_buf_cap = 0;
	jtk->token.type = 0;
	jtk->line_n = 1;
	jtk->has_more_tokens = true;
}
