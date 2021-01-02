/* Hack Virtual Machine Language Translator */

/* 
IMPORTANT:
implementing optimizations like uncalled functions removal, etc. is probably best done
in a two-pass translator. So I'll probably call it a day improving this version further
and write a more advanced, yet more convenient one.
*/
/* TODO
- clean up the mess!
- optimizations
- push/pop operations can be optimized when the segment address offset is 0, or 1 in some cases (DONE)
- context-dependent optimizations (requires significant changes)
- dead code elimination
- make label generation compatible with the VM specification (See slides 8.) (DONE)
- fancy error reporting for things like undefined functions, labels, etc. (?)
  (probably not for functions, since we can have functions in different files
   that call each another by calling vm_translate() multiple times)
- labels in the VM language have function scope,
  so we can have multiple labels with the same names in different functions.
  we should prefix these labels with their function names
  so that they look different to the assembler (DONE)
- refactor static variables like call_i to make it thread-safe
- different optimization options for code size and execution speed
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <stdbool.h>

#include "n2t-common.h"
#include "command-table.h"
#include "segment-table.h"


int vm_segment_translate_push_pointer(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra) {

	static const char *asm_code = "\
@%s\n\
D=M\n\
@SP\n\
M=M+1\n\
A=M-1\n\
M=D\n\
";
	if(extra->address > 1) {
		hvm_error(vm, "'push pointer' operation address should be 0 or 1");
		return 0;
	}
	static const char *segments[2] = {"THIS", "THAT"};
	fprintf(vm->dest, asm_code, segments[extra->address]);
	return 1;
}

int vm_segment_translate_push_temp(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra) {

	static const char *asm_code = "\
@R%u\n\
D=M\n\
@SP\n\
M=M+1\n\
A=M-1\n\
M=D\n\
";
	if(extra->address > 7) {
		hvm_error(vm, "'push temp' operation address should be in the range [0,7]");
		return 0;
	}
	
	fprintf(vm->dest, asm_code, 
	5 + extra->address);
	return 1;
}

int vm_segment_translate_push_constant(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra) {
	
	static const char *asm_code = "\
@%s\n\
D=A\n\
@SP\n\
M=M+1\n\
A=M-1\n\
M=D\n\
";

	static const char *asm_code_01 = "\
@SP\n\
M=M+1\n\
A=M-1\n\
M=%s\n\
";

	if(!extra->address || extra->address == 1) {
		fprintf(vm->dest, asm_code_01, 
		&vmline->line[vmline->tokens_indices[4]]);
	}
	else {
		fprintf(vm->dest, asm_code, 
		&vmline->line[vmline->tokens_indices[4]]);
	}
	return 1;
}

int vm_segment_translate_push_basic(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra) {

	static const char *asm_code = "\
@%s\n\
D=A\n\
@%s\n\
A=D+M\n\
D=M\n\
@SP\n\
M=M+1\n\
A=M-1\n\
M=D\n\
";

	static const char *asm_code_0 = "\
@%s\n\
A=M\n\
D=M\n\
@SP\n\
M=M+1\n\
A=M-1\n\
M=D\n\
";

	static const char *asm_code_1 = "\
@%s\n\
A=M+1\n\
D=M\n\
@SP\n\
M=M+1\n\
A=M-1\n\
M=D\n\
";
	if(!extra->address) {
		fprintf(vm->dest, asm_code_0,
		extra->asm_name);
	}
	else if(extra->address == 1) {
		fprintf(vm->dest, asm_code_1,
		extra->asm_name);
	}
	else {
		fprintf(vm->dest, asm_code, 
		&vmline->line[vmline->tokens_indices[4]],
		extra->asm_name);
	}
	
	return 1;
}

int vm_segment_translate_push_static(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra) {

	static const char *asm_code = "\
@%s.%s\n\
D=M\n\
@SP\n\
M=M+1\n\
A=M-1\n\
M=D\n\
";
	fprintf(vm->dest, asm_code,
	vm->base_name,
	&vmline->line[vmline->tokens_indices[4]]);
	return 1;
}

int vm_segment_translate_pop_pointer(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra) {
	
	static const char *asm_code = "\
@SP\n\
AM=M-1\n\
D=M\n\
@%s\n\
M=D\n\
";
	
	if(extra->address > 1) {
		hvm_error(vm, "'pop pointer' operation address should be 0 or 1");
		return 0;
	}
	static const char *segments[2] = {"THIS", "THAT"};
	fprintf(vm->dest, asm_code, segments[extra->address]);
	return 1;
}

int vm_segment_translate_pop_temp(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra) {
	
	static const char *asm_code = "\
@SP\n\
AM=M-1\n\
D=M\n\
@R%u\n\
M=D\n\
";
	
	if(extra->address > 7) {
		hvm_error(vm, "'pop temp' operation address should be in the range [0,7]");
		return 0;
	}
	
	fprintf(vm->dest, asm_code, 5 + extra->address);
	return 1;
}

int vm_segment_error_pop_constant(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra) {
	
	hvm_error(vm,
	"'pop constant' does not make sense. Do you mean 'push'?");
	return 0;
	
}

int vm_segment_translate_pop_basic(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra) {
/*	
	static const char *asm_code = "\
@SP\n\
AM=M-1\n\
D=M\n\
@R13\n\
M=D\n\
@%s\n\
D=A\n\
@%s\n\
D=D+M\n\
@R14\n\
M=D\n\
@R13\n\
D=M\n\
@R14\n\
A=M\n\
M=D\n\
";
*/
	static const char *asm_code = "\
@%s\n\
D=A\n\
@%s\n\
D=D+M\n\
@R14\n\
M=D\n\
@SP\n\
AM=M-1\n\
D=M\n\
@R14\n\
A=M\n\
M=D\n\
";

	static const char *asm_code_0 = "\
@SP\n\
AM=M-1\n\
D=M\n\
@%s\n\
A=M\n\
M=D\n\
";

	static const char *asm_code_1 = "\
@SP\n\
AM=M-1\n\
D=M\n\
@%s\n\
A=M+1\n\
M=D\n\
";

	if(!extra->address) {
		fprintf(vm->dest, asm_code_0,
		extra->asm_name);
	}
	else if(extra->address == 1) {
		fprintf(vm->dest, asm_code_1,
		extra->asm_name);
	}
	else {
		fprintf(vm->dest, asm_code, 
		&vmline->line[vmline->tokens_indices[4]],
		extra->asm_name);
	}
	
	return 1;
}

