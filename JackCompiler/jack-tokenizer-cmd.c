/* takes jack source file, tokenizes it, and outputs its tokens in xml format */

#include <stdio.h>
#include <stdlib.h>

#include "jack-tokenizer.h"

#define usage(cmd) fprintf(stderr, "Usage:\n %s [source file]\n", cmd)

int main(int argc, char **argv) {
	
	setbuf(stdout, NULL);
	
	if(argc < 2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	
	struct j_tokenizer tokenizer;
	j_tokenizer_init(&tokenizer);
	tokenizer.src_name = argv[1];
	tokenizer.src = fopen(tokenizer.src_name, "rb");
	if(!tokenizer.src) {
		fprintf(stderr, "error: failed to open source file\n");
		return EXIT_FAILURE;
	}
	
	puts("<tokens>");
	j_tokenizer_advance(&tokenizer);
	while(tokenizer.has_more_tokens) {
		j_tokenizer_write_xml_el(&tokenizer, stdout);
		j_tokenizer_advance(&tokenizer);
	}
	puts("</tokens>");
	
	return 0;
}
