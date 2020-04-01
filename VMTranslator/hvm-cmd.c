#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hvm.h"


static const char *usage_str = "\
usage:\n\
- %s [source file] [options]\n\
- %s [directory] [options]\n\
- options:\n\
    -h : show this message\n\
    -o [destination file] : specify output file\n\
    -a : annotate generated code with comments\n\
    -n : do not output bootstrap code\n\
    -q : suppress console output\n\
    -s: treat some warnings as errors\n";

#define usage(path) fprintf(stderr, usage_str, path, path)

static char *user_path;

static void vm_parse_argv(struct vm *vm, int argc, char **argv) {
	
	char *path = *argv;
	if(argc < 2) {
		usage(path);
		exit(EXIT_FAILURE);
	}
	argv++;
	
	while(*argv) {
		
		if(!strcmp(*argv, "-h")) {
			usage(path);
			exit(0);
		}
		else if(!strcmp(*argv, "-o")) {
			argv++;
			if(*argv) vm->dest_path = *argv;
			else {
				n2t_func_error("expected destination file name after '-o'");
				exit(EXIT_FAILURE);
			}
		}
		else if(!strcmp(*argv, "-a")) {
			vm->annotated = true;
		}
		else if(!strcmp(*argv, "-n")) {
			vm->noinit = true;
		}
		else if(!strcmp(*argv, "-q")) {
			vm->quiet = true;
		}
		else if(!strcmp(*argv, "-s")) {
			vm->strict = true;
		}
		else {
			user_path = *argv;
		}
		argv++;
	}
	if(!user_path) {
		n2t_func_error("No source file or directory was provided");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv) {
	
#ifdef N2T_DEBUG	
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
#endif
	
	struct vm vm;
	vm_init(&vm);
	vm_parse_argv(&vm, argc, argv);
	int ret = 0;

	struct n2t_path_info pinfo;
	n2t_path_type ptype = n2t_get_path_type(user_path);
	if(ptype != n2t_path_dir && ptype != n2t_path_file) {
		hvm_func_error_f(&vm, "could not find source path '%s'", user_path);
		return EXIT_FAILURE;
	}
	n2t_parse_path(&pinfo, user_path);
	
	if(ptype == n2t_path_file) {
		if(strcmp(user_path + pinfo.ext_pos, ".vm")) {
			hvm_func_error(&vm, "input file should have the extention 'vm'");
			return EXIT_FAILURE;
		}
	}
	
	if(!vm.dest_path) {
		/*
		if(ptype == n2t_path_file) {
			vm.dest_path = n2t_make_filename_with_ext(user_path, "asm");
			if(!vm.dest_path) {
				hvm_func_error(&vm, "failed to make output file path");
				return EXIT_FAILURE;
			}
		}
		else if(ptype == n2t_path_dir) {
			vm.dest_path = n2t_malloc(pinfo.base_name_len + 4 + 1);
			if(!vm.dest_path) {
				hvm_func_error(&vm, "failed to make output file path");
				return EXIT_FAILURE;
			}
			strncpy(vm.dest_path, user_path + pinfo.base_name_pos, pinfo.base_name_len);
			strcpy(vm.dest_path + pinfo.base_name_len, ".asm");
		}
		*/
		if(ptype == n2t_path_dir || ptype == n2t_path_file) {
			vm.dest_path = n2t_malloc(pinfo.base_name_len + 4 + 1);
			if(!vm.dest_path) {
				hvm_func_error(&vm, "failed to make output file path");
				return EXIT_FAILURE;
			}
			strncpy(vm.dest_path, user_path + pinfo.base_name_pos, pinfo.base_name_len);
			strcpy(vm.dest_path + pinfo.base_name_len, ".asm");
		}
	}
	
	if(ptype == n2t_path_file) {
		if(!strcmp(user_path, vm.dest_path)) {
			hvm_func_error_f(&vm, "input file '%s' is the same as output file", user_path);
			return EXIT_FAILURE;
		}
	}
	
	vm.dest = fopen(vm.dest_path, "w");
	if(!vm.dest) {
		hvm_func_error_f(&vm, "failed to open output file '%s'",
		vm.dest_path);
		return EXIT_FAILURE;
	}
	
	vm.base_name = n2t_strdup_n(&user_path[pinfo.base_name_pos], pinfo.base_name_len);
	if(!vm.base_name) {
		hvm_func_error(&vm, "out of memory");
		ret = EXIT_FAILURE;
		goto CLEAN;
	}
	
	if(ptype == n2t_path_file) {
		
		vm.src_path = user_path;
		vm.src = fopen(vm.src_path, "r");
		if(!vm.src) {
			hvm_func_error_f(&vm, "failed to open source file '%s'",
			vm.src_path);
			ret = EXIT_FAILURE;
			goto CLEAN;
		}
		
		if(vm_analyze(&vm)) {
			if(!vm_translate(&vm)) ret = EXIT_FAILURE;
		}
		else ret = EXIT_FAILURE;
		fclose(vm.src);
	}
	else if(ptype == n2t_path_dir) {
		
		struct n2t_dir_search dir_srch;
		vm.src_path = n2t_dir_get_first_file(&dir_srch,
					  user_path, pinfo.path_len,
					  "vm");
		if(dir_srch.state == n2t_dir_no_match) {
			hvm_func_error_f(&vm, "no vm source files were found in the directory '%s'",
			user_path);
			ret = EXIT_FAILURE;
			goto CLEAN;
		}
		else if(dir_srch.state == n2t_dir_oom) {
			hvm_func_error(&vm, "out of memory");
			ret = EXIT_FAILURE;
			goto CLEAN;
		}
		else if(dir_srch.state == n2t_dir_error) {
			hvm_func_error(&vm, "error while traversing source directory");
			ret = EXIT_FAILURE;
			goto CLEAN;
		}
		else if(dir_srch.state == n2t_dir_open_failed) {
			hvm_func_error_f(&vm, "failed to open source directory '%s'",
			user_path);
			ret = EXIT_FAILURE;
			goto CLEAN;
		}
		
		do {
			vm.src = fopen(vm.src_path, "r");
			if(!vm.src) {
				hvm_func_error_f(&vm, "failed to open source file '%s'", vm.src_path);
				ret = EXIT_FAILURE;
				goto CLEAN;
			}
			
			//n2t_debug_f("vm.base_name = '%s'", vm.base_name);
			//n2t_debug_f("vm.src_path = '%s'", vm.src_path);
			
			if(!vm_analyze(&vm)) {
				ret = EXIT_FAILURE;
				goto CLEAN;
			}
			
			vm.src_path = n2t_dir_get_next_file(&dir_srch);
			if(dir_srch.state == n2t_dir_oom) {
				hvm_func_error(&vm, "out of memory");
				ret = EXIT_FAILURE;
				goto CLEAN;
			}
			else if(dir_srch.state == n2t_dir_error) {
				hvm_func_error(&vm, "error while traversing source directory");
				ret = EXIT_FAILURE;
				goto CLEAN;
			}
			fclose(vm.src);
		} while(dir_srch.state != n2t_dir_no_more);
		
		if(!vm_translate(&vm)) {
			ret = EXIT_FAILURE;
			goto CLEAN;
		}
	}
	
CLEAN:
	fclose(vm.dest);
	if(ret) {
		remove(vm.dest_path);
	}
	return ret;
}
