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
    -q : suppress console output\n";

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
	
	if(ptype == n2t_path_file) {
		vm.base_name = n2t_strdup_n(&user_path[pinfo.base_name_pos], pinfo.base_name_len);
		if(!vm.base_name) {
			hvm_func_error(&vm, "out of memory");
			ret = EXIT_FAILURE;
			goto CLEAN;
		}
		vm.src_path = user_path;
		vm.src = fopen(vm.src_path, "r");
		if(!vm.src) {
			hvm_func_error_f(&vm, "failed to open source file '%s'",
			vm.src_path);
			ret = EXIT_FAILURE;
			goto CLEAN;
		}
		ret = !vm_translate(&vm);
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
		
		
		struct n2t_path_info src_pinfo;
		n2t_parse_path(&src_pinfo, vm.src_path + pinfo.path_len + 1);
		n2tstr nstr;
		if(!n2tstr_init_n(&nstr, pinfo.path_len + 1 + src_pinfo.base_name_len + 1)) {
			hvm_func_error(&vm, "out of memory");
			ret = EXIT_FAILURE;
			goto CLEAN;
		}
		if(!n2tstr_strcpy_n(&nstr, vm.src_path + pinfo.path_len + 1, src_pinfo.base_name_len)) {
			hvm_func_error(&vm, "unknown error");
			ret = EXIT_FAILURE;
			goto CLEAN;
		}
		nstr.data[src_pinfo.base_name_len] = '\0';
		vm.base_name = nstr.data;
		
		do {
			vm.src = fopen(vm.src_path, "r");
			if(!vm.src) {
				hvm_func_error_f(&vm, "failed to open source file '%s'", vm.src_path);
				ret = EXIT_FAILURE;
				goto CLEAN;
			}
			
			if(!vm_translate(&vm)) {
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
			else if(dir_srch.state == n2t_dir_success) {
				n2t_parse_path(&src_pinfo, vm.src_path + pinfo.path_len + 1);
				if(!n2tstr_strcpy_n(&nstr, vm.src_path + pinfo.path_len + 1, src_pinfo.base_name_len+1)) {
					hvm_func_error(&vm, "out of memory");
					ret = EXIT_FAILURE;
					goto CLEAN;
				}
				nstr.data[src_pinfo.base_name_len] = '\0';
				//printf("src_pinfo.base_name_len = %zu, nstr.data = '%s'\n",
				//src_pinfo.base_name_len, nstr.data);
			}
			fclose(vm.src);
		} while(dir_srch.state != n2t_dir_no_more);
	}
	
CLEAN:
	if(ret) {
		remove(vm.dest_path);
	}
	return ret;
}
