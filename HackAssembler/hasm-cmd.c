#include <stdlib.h>
#include <stdio.h>

#include "n2t-common.h"
#include "hasm.h"

static const char *usage_str = "\
usage:\n\
%s [source_file] [options]\n\
- options:\n\
    -h / --help : show this message\n\
    -s / --strict : treat warnings as errors\n\
    -q / --quiet : suppress most assembler messages\n\
    -o [destination_file] : output assembled code to destination_file\n"
;

#define usage(path) fprintf(stderr, usage_str, path)

void hasm_parse_argv(struct hasm *hasm, int argc, char **argv) {
	
	char *prog_path = *argv;
	if(argc < 2) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	argv++;
	while(*argv) {
		
		if(!strcmp(*argv, "-h") || !strcmp(*argv, "--help")) {
			usage(prog_path);
			exit(0);
		}
		else if(!strcmp(*argv, "-s") || !strcmp(*argv, "--strict")) {
			hasm->strict = true;
		}
		else if(!strcmp(*argv, "-q") || !strcmp(*argv, "--quiet")) {
			hasm->quiet = true;
		}
		else if(!strcmp(*argv, "-o") || !strcmp(*argv, "--output")) {
			char *cmd = *argv;
			argv++;
			if(!(*argv)) {
				n2t_func_error_f("expected destination path after '%s' command line option",
				cmd);
				exit(EXIT_FAILURE);
			}
			hasm->dest_path = *argv;
		}
		else {
			if(!hasm->src_path) {
				hasm->src_path = *argv;
			}
			else {
				n2t_func_error_f("unrecognized command line option '%s'", *argv);
				exit(EXIT_FAILURE);
			}
		}
		argv++;
	}
	if(!hasm->src_path) {
		n2t_func_error("no source file was specified");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv) {
	
	struct hasm hasm;
	hasm_init(&hasm);
	hasm_parse_argv(&hasm, argc, argv);
	
	size_t src_len = strlen(hasm.src_path);
	if(src_len <= 4 || strcmp(hasm.src_path + src_len - 4, ".asm")) {
		hasm_func_error(&hasm, "source file should have the extension 'asm'");
		return EXIT_FAILURE;
	}
	
	hasm.src = fopen(hasm.src_path, "r");
	if(!hasm.src) {
		hasm_func_error_f(&hasm, "failed to open source file %s", hasm.src_path);
		return EXIT_FAILURE;
	}
	
	if(!hasm.dest_path) {
		hasm.dest_path = n2t_make_filename_with_ext(hasm.src_path, "hack");
		if(!hasm.dest_path) {
			hasm_func_error(&hasm, "out of memory");
			return EXIT_FAILURE;
		}
	}
	
	if(!strcmp(hasm.src_path, hasm.dest_path)) {
		hasm_func_error_f(&hasm, "input file '%s' is the same as output file", hasm.src_path);
		return EXIT_FAILURE;
	}
	
	hasm.dest = fopen(hasm.dest_path, "w");
	if(!hasm.dest) {
		hasm_func_error(&hasm, "failed to open output file");
		return EXIT_FAILURE;
	}
	
	int ret = 0;
	hasm_state status = hasm_assemble(&hasm);
	fclose(hasm.src);
	fclose(hasm.dest);
	if(status) {
		remove(hasm.dest_path);
		ret = EXIT_FAILURE;
	}
	
	return ret;
}