int vm_segment_translate_pop_static(struct vm_line *vmline, struct vm *vm, struct vm_segment_extra *extra) {
	
	static const char *asm_code = "\
@SP\n\
AM=M-1\n\
D=M\n\
@%s.%s\n\
M=D\n\
";
	fprintf(vm->dest, asm_code,
	vm->base_name,
	&vmline->line[vmline->tokens_indices[4]]);
	return 1;
}

int vm_cmd_push_pop_dispatch(struct vm_line *vmline, struct vm *vm) {
	
	if(vmline->token_num < 3) {
		hvm_error_f(vm,
		"A segment name and a non-negative integer address expected after the %s keyword",
		&vmline->line[vmline->tokens_indices[0]]);
		return 0;
	}
	
	struct vm_segment *segment = 
	vm_get_segment(&vmline->line[vmline->tokens_indices[2]],
	               vmline->tokens_indices[3] - vmline->tokens_indices[2]);
	if(!segment) {
		hvm_error(vm,
		"unrecognized segment name");
		return 0;
	}
	bool is_push = false;
	if(!strcmp(&vmline->line[vmline->tokens_indices[0]], "push")) {
		is_push = true;
	}
	
	char *endptr;
	errno = 0;
	intmax_t addr = strtoimax(&vmline->line[vmline->tokens_indices[4]], &endptr, 10);
	if(!addr && endptr != &vmline->line[vmline->tokens_indices[5]]) {
		hvm_error_f(vm,
		"expected an integer address, got '%s'", &vmline->line[vmline->tokens_indices[4]]);
		return 0;
	}
	if(addr < 0) {
		hvm_error(vm,
		"segment address offset should not be negative");
		return 0;
	}
	if(errno == ERANGE || addr > HACK_ADDRESS_MAX) {
		hvm_error_f(vm,
		"segment address offset is greater than maximum value '%u'", HACK_ADDRESS_MAX);
		return 0;
	}
	/* additional address checking can be performed by each segment handler */
	segment->extra.address = (uint16_t)addr;
	if(is_push) {
		return segment->push_op(vmline, vm, &segment->extra);
	}
	else {
		return segment->pop_op(vmline, vm, &segment->extra);
	}
}

