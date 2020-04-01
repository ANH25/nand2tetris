/* Hack Virtual Machine Language Translator */

/* TODO
- should I use the `inline` qualifier for code generation functions?
- peephole optimizations
- error reporting for undefined labels (?)
- different optimization options for code size and execution speed (?)
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
#include "hvm.h"


int vm_segment_translate_push_pointer(struct vm *vm, struct vm_segment_extra *extra) {

	static const char *asm_code = "\
@%s\n\
D=M\n\
@SP\n\
M=M+1\n\
A=M-1\n\
M=D\n\
";
	uint16_t address = vm->vm_cmds.data[vm->cur_cmd_i].address;
	if(address > 1) {
		hvm_error(vm, "'push pointer' operation address should be 0 or 1");
		return 0;
	}
	static const char *segments[2] = {"THIS", "THAT"};
	fprintf(vm->dest, asm_code, segments[address]);
	return 1;
}

int vm_segment_translate_push_temp(struct vm *vm, struct vm_segment_extra *extra) {

	static const char *asm_code = "\
@R%" PRIu16 "\n\
D=M\n\
@SP\n\
M=M+1\n\
A=M-1\n\
M=D\n\
";
	uint16_t address = vm->vm_cmds.data[vm->cur_cmd_i].address;
	if(address > 7) {
		hvm_error(vm, "'push temp' operation address should be in the range [0,7]");
		return 0;
	}
	
	fprintf(vm->dest, asm_code, 
	5 + address);
	return 1;
}

int vm_segment_translate_push_constant(struct vm *vm, struct vm_segment_extra *extra) {
	
	static const char *asm_code = "\
@%" PRIu16 "\n\
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
M=%" PRIu16 "\n";

	uint16_t address = vm->vm_cmds.data[vm->cur_cmd_i].address;
	if(address <= 1) {
		fprintf(vm->dest, asm_code_01, 
		address);
	}
	else {
		fprintf(vm->dest, asm_code, address);
	}
	return 1;
}

int vm_segment_translate_push_basic(struct vm *vm, struct vm_segment_extra *extra) {

	static const char *asm_code = "\
@%" PRIu16 "\n\
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
	uint16_t address = vm->vm_cmds.data[vm->cur_cmd_i].address;
	if(!address) {
		fprintf(vm->dest, asm_code_0,
		extra->asm_name);
	}
	else if(address == 1) {
		fprintf(vm->dest, asm_code_1,
		extra->asm_name);
	}
	else {
		fprintf(vm->dest, asm_code, 
		address,
		extra->asm_name);
	}
	
	return 1;
}

int vm_segment_translate_push_static(struct vm *vm, struct vm_segment_extra *extra) {

	static const char *asm_code = "\
@%s.%zu.%" PRIu16 "\n\
D=M\n\
@SP\n\
M=M+1\n\
A=M-1\n\
M=D\n\
";
	fprintf(vm->dest, asm_code,
	vm->base_name, vm->cur_src_i,
	vm->vm_cmds.data[vm->cur_cmd_i].address);
	return 1;
}

int vm_segment_translate_pop_pointer(struct vm *vm, struct vm_segment_extra *extra) {
	
	static const char *asm_code = "\
@SP\n\
AM=M-1\n\
D=M\n\
@%s\n\
M=D\n\
";
	uint16_t address = vm->vm_cmds.data[vm->cur_cmd_i].address;
	if(address > 1) {
		hvm_error(vm, "'pop pointer' operation address should be 0 or 1");
		return 0;
	}
	static const char *segments[2] = {"THIS", "THAT"};
	fprintf(vm->dest, asm_code, segments[address]);
	return 1;
}

int vm_segment_translate_pop_temp(struct vm *vm, struct vm_segment_extra *extra) {
	
	static const char *asm_code = "\
@SP\n\
AM=M-1\n\
D=M\n\
@R%" PRIu16 "\n\
M=D\n\
";
	uint16_t address = vm->vm_cmds.data[vm->cur_cmd_i].address;
	if(address > 7) {
		hvm_error(vm, "'pop temp' operation address should be in the range [0,7]");
		return 0;
	}
	
	fprintf(vm->dest, asm_code, 5 + address);
	return 1;
}

int vm_segment_error_pop_constant(struct vm *vm, struct vm_segment_extra *extra) {
	
	hvm_error(vm,
	"'pop constant' does not make sense. Do you mean 'push'?");
	return 0;
	
}

int vm_segment_translate_pop_basic(struct vm *vm, struct vm_segment_extra *extra) {

	static const char *asm_code = "\
@%" PRIu16 "\n\
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
	uint16_t address = vm->vm_cmds.data[vm->cur_cmd_i].address;
	if(!address) {
		fprintf(vm->dest, asm_code_0,
		extra->asm_name);
	}
	else if(address == 1) {
		fprintf(vm->dest, asm_code_1,
		extra->asm_name);
	}
	else {
		fprintf(vm->dest, asm_code, 
		address,
		extra->asm_name);
	}
	
	return 1;
}

int vm_segment_translate_pop_static(struct vm *vm, struct vm_segment_extra *extra) {
	
	static const char *asm_code = "\
@SP\n\
AM=M-1\n\
D=M\n\
@%s.%zu.%" PRIu16 "\n\
M=D\n\
";
	fprintf(vm->dest, asm_code,
	vm->base_name, vm->cur_src_i,
	vm->vm_cmds.data[vm->cur_cmd_i].address);
	return 1;
}

int vm_cmd_push_pop_dispatch(struct vm *vm) {
	
	struct vm_cmd *cmd = vm->vm_cmds.data + vm->cur_cmd_i;
	if(cmd->vmline.token_num < 3) {
		hvm_error_f(vm,
		"A segment name and a non-negative integer address expected after the %s keyword",
		cmd->name);
		return 0;
	}
	
	struct vm_segment *segment = 
	vm_get_segment(cmd->segment,
	               cmd->vmline.tokens_indices[3] - cmd->vmline.tokens_indices[2]);
	if(!segment) {
		hvm_error_f(vm,
		"unrecognized segment name '%s'", cmd->segment);
		return 0;
	}
	
	if(cmd->type == vm_cmd_push) {
		return segment->push_op(vm, &segment->extra);
	}
	else {
		return segment->pop_op(vm, &segment->extra);
	}
}

int vm_cmd_translate_add(struct vm *vm) {

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
	fputs(asm_code, vm->dest);
	return 1;
}

int vm_cmd_translate_sub(struct vm *vm) {

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
	fputs(asm_code, vm->dest);
	return 1;
}

int vm_cmd_translate_neg(struct vm *vm) {

	static const char *asm_code = "\
@SP\n\
A=M-1\n\
M=-M\n\
";
	fputs(asm_code, vm->dest);
	return 1;
}

int vm_cmd_translate_eq(struct vm *vm) {

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
@%s$EQ%" PRIu16 "\n\
D;JEQ\n\
D=0\n\
@%s$EQ%" PRIu16 "_END\n\
0;JMP\n\
(%s$EQ%" PRIu16 ")\n\
D=-1\n\
(%s$EQ%" PRIu16 "_END)\n\
@SP\n\
A=M-1\n\
M=D\n\
";
	fprintf(vm->dest, asm_code, 
	vm->cur_func.data, vm->eq_i,
	vm->cur_func.data, vm->eq_i,
	vm->cur_func.data, vm->eq_i,
	vm->cur_func.data, vm->eq_i
	);
	vm->eq_i++;
	return 1;
}

int vm_cmd_translate_gt(struct vm *vm) {

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
@%s$GT%" PRIu16 "\n\
D;JGT\n\
D=0\n\
@%s$GT%" PRIu16 "_END\n\
0;JMP\n\
(%s$GT%" PRIu16 ")\n\
D=-1\n\
(%s$GT%" PRIu16 "_END)\n\
@SP\n\
A=M-1\n\
M=D\n\
";
	fprintf(vm->dest, asm_code,
	vm->cur_func.data, vm->gt_i,
	vm->cur_func.data, vm->gt_i,
	vm->cur_func.data, vm->gt_i,
	vm->cur_func.data, vm->gt_i
	);
	vm->gt_i++;
	return 1;
}

int vm_cmd_translate_lt(struct vm *vm) {

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
@%s$LT%" PRIu16 "\n\
D;JLT\n\
D=0\n\
@%s$LT%" PRIu16 "_END\n\
0;JMP\n\
(%s$LT%" PRIu16 ")\n\
D=-1\n\
(%s$LT%" PRIu16 "_END)\n\
@SP\n\
A=M-1\n\
M=D\n\
";
	fprintf(vm->dest, asm_code,
	vm->cur_func.data, vm->lt_i,
	vm->cur_func.data, vm->lt_i,
	vm->cur_func.data, vm->lt_i,
	vm->cur_func.data, vm->lt_i
	);
	vm->lt_i++;
	return 1;
}

int vm_cmd_translate_and(struct vm *vm) {

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

	fputs(asm_code, vm->dest);
	return 1;
}

int vm_cmd_translate_or(struct vm *vm) {

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
	fputs(asm_code, vm->dest);
	return 1;
}

int vm_cmd_translate_not(struct vm *vm) {

	static const char *asm_code = "\
@SP\n\
A=M-1\n\
M=!M\n\
";
	fputs(asm_code, vm->dest);
	return 1;
}

int vm_cmd_translate_label(struct vm *vm) {
	
	struct vm_cmd *cmd = vm->vm_cmds.data + vm->cur_cmd_i;
	if(cmd->vmline.token_num <= 1) {
		hvm_error(vm, "label keyword should be followed by a label identifier");
		return 0;
	}
	
	fprintf(vm->dest, "(%s$%s)\n",
	vm->cur_func.data, cmd->label);
	
	return 1;
}

int vm_cmd_translate_goto(struct vm *vm) {

	static const char *asm_code_1 = "\
@%s$%s\n\
0;JMP\n";
	
	struct vm_cmd *cmd = vm->vm_cmds.data + vm->cur_cmd_i;
	if(cmd->vmline.token_num <= 1) {
		hvm_error(vm, "goto keyword should be followed by a label identifier");
		return 0;
	}
	
	fprintf(vm->dest, asm_code_1, 
	vm->cur_func.data, cmd->label);
		
	return 1;
}

int vm_cmd_translate_ifgoto(struct vm *vm) {

	static const char *asm_code_1 = "\
@SP\n\
AM=M-1\n\
D=M\n\
@%s$%s\n\
D;JNE\n";
	
	struct vm_cmd *cmd = vm->vm_cmds.data + vm->cur_cmd_i;
	if(cmd->vmline.token_num <= 1) {
		hvm_error(vm, "if-goto keyword should be followed by a label identifier");
		return 0;
	}
	
	fprintf(vm->dest, asm_code_1, 
	vm->cur_func.data, cmd->label);
	
	return 1;
}

#define PUSH_D_INC_SP_ASM "\
@SP\n\
A=M\n\
M=D\n\
@SP\n\
M=M+1\n\
"

int vm_cmd_translate_call(struct vm *vm) {

	static const char *pre_call_func_asm = "@%s\n\
D=A\n\
@R15\n\
M=D\n\
@%" PRIu16 "\n\
D=A\n\
@R14\n\
M=D\n\
@%s$ret%" PRIu16 "\n\
D=A\n\
@__vm_call__\n\
0;JMP\n";

	static const char *call_func_asm = 
"(__vm_call__)\n"
/* push retAddrLabel */
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
"@R14\n\
D=M\n\
@SP\n\
D=M-D\n\
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
"@R15\n\
A=M\n\
0;JMP\n\
"
;

