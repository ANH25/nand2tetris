#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "n2t-common.h"
#include "hasm.h"


void hasm_test_write_src(struct hasm *hasm) {
	
	static const char *A_instructions = "\
	@999999999999999999\n\
	@-999999999999999999999\n\
	@-23\n\
	@%u\n\
	@%u\n\
	@0\n\
	@23GARBAGE\n\
	@23GARBAGE\n";
	
	static const char *C_instructions = "\
	D=;JMP\n\
	D=A;JNE//comment\n\
	D=M;JUNK\n\
	D=M;JUNK\n\
	A=\n\
	D;\n\
	D\n\
	AD=M\n\
	A=M;\n\
	D;JEQ\n";
	
	static const char *labels = "\
	(mismatched\n\
	@DUMMY\n\
	(mismatched_strict\n\
	(343invalid_label)\n\
	(redefined_label)\n\
	(redefined_label)\n\
	@DUMMY\n\
	@DUMMY\n\
	(redefined_label_strict)\n\
	(redefined_label_strict)\n\
	(mismatched_strict_before_comment//\n\
	(SP)\n\
	@DUMMY\n\
	(SP)//redefines a predefined symbol when hasm->strict\n\
	@DUMMY\n";
	
	fpos_t pos;
	fgetpos(hasm->src, &pos);
	fprintf(hasm->src, A_instructions, HACK_ADDRESS_MAX+1, HACK_ADDRESS_MAX);
	fputs(C_instructions, hasm->src);
	fputs(labels, hasm->src);
	fsetpos(hasm->src, &pos);
}

void hasm_test_A_instruction(struct hasm *hasm) {
	
	hasm_state ret;

	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_invalid_address);
	
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_invalid_address);
	
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_invalid_address);
	
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_invalid_address);
	
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_success);
	
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_success);
	
	ret = hasm_assemble_line(hasm);
	// should translate '@23GARBAGE' correctly (with a warning)
	assert(ret == hasm_success);
	
	hasm->strict = true;
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_invalid_address);
	hasm->strict = false;
	
}

void hasm_test_C_instruction(struct hasm *hasm) {
	
	hasm_state ret;

	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_syntax_error);
	
	hasm->strict = true;
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_success);
	
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_syntax_error);

	hasm->strict = false;
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_success);
	
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_syntax_error);
	
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_success);
	
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_success);
	
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_success);
	
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_success);
	
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_success);
	
}

void hasm_test_label(struct hasm *hasm) {
	
	hasm_state ret;

	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_success);
	
	hasm->strict = true;
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_syntax_error);
	hasm->strict = false;
	
	
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_invalid_label);
	
	ret = hasm_assemble_n(hasm, 2);
	assert(ret == hasm_success);
	
	hasm->strict = true;
	ret = hasm_assemble_n(hasm, 2);
	assert(ret == hasm_redefined_label);
	hasm->strict = false;
	
	hasm->strict = true;
	ret = hasm_assemble_n(hasm, 2);
	assert(ret == hasm_syntax_error);
	hasm->strict = false;
	
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_success);
	
	hasm->strict = true;
	ret = hasm_assemble_line(hasm);
	assert(ret == hasm_redefined_label);
	hasm->strict = false;
	
	
}	

void hasm_test(struct hasm *hasm) {
	
	hasm_test_write_src(hasm);
	hasm->strict = false;
	hasm->quiet = true;
	hasm_test_A_instruction(hasm);
	hasm_test_C_instruction(hasm);
	hasm_test_label(hasm);
	
}



int main(void) {
	
	
	struct hasm hasm;
	hasm_init(&hasm);
	hasm.src_path = "hasm_test.asm";
	hasm.src = fopen(hasm.src_path, "w+");
	if(!hasm.src) {
		n2t_func_error_f("Failed to open source file %s", hasm.src_path);
	}
	hasm.dest = fopen("hasm_test.hack", "w");
	if(!hasm.dest) {
		n2t_func_error("Failed to create destination file");
	}
	
	hasm_test(&hasm);
	fclose(hasm.src);
	fclose(hasm.dest);
	hasm_free(&hasm);
	
	return 0;
}