int vm_cmd_translate_add(struct vm_line *vmline, struct vm *vm) {

	static const char *asm_code = "\
@SP\n\
AM=M-1\n\
D=M\n\
@R13\n\
M=D\n\
@SP\n\
A=M-1\n\
D=M\n\
@R13\n\
D=M+D\n\
@SP\n\
A=M-1\n\
M=D\n\
";
	fprintf(vm->dest, asm_code);
	return 1;
}

int vm_cmd_translate_sub(struct vm_line *vmline, struct vm *vm) {

	static const char *asm_code = "\
@SP\n\
AM=M-1\n\
D=M\n\
@R13\n\
M=D\n\
@SP\n\
A=M-1\n\
D=M\n\
@R13\n\
D=D-M\n\
@SP\n\
A=M-1\n\
M=D\n\
";
	fprintf(vm->dest, asm_code);
	return 1;
}

int vm_cmd_translate_neg(struct vm_line *vmline, struct vm *vm) {

	static const char *asm_code = "\
@SP\n\
A=M-1\n\
M=-M\n\
";
	fprintf(vm->dest, asm_code);
	return 1;
}

int vm_cmd_translate_eq(struct vm_line *vmline, struct vm *vm) {

	static const char *asm_code = "\
@SP\n\
AM=M-1\n\
D=M\n\
@R13\n\
M=D\n\
@SP\n\
A=M-1\n\
D=M\n\
@R13\n\
D=M-D\n\
@%s_EQ_%u\n\
D;JEQ\n\
D=0\n\
@%s_EQ_%u_END\n\
0;JMP\n\
(%s_EQ_%u)\n\
D=-1\n\
(%s_EQ_%u_END)\n\
@SP\n\
A=M-1\n\
M=D\n\
";
	static unsigned int eq_i = 0;
	fprintf(vm->dest, asm_code, 
	vm->base_name, eq_i,
	vm->base_name, eq_i,
	vm->base_name, eq_i,
	vm->base_name, eq_i
	);
	eq_i++;
	return 1;
}

int vm_cmd_translate_gt(struct vm_line *vmline, struct vm *vm) {

	static const char *asm_code = "\
@SP\n\
AM=M-1\n\
D=M\n\
@R13\n\
M=D\n\
@SP\n\
A=M-1\n\
D=M\n\
@R13\n\
D=D-M\n\
@%s_GT_%u\n\
D;JGT\n\
D=0\n\
@%s_GT_%u_END\n\
0;JMP\n\
(%s_GT_%u)\n\
D=-1\n\
(%s_GT_%u_END)\n\
@SP\n\
A=M-1\n\
M=D\n\
";
	static unsigned int gt_i = 0;
	fprintf(vm->dest, asm_code,
	vm->base_name, gt_i,
	vm->base_name, gt_i,
	vm->base_name, gt_i,
	vm->base_name, gt_i
	);
	gt_i++;
	return 1;
}

int vm_cmd_translate_lt(struct vm_line *vmline, struct vm *vm) {

	static const char *asm_code = "\
@SP\n\
AM=M-1\n\
D=M\n\
@R13\n\
M=D\n\
@SP\n\
A=M-1\n\
D=M\n\
@R13\n\
D=D-M\n\
@%s_LT_%u\n\
D;JLT\n\
D=0\n\
@%s_LT_%u_END\n\
0;JMP\n\
(%s_LT_%u)\n\
D=-1\n\
(%s_LT_%u_END)\n\
@SP\n\
A=M-1\n\
M=D\n\
";
	static unsigned int lt_i = 0;
	fprintf(vm->dest, asm_code,
	vm->base_name, lt_i,
	vm->base_name, lt_i,
	vm->base_name, lt_i,
	vm->base_name, lt_i
	);
	lt_i++;
	return 1;
}

int vm_cmd_translate_and(struct vm_line *vmline, struct vm *vm) {

	static const char *asm_code = "\
@SP\n\
AM=M-1\n\
D=M\n\
@R13\n\
M=D\n\
@SP\n\
A=M-1\n\
D=M\n\
@R13\n\
D=D&M\n\
@SP\n\
A=M-1\n\
M=D\n\
";

	fprintf(vm->dest, asm_code);
	return 1;
}

int vm_cmd_translate_or(struct vm_line *vmline, struct vm *vm) {

	static const char *asm_code = "\
@SP\n\
AM=M-1\n\
D=M\n\
@R13\n\
M=D\n\
@SP\n\
A=M-1\n\
D=M\n\
@R13\n\
D=D|M\n\
@SP\n\
A=M-1\n\
M=D\n\
";
	fprintf(vm->dest, asm_code);
	return 1;
}