#if 0
	static const char *asm_code = 
/* push retAddrLabel */
"\
@%s.ret.%" PRIu16 "\n\
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
@%" PRIu16 "\n\
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
"(%s.ret.%" PRIu16 ")\n"
;
#endif
	
	struct vm_cmd *cmd = vm->vm_cmds.data + vm->cur_cmd_i;
	if(cmd->vmline.token_num < 3) {
		hvm_error(vm,
		"the 'call' keyword should be followed by a function name \
and a positive number indicating the number of the function arguments");
		
		return 0;
	}
	
#if 0
	fprintf(vm->dest, asm_code,
	vm->base_name, vm->call_i,
	cur_cmd->arg_n, /* nArgs */
	cur_cmd->function, /* functionName */
	vm->base_name, vm->call_i
	);
	vm->call_i++;
#endif
	
#if 1
	fprintf(vm->dest, pre_call_func_asm,
	        cmd->function,
	        cmd->arg_n,
	        vm->cur_func.data, vm->call_i);
	
	if(!vm->has_call) {
		fputs(call_func_asm, vm->dest);
		vm->has_call = true;
	}
	fprintf(vm->dest, "(%s$ret%" PRIu16 ")\n",
	vm->cur_func.data, vm->call_i);
	vm->call_i++;
	
