
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "n2t-common.h"
#include "jack-compiler.h"


static const char *usg_str = "\
jc [source file] [options]\n\
jc [source directory] [options]\n\
- options:\n\
    -o [destination path] : choose a destination path\n\
	   (valid only when source is a single file)\n";

#define usage() fputs(usg_str, stderr)


struct jc_cmd {
	char *src;
	char *dest;
};

static inline void jc_cmd_parse_argv(struct jc_cmd *jcc, int argc, char **argv) {
	
	if(argc < 2) {
		usage();
		exit(EXIT_FAILURE);
	}
	jcc->src = jcc->dest = NULL;
	argv++;
	while(*argv) {
		if(!strcmp(*argv, "-o")) {
			argv++;
			if(*argv) {
				jcc->dest = *argv;
			}
			else {
				usage();
				fprintf(stderr, "expected destination file after -o command line switch");
				exit(EXIT_FAILURE);
			}
		}
		else {
			jcc->src = *argv;
		}
		argv++;
	}
	if(!jcc->src) {
		usage();
		fprintf(stderr, "no source path was specified");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv) {
	
	struct jc_cmd jcc;
	jc_cmd_parse_argv(&jcc, argc, argv);
	
	struct j_compiler jc;
	jc_init(&jc);
	
	n2t_path_type ptype = n2t_get_path_type(jcc.src);
	if(ptype == n2t_path_file) {
		if(!jcc.dest) {
			#ifdef JC_XML
			jcc.dest = n2t_make_filename_with_ext(jcc.src, "xml");
			#else
			jcc.dest = n2t_make_filename_with_ext(jcc.src, "vm");
			#endif
			if(!jcc.dest) {
				return EXIT_FAILURE;
			}
		}
		jc.tokenizer.src_name = jcc.src;
		jc.tokenizer.src = fopen(jc.tokenizer.src_name, "rb");
		if(!jc.tokenizer.src) {
			n2t_func_error("could not open source file");
			return EXIT_FAILURE;
		}
		jc.dest = fopen(jcc.dest, "w");
		if(!jc.dest) {
			n2t_func_error("could not open destination file");
			return EXIT_FAILURE;
		}
		return !jc_compile_class(&jc);
	}
	else if(ptype == n2t_path_dir) {
		
		struct n2t_dir_search dir_srch;
		char *fpath = n2t_dir_get_first_file(&dir_srch,
					  jcc.src, strlen(jcc.src),
					  "jack");
		do {
			jc.tokenizer.src_name = fpath;
			jc.tokenizer.src = fopen(jc.tokenizer.src_name, "rb");
			if(!jc.tokenizer.src) {
				n2t_func_error_f("could not open source file %s", jc.tokenizer.src_name);
				return EXIT_FAILURE;
			}
			
			//size_t i;
			//for(i = dir_srch.fpath_len; fpath[i] != '.'; i--) {	}
			//printf("i = %llu\n", i);
			char *c = strstr(jc.tokenizer.src_name, "jack");
			size_t i = c  - jc.tokenizer.src_name;
			#ifdef JC_XML
			strcpy(&jc.tokenizer.src_name[i], "xml");
			#else
			strcpy(&jc.tokenizer.src_name[i], "vm");
			#endif
			//printf("jc.tokenizer.src_name = %s\n", jc.tokenizer.src_name);
			
			jcc.dest = jc.tokenizer.src_name;
			jc.dest = fopen(jcc.dest, "w");
			if(!jc.dest) {
				n2t_func_error("could not open destination file");
			}
			if(!jc_compile_class(&jc)) return EXIT_FAILURE;
			fpath = n2t_dir_get_next_file(&dir_srch);
			
		} while(fpath);
	}
	else {
		n2t_func_error_f("could not find source path %s", jcc.src);
		return EXIT_FAILURE;
	}
	
	return 0;
}