int vm_cmd_translate_not(struct vm_line *vmline, struct vm *vm) {

	static const char *asm_code = "\
@SP\n\
A=M-1\n\
M=!M\n\
";
	fprintf(vm->dest, asm_code);
	return 1;
}

int vm_cmd_translate_label(struct vm_line *vmline, struct vm *vm) {
	
	if(vm->cur_func) {
		fprintf(vm->dest, "(%s$%s)\n",
		vm->cur_func, &vmline->line[vmline->tokens_indices[2]]);
	}
	else {
		fprintf(vm->dest, "(%s)\n",
		&vmline->line[vmline->tokens_indices[2]]);
	}
	return 1;
}

int vm_cmd_translate_goto(struct vm_line *vmline, struct vm *vm) {

	static const char *asm_code_1 = "\
@%s$%s\n\
0;JMP\n";
	static const char *asm_code_2 = "\
@%s\n\
0;JMP\n";
	if(vm->cur_func) {
		fprintf(vm->dest, asm_code_1, vm->cur_func, &vmline->line[vmline->tokens_indices[2]]);
	}
	else {
		fprintf(vm->dest, asm_code_2, &vmline->line[vmline->tokens_indices[2]]);
	}
	return 1;
}

int vm_cmd_translate_ifgoto(struct vm_line *vmline, struct vm *vm) {

	static const char *asm_code_1 = "\
@SP\n\
AM=M-1\n\
D=M\n\
@%s$%s\n\
D;JNE\n";

	static const char *asm_code_2 = "\
@SP\n\
AM=M-1\n\
D=M\n\
@%s\n\
D;JNE\n";
	
	if(vm->cur_func) {
		fprintf(vm->dest, asm_code_1, vm->cur_func, &vmline->line[vmline->tokens_indices[2]]);
	}
	else {
		fprintf(vm->dest, asm_code_2, &vmline->line[vmline->tokens_indices[2]]);
	}
	
	return 1;
}

#define PUSH_D_INC_SP_ASM "\
@SP\n\
A=M\n\
M=D\n\
@SP\n\
M=M+1\n\
"

int vm_cmd_translate_call(struct vm_line *vmline, struct vm *vm) {
	
	static const char *asm_code = 
/* push retAddrLabel */
"\
@%s.ret.%u\n\
D=A\n\
"
PUSH_D_INC_SP_ASM
/* push LCL */
"\
@LCL\n\
D=M\n\
"
PUSH_D_INC_SP_ASM
/* push ARG */
"\
@ARG\n\
D=M\n\
"
PUSH_D_INC_SP_ASM
/* push THIS */
"\
@THIS\n\
D=M\n\
"
PUSH_D_INC_SP_ASM
/* push THAT */
"\
@THAT\n\
D=M\n\
"
PUSH_D_INC_SP_ASM
/* ARG = SP - 5 - nArgs */
"\
@SP\n\
D=M\n\
@5\n\
D=D-A\n\
@%s\n\
D=D-A\n\
@ARG\n\
M=D\n\
"
/* LCL = SP */
"\
@SP\n\
D=M\n\
@LCL\n\
M=D\n\
"
/* goto functionName */
"\
@%s\n\
0;JMP\n\
"
/* (retAddrLabel) */
"(%s.ret.%u)\n"
	;
	
	if(vmline->token_num < 3) {
		hvm_error(vm,
		"the 'call' keyword should be followed by a function name \
and a positive number indicating the number of the function arguments");
		
		return 0;
	}
	
	static unsigned int call_i = 0;
	fprintf(vm->dest, asm_code,
	vm->base_name, call_i,
	&vmline->line[vmline->tokens_indices[4]], /* nArgs */
	&vmline->line[vmline->tokens_indices[2]], /* functionName */
	vm->base_name, call_i
	);
	call_i++;
	return 1;
}