#endif	
	return 1;
}

int vm_cmd_translate_function(struct vm *vm) {
	
	struct vm_cmd *cmd = vm->vm_cmds.data + vm->cur_cmd_i;
	if(cmd->vmline.token_num < 3) {
		hvm_error(vm,
		"the 'function' keyword should be followed by a function name \
and a positive number indicating how many local variables to allocate for the function");

		return 0;
	}
	
	vm->call_i = 0;
	vm->gt_i = 0;
	vm->lt_i = 0;
	vm->eq_i = 0;
	
	fprintf(vm->dest, "(%s)\n",
	cmd->function);
	
	if(!n2tstr_strcpy_n(&vm->cur_func,
	   cmd->function,
	   cmd->vmline.tokens_indices[3] - cmd->vmline.tokens_indices[2] + 1)
	  ) { 
		hvm_func_error(vm, "out of memory");
		return 0;
	  }
	
	
	static const char *asm_push_0 = "\
M=0\n\
@SP\n\
AM=M+1\n\
";
	if(cmd->arg_n) {
		fputs("@SP\nA=M\n", vm->dest);
		for(uint16_t i = 0; i < cmd->arg_n; i++) {
			fputs(asm_push_0, vm->dest);
		}
	}
	
	return 1;
}

int vm_cmd_translate_return(struct vm *vm) {
	
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
		
		fputs("\
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
", vm->dest);

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

int _vm_remove_uncalled_functions(struct vm *vm) {
	
	//n2t_debug("_vm_remove_uncalled_functions()");
	
	int ret = 1;
	for( ; vm->cur_cmd_i < vm->vm_cmds.length; vm->cur_cmd_i++) {
		struct vm_cmd *cmd = vm->vm_cmds.data + vm->cur_cmd_i;
		uint16_t saved_cmd_i;
		if(cmd->type == vm_cmd_call) {
			saved_cmd_i = vm->cur_cmd_i;
			struct vm_func_t *called_func = NULL;
			//n2t_debug_f("called function: '%s'\n", cmd->function);
			HASH_FIND_STR(vm->functions, cmd->function, called_func);
			if(!called_func) {
				hvm_func_error(vm, "BUG!!");
				return 0;
			}
			if(!called_func->scanned) {
				vm->cur_cmd_i = called_func->func_cmd_i + 1;
				called_func->scanned = true;
				ret = _vm_remove_uncalled_functions(vm);
				vm->cur_cmd_i = saved_cmd_i;
			}
		}
		else if(cmd->type == vm_cmd_function) break;
	}
	return ret;
}

int vm_remove_uncalled_functions(struct vm *vm) {
	
	//n2t_debug_f("vm->noinit = %d\n", vm->noinit);
	if(vm->noinit) {
		vm->cur_cmd_i = 1;
		return _vm_remove_uncalled_functions(vm);
	}
	else {
		struct vm_func_t *init_func;
		//n2t_debug("searching for Sys.init()");
		//char *sys_init = "Sys.init";
		HASH_FIND_STR(vm->functions, "Sys.init", init_func);
		if(!init_func) {
			hvm_func_warn(vm, "Sys.init definition not found (?)");
			//return 0;
			vm->cur_cmd_i = 1;
			return _vm_remove_uncalled_functions(vm);
		}
		init_func->scanned = true;
		vm->cur_cmd_i = init_func->func_cmd_i+1;
		return _vm_remove_uncalled_functions(vm);
	}
}

int vm_analyze(struct vm *vm) {
	
	int ret = 1;
	char *line = NULL;
	size_t line_buf_len = 0;
	size_t line_len;
	struct vm_func_t *vm_func = NULL;
	bool func_has_return = false;
	
	struct vm_src_range range;
	range.start = vm->vm_cmds.length;
	
	vm->line_n = 0;
	while((line_len = n2t_getline(&line, &line_buf_len, vm->src)) != (size_t)-1) {
		
		vm->line_n++;
		//printf("- line %zu = '%s'\n", vm->line_n, line);
		
		struct vm_line vmline;
		//vm->line = &vmline;
		vmline.line = line;
		vmline.line_num = vm->line_n;
		vm_parse_line(&vmline);
		//printf("vmline.token_num  = %u\n", vmline.token_num);
		if(vmline.token_num == 0) continue;
		line[vmline.tokens_indices[1]] = '\0';
		if(vmline.token_num >= 2) {
			line[vmline.tokens_indices[3]] = '\0';
		}
		if(vmline.token_num >= 3) {
			line[vmline.tokens_indices[5]] = '\0';
		}
		
		const struct vm_cmd *cmd = vm_get_cmd(&line[vmline.tokens_indices[0]], 
		vmline.tokens_indices[1] - vmline.tokens_indices[0]);
		if(cmd) {
			if(vmline.token_num > cmd->token_num) {
				static const char *msg = "excess tokens after '%s' command";
				if(vm->strict) {
					hvm_error_f(vm, msg, cmd->name);
					ret = 0;
					n2t_free(line);
					goto CLEAN;
				}
				else {
					hvm_warn_f(vm, msg, cmd->name);
				}
			}
			
			intmax_t addr = 0;
			if(vmline.token_num == 3) {
				char *endptr = &line[vmline.tokens_indices[4]];
				errno = 0;
				addr = strtoimax(&line[vmline.tokens_indices[4]], &endptr, 10);
				if(!addr && endptr != &line[vmline.tokens_indices[5]]) {
					hvm_error_f(vm,
					"expected a numerical constant, got '%s'",
					&line[vmline.tokens_indices[4]]);
					n2t_free(line);
					ret = 0;
					goto CLEAN;
				}
				if(errno == ERANGE || addr > HACK_ADDRESS_MAX || addr < 0) {
					hvm_error_f(vm,
					"numerical constants should be in the range [0, %u]", HACK_ADDRESS_MAX);
					n2t_free(line);
					ret = 0;
					goto CLEAN;
				}
			}
			
			struct vm_cmd vmcmd;
			vmcmd.name = cmd->name;
			vmcmd.type = cmd->type;
			vmcmd.segment = &line[vmline.tokens_indices[2]];
			vmcmd.address = (uint16_t)addr;
			vmcmd.vmline = vmline;
			if(VEC_PUSH(vm->vm_cmds, vmcmd)) {
				hvm_func_error(vm, "out of memory");
				ret = 0;
				goto CLEAN;
			}
			
			if(cmd->type == vm_cmd_function) {
				vm_func = n2t_malloc(sizeof(struct vm_func_t));
				if(!vm_func) {
					hvm_func_error(vm, "out of memory");
					ret = 0;
					goto CLEAN;
				}
				vm_func->func = vmcmd;
				vm_func->func_cmd_i = vm->vm_cmds.length - 1;
				vm_func->scanned = false;
				vm_func->ret.name = NULL;
				func_has_return = false;
			}
			else if(cmd->type == vm_cmd_return) {
				if(!vm_func) {
					//return outside function ?
					hvm_error(vm, "return statement should be inside a function");
					ret = 0;
					goto CLEAN;
				}
				
				vm_func->ret = vmcmd;
				vm_func->ret_cmd_i = vm->vm_cmds.length - 1;
				func_has_return = true;
				//n2t_debug_f("searching for vm_func->func.function '%s'",
				//vm_func->func.function);
				struct vm_func_t *t;
				HASH_FIND_STR(vm->functions, vm_func->func.function, t);
				if(!t) {
					//n2t_debug_f("adding vm_func->func.function '%s'",
					//vm_func->func.function);
					HASH_ADD_KEYPTR(hh, vm->functions,
					vm_func->func.function,
					vm_func->func.vmline.tokens_indices[3] - vm_func->func.vmline.tokens_indices[2],
					vm_func);
					
				}
				//vm_func = NULL;
			}
			
			/* allocate a new buffer for each line */
			line = NULL;
			line_buf_len = 0;
		}
		else {
			static const char *msg = "unrecognized keyword '%s'";
			if(vm->strict) {
				hvm_error_f(vm, msg,
				&line[vmline.tokens_indices[0]]);
				n2t_free(line);
				ret = 0;
				goto CLEAN;
			}
			else {
				hvm_warn_f(vm, msg,
				&line[vmline.tokens_indices[0]]);
			}
		}
		errno = 0;
	}
	
	if(vm_func && !func_has_return) {
		// a function without return ?
		//hvm_error(vm, "function without return?");
		vm_func->ret = (struct vm_cmd){NULL,0,{NULL},{0},0,{{0}}};
		vm_func->ret_cmd_i = vm->vm_cmds.length - 1;
		HASH_ADD_KEYPTR(hh, vm->functions,
		vm_func->func.function,
		vm_func->func.vmline.tokens_indices[3] - vm_func->func.vmline.tokens_indices[2],
		vm_func);
		
	}
	vm_func = NULL;
	
	if(errno || ferror(vm->src)) {
		hvm_func_error(vm, "error while processing source file");
		ret = 0;
		goto CLEAN;
	}
	
	range.end = vm->vm_cmds.length - 1;
	if(VEC_PUSH(vm->src_ranges, range)) {
		hvm_func_error(vm, "out of memory");
		ret = 0;
		goto CLEAN;
	}
	
CLEAN:
	n2t_free(vm_func);
	return ret;
}

static inline void vm_write_annotation(struct vm *vm, struct vm_cmd *cmd) {
	
	if(vm->annotated) {
		fprintf(vm->dest, "// %s", cmd->name);
		if(cmd->token_num >= 2) {
			fprintf(vm->dest, " %s", cmd->function);
		}
		if(cmd->token_num >= 3) {
			fprintf(vm->dest, " %" PRIu16, cmd->address);
		}
		fputc('\n', vm->dest);
	}
}

int vm_translate(struct vm *vm) {
	
	int ret = 1;
	
	if(!vm->noinit) {
		vm_write_init(vm);
	}
	
	//n2t_debug("calling vm_remove_uncalled_functions():\n");
	ret = vm_remove_uncalled_functions(vm);
	if(!ret) return 0;
	
	/*struct vm_func_t *f;
    for(f=vm->functions; f != NULL; f=f->hh.next) {
		printf("f->func.function = '%s', f->scanned = %d\n",
		f->func.function, f->scanned);
    }*/
	
	for(vm->cur_src_i = 0; vm->cur_src_i < vm->src_ranges.length; vm->cur_src_i++) {
		for(vm->cur_cmd_i = vm->src_ranges.data[vm->cur_src_i].start;
			vm->cur_cmd_i <= vm->src_ranges.data[vm->cur_src_i].end;
			vm->cur_cmd_i++) {
			struct vm_cmd *cmd = vm->vm_cmds.data + vm->cur_cmd_i;
			//n2t_debug_f("cmd->name = '%s'\n", cmd->name);
			if(cmd->type == vm_cmd_function) {
				/* mark first encountered function as scanned */
				struct vm_func_t *called_func = NULL;
				//n2t_debug_f("cmd->function = '%s'\n", cmd->function);
				HASH_FIND_STR(vm->functions, cmd->function, called_func);
				if(!called_func) {
					hvm_func_error_f(vm, "BUG!! could not find function '%s' in table",
					cmd->function);
					return 0;
				}
				called_func->scanned = true;
				break;
			}
			else {
				vm_write_annotation(vm, cmd);
				ret = vm_cmd_functions[cmd->type](vm);
				if(!ret) return 0;
			}
		}
	}
	
	vm->cur_src_i = 0;
	struct vm_func_t *f;
    for(f=vm->functions; f != NULL; f=f->hh.next) {
		
		if(!f->scanned) continue;
		
		if(f->func_cmd_i > vm->src_ranges.data[vm->cur_src_i].end) {
			vm->cur_src_i ++;
		}
		
		for(vm->cur_cmd_i = f->func_cmd_i; vm->cur_cmd_i <= f->ret_cmd_i; vm->cur_cmd_i++) {
			struct vm_cmd *cur_cmd = vm->vm_cmds.data + vm->cur_cmd_i;
			vm->line_n = cur_cmd->vmline.line_num;
			vm_write_annotation(vm, cur_cmd);
			ret = vm_cmd_functions[cur_cmd->type](vm);
			if(!ret) return 0;
		}
    }
	
	return ret;
}

int vm_init(struct vm *vm) {
	memset(vm, 0, sizeof(struct vm));
	if(VEC_INIT(vm->vm_cmds)) {
		hvm_func_error(vm, "out of memory");
		return 0;
	}
	if(VEC_INIT(vm->src_ranges)) {
		VEC_FREE(vm->vm_cmds);
		hvm_func_error(vm, "out of memory");
		return 0;
	}
	if(!n2tstr_init(&vm->cur_func)) {
		VEC_FREE(vm->vm_cmds);
		VEC_FREE(vm->src_ranges);
		hvm_func_error(vm, "out of memory");
		return 0;
	}
	vm->cur_func.data[0] = '\0';
	
	return 1;
}

void vm_free(struct vm *vm) {
	
	for(size_t i = 0; i < vm->vm_cmds.length; i++) {
		n2t_free(vm->vm_cmds.data[i].vmline.line);
	}
	VEC_FREE(vm->vm_cmds);
	VEC_FREE(vm->src_ranges);
	n2tstr_free(&vm->cur_func);
	
	struct vm_func_t *f, *t;
	HASH_ITER(hh, vm->functions, f, t) {
		HASH_DEL(vm->functions, f);
		n2t_free(f);
	}
	
}