int vm_cmd_translate_function(struct vm_line *vmline, struct vm *vm) {
	
	if(vmline->token_num < 3) {
		hvm_error(vm,
		"the 'function' keyword should be followed by a function name \
and a positive number indicating how many local variables to allocate for the function");

		return 0;
	}
	
	char *endptr;
	errno = 0;
	intmax_t nArgs = strtoimax(&vmline->line[vmline->tokens_indices[4]], &endptr, 10);
	if((nArgs == 0 && endptr == &vmline->line[vmline->tokens_indices[4]]) ||
	   nArgs < 0
	) {
		hvm_error_f(vm,
		"expected a positive integer for the number of local variables of function '%s', got '%s'",
		&vmline->line[vmline->tokens_indices[2]],
		&vmline->line[vmline->tokens_indices[4]]);
		return 0;
	}
	if(errno == ERANGE || nArgs > HACK_ADDRESS_MAX) {
		hvm_error_f(vm,
		"number of local variables of function '%s' is too large",
		&vmline->line[vmline->tokens_indices[2]]);
		return 0;
	}
	
	fprintf(vm->dest, "(%s)\n",
	&vmline->line[vmline->tokens_indices[2]] /* functionName */);
	
	if(vm->cur_func) {
		free(vm->cur_func);
	}
	vm->cur_func = n2t_strdup_n(&vmline->line[vmline->tokens_indices[2]], 
	vmline->tokens_indices[3]-vmline->tokens_indices[2]);
	//abort_if_oom(vm->cur_func);
	
	static const char *asm_push_0 = "\
M=0\n\
@SP\n\
AM=M+1\n\
";
	if(nArgs) {
		fprintf(vm->dest, "@SP\nA=M\n");
		for(intmax_t i = 0; i < nArgs; i++) {
		fprintf(vm->dest, asm_push_0);
		}
	}
	
	return 1;
}

int vm_cmd_translate_return(struct vm_line *vmline, struct vm *vm) {
	
	static const char *asm_code = 
/* endFrame = LCL */
"\
@LCL\n\
D=M\n\
@R13\n\
M=D\n\
"
/* retAddr = *(endFrame - 5) */
"\
@5\n\
A=D-A\n\
D=M\n\
@R14\n\
M=D\n\
"
/* *ARG = pop() */
"\
@SP\n\
AM=M-1\n\
D=M\n\
@ARG\n\
A=M\n\
M=D\n\
"
/* SP = ARG + 1 */
"\
@ARG\n\
D=M+1\n\
@SP\n\
M=D\n\
"
/* THAT = *(endFrame - 1) */
"\
@R13\n\
A=M-1\n\
D=M\n\
@THAT\n\
M=D\n\
"
/* THIS = *(endFrame – 2) */
"\
@R13\n\
D=M\n\
@2\n\
A=D-A\n\
D=M\n\
@THIS\n\
M=D\n\
"
/* ARG = *(endFrame – 3) */
"\
@R13\n\
D=M\n\
@3\n\
A=D-A\n\
D=M\n\
@ARG\n\
M=D\n\
"
/* LCL = *(endFrame – 4) */
"\
@R13\n\
D=M\n\
@4\n\
A=D-A\n\
D=M\n\
@LCL\n\
M=D\n\
"
/* goto retAddr */
"\
@R14\n\
A=M\n\
0;JMP\n\
"
;
	
	if(vm->has_return) {
		fputs("@__vm_ret__\n0;JMP\n", vm->dest);
	}
	else {
		fputs("(__vm_ret__)\n", vm->dest);
		fputs(asm_code, vm->dest);
		vm->has_return = true;
	}
	
	//fputs(asm_code, vm->dest);
	
	return 1;
}


static inline void vm_write_init(struct vm *vm) {
		
		fprintf(vm->dest, "\
@256\n\
D=A\n\
@SP\n\
M=D\n\
"
/* push retAddrLabel */
"\
@Sys.halt\n\
D=A\n\
"
PUSH_D_INC_SP_ASM
/* push LCL */
"\
@LCL\n\
D=M\n\
"
PUSH_D_INC_SP_ASM
/* push ARG */
"\
@ARG\n\
D=M\n\
"
PUSH_D_INC_SP_ASM
/* push THIS */
"\
@THIS\n\
D=M\n\
"
PUSH_D_INC_SP_ASM
/* push THAT */
"\
@THAT\n\
D=M\n\
"
PUSH_D_INC_SP_ASM
/* ARG = SP - 5 - nArgs */
"\
@SP\n\
D=M\n\
@5\n\
D=D-A\n\
@ARG\n\
M=D\n\
"
/* LCL = SP */
"\
@SP\n\
D=M\n\
@LCL\n\
M=D\n\
"
/* goto functionName */
"\
@Sys.init\n\
0;JMP\n\
"
/* (retAddrLabel) */
"(Sys.halt)\n\
@Sys.halt\n\
0;JMP\n\
");

}


void vm_parse_line(struct vm_line *vmline) {
	
	size_t i = 0;
	vmline->token_num = 0;
	for(unsigned char k = 0; k < 3; k++) {
		
		while(isspace(vmline->line[i])) i++;
		if(!vmline->line[i]) return;
		if(vmline->line[i] == vmline->line[i+1] && vmline->line[i] == '/') return;
		
		vmline->tokens_indices[k*2] = i;
		while(vmline->line[i] && 
			  !isspace(vmline->line[i]) &&
			  !(vmline->line[i] == vmline->line[i+1] && vmline->line[i] == '/')
			  ) 
		{
			i++;	  
		}
		vmline->tokens_indices[k*2 + 1] = i;
		vmline->token_num++;
	}
	/* check for excessive tokens */
	while(isspace(vmline->line[i])) i++;
	if(vmline->line[i] && 
	   !(vmline->line[i] == vmline->line[i+1] && 
	     vmline->line[i] == '/'))
	   {
		   vmline->token_num = -1;
	   }
}

#if 0
char* vm_dir_get_first_file(struct vm *vm, struct n2t_dir_search *dir_srch) {
	
	if(vm->ptype == n2t_path_file) {
		n2t_parse_path(&vm->src_pinfo, vm->src_path);
		return vm->src_path;
	}
	char *ret = n2t_dir_get_first_file(dir_srch,
	vm->user_path, strlen(vm->user_path),
	"vm");
	
	n2t_parse_path(&vm->src_pinfo, ret);
	return ret;
}

char *vm_dir_get_next_file(struct vm *vm, struct n2t_dir_search *dir_srch) {
	
	if(vm->ptype == n2t_path_file) return NULL;
	char *ret = n2t_dir_get_next_file(dir_srch);
	//fprintf(stderr, "vm_dir_get_next_file ret = '%s'\n", ret);
	n2t_parse_path(&vm->src_pinfo, ret);
	return ret;
}
#endif

int vm_translate(struct vm *vm) {
	
	int ret = 1;
	char *line = NULL;
	size_t line_buf_len = 0;
	
	if(!vm->noinit) {
		vm_write_init(vm);
	}
	
	size_t line_len;
	size_t line_num = 0;
	while((line_len = n2t_getline(&line, &line_buf_len, vm->src)) != (size_t)-1) {
		
		line_num++;
		struct vm_line vmline;
		vm->line = &vmline;
		vmline.line = line;
		vmline.line_num = line_num;
		vm_parse_line(&vmline);
		if(vmline.token_num == 0) continue;
		line[vmline.tokens_indices[1]] = '\0';
		if(vmline.token_num >= 2) {
			line[vmline.tokens_indices[3]] = '\0';
		}
		if(vmline.token_num >= 3) {
			line[vmline.tokens_indices[5]] = '\0';
		}
		
		const struct vm_command *cmd = vm_get_command(&line[vmline.tokens_indices[0]], 
		vmline.tokens_indices[1] - vmline.tokens_indices[0]);
		if(cmd) {
			if(vm->annotated) {
				fputs("// ", vm->dest);
				for(unsigned char k = 0; k < cmd->token_num; k++) {
					fprintf(vm->dest, "%s ", &line[vmline.tokens_indices[k*2]]);
				}
				fputc('\n', vm->dest);
			}
			ret = cmd->op(&vmline, vm);
			if(vmline.token_num > cmd->token_num) {
				/* warn about excess tokens if found */
				hvm_warn_f(vm, "excess tokens after '%s' command", cmd->name);
			}
		}
		else {
			hvm_error_f(vm, "unrecognized keyword '%s'",
			&line[vmline.tokens_indices[0]]);
			ret = 0;
		}
		errno = 0;
	}
	if(errno || ferror(vm->src)) {
		hvm_func_error(vm, "Error while processing source file");
		ret = 0;
		//fclose(vm->src);
		goto CLEAN;
	}
	//fclose(vm->src);
	
CLEAN:	
	free(line);
	
	return ret;
}

void vm_init(struct vm *vm) {
	memset(vm, 0, sizeof(struct vm));
}

void vm_free(struct vm *vm) {
	if(vm->cur_func) {
		free(vm->cur_func);
	}
}

