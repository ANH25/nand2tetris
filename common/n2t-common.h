#if !defined(N2T_COMMON_H)
#define N2T_COMMON_H

/* NOTES:
- Though this code and things like the assembler and VM translator try to return error codes
  on out-of-memory conditions instead of aborting, the currently used hash table library (uthash)
  aborts on oom conditions and can't be modified to behave otherwise easily in all scenarios
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* The maximum addressable memory register in the Hack platform */
#define HACK_ADDRESS_MAX 32767U


#ifdef N2T_DEBUG

#define n2t_func_report(type, msg) \
	fprintf(stderr, "%s:%s:%u: " type ": %s\n", __FILE__, __func__, __LINE__, msg)

#define n2t_func_report_f(type, fmt, ...) ( \
	fprintf(stderr, "%s:%s:%u: " type ": ", __FILE__, __func__, __LINE__), \
	fprintf(stderr, fmt, __VA_ARGS__), \
	putc('\n', stderr) \
)

#else

#define n2t_func_report(type, msg) fprintf(stderr, type ": %s\n", msg)

#define n2t_func_report_f(type, fmt, ...) ( \
	fputs(stderr, type ": "), \
	fprintf(stderr, fmt, __VA_ARGS__), \
	putc('\n', stderr) \
)

#endif

#define n2t_func_error(msg) n2t_func_report("error", msg)
#define n2t_func_error_f(fmt, ...) n2t_func_report_f("error", fmt, __VA_ARGS__)
#define n2t_func_warn(msg) n2t_func_report("warning", msg)
#define n2t_func_warn_f(fmt, ...) n2t_func_report_f("warning", fmt, __VA_ARGS__)



#ifdef N2T_DEBUG

#define n2t_report(type, file, line, msg) ( \
	fprintf(stderr, "%s:%s:%u: ", __FILE__, __func__, __LINE__), \
	fprintf(stderr, "%s:%zu: " type ": %s\n", file, line, msg) \
)

#define n2t_report_f(type, file, line, fmt, ...) ( \
	fprintf(stderr, "%s:%s:%u: ", __FILE__, __func__, __LINE__), \
	fprintf(stderr, "%s:%zu: " type ": ", file, line), \
	fprintf(stderr, fmt, __VA_ARGS__), \
	putc('\n', stderr) \
)

#else

#define n2t_report(type, file, line, msg) ( \
	fprintf(stderr, "%s:%zu: " type ": %s\n", file, line, msg) \
)

#define n2t_report_f(type, file, line, fmt, ...) ( \
	fprintf(stderr, "%s:%zu: " type ": ", file, line), \
	fprintf(stderr, fmt, __VA_ARGS__), \
	putc('\n', stderr) \
)

#endif

#define n2t_error(file, line, msg) n2t_report("error", (file), (line), msg)
#define n2t_error_f(file, line, fmt, ...) n2t_report_f("error", (file), (line), fmt, __VA_ARGS__)
#define n2t_warn(file, line, msg) n2t_report("warning", (file), (line), msg)
#define n2t_warn_f(file, line, fmt, ...) n2t_report_f("warning", (file), (line), fmt, __VA_ARGS__)


#ifdef N2T_DEBUG

#define n2t_debug(msg) \
	fprintf(stderr, "%s:%s:%u: %s\n", __FILE__, __func__, __LINE__, msg)

#define n2t_debug_f(fmt, ...) ( \
	fprintf(stderr, "%s:%s:%u: ", __FILE__, __func__, __LINE__), \
	fprintf(stderr, fmt, __VA_ARGS__), \
	putc('\n', stderr) \
)

#else
#define n2t_debug(msg)
#define n2t_debug_f(fmt, ...)
#endif


#ifndef n2t_malloc
#define n2t_malloc(size) malloc(size)
#endif

#ifndef n2t_realloc
#define n2t_realloc(p, size) realloc(p, size)
#endif

#ifndef n2t_free
#define n2t_free(p) free(p)
#endif

#define vec_malloc(size) n2t_malloc(size)
#define vec_realloc(p, size) n2t_realloc(p, size)
#define vec_free(p) n2t_free(p)
#define uthash_malloc(size) n2t_malloc(size)
#define uthash_free(p, size) n2t_free(p)

#include "vector.h"


static inline char* n2t_strdup_n(const char *str, size_t n) {
	
	char *dup = n2t_malloc(n+1);
	if(!dup) {
		return NULL;
	}
	strncpy(dup, str, n);
	dup[n] = '\0';
	//memcpy(dup, str, n);
	//dup[n] = '\0';
	return dup;
}

static inline char* n2t_strdup(const char *str) {
	return n2t_strdup_n(str, strlen(str));
}

VEC_DEF(n2tstr, char);

static inline int n2tstr_init_n(n2tstr *nstr, size_t n) {
	
	return !VEC_INIT_N(*nstr, n);
}

static inline int n2tstr_init(n2tstr *nstr) {
	
	return !VEC_INIT(*nstr);
}

static inline void n2tstr_free(n2tstr *nstr) {
	
	VEC_FREE(*nstr);
}

static inline char* n2tstr_str_insert_n(n2tstr *nstr, size_t i,
										const char *cstr, size_t n) {
	if(nstr->capacity < n+i) {
		if(VEC_RESIZE(*nstr, n+i)) return NULL;
	}
	memcpy(nstr->data + i, cstr, n);
	return nstr->data;
}

static inline char* n2tstr_strcpy_n(n2tstr *nstr, const char *cstr, size_t n) {
	return n2tstr_str_insert_n(nstr, 0, cstr, n);
}

static inline char* n2tstr_strcpy(n2tstr *nstr, const char *cstr) {	
	return n2tstr_strcpy_n(nstr, cstr, strlen(cstr) + 1);
}

/* strip everything except the file name from 'path' and append .'ext' to it and return
   a new malloced string
*/
char* n2t_make_filename_with_ext(const char *path, const char *ext);


// todo: make dir state a member of n2t_dir_search
typedef enum n2t_dir_state {
	n2t_dir_success = 0,
	n2t_dir_error,
	n2t_dir_no_match,
	n2t_dir_oom,
	n2t_dir_open_failed,
	n2t_dir_no_more
} n2t_dir_state;

#ifdef _WIN32
#include <Windows.h>
#include <Shlwapi.h>

struct n2t_dir_search {
	HANDLE search_handle;
	WIN32_FIND_DATAA *file_handle;
	size_t dir_len;
	char *fpath;
	//size_t fpath_len;
	n2t_dir_state state;
};

#else // assume POSIX
#include <sys/types.h>
#include <dirent.h>

struct n2t_dir_search {
	DIR *dp;
	struct dirent *ep;
	size_t dir_len;
	char *fpath;
	char *dot_ext;
	n2t_dir_state state;
};
#endif


char* n2t_dir_get_first_file(struct n2t_dir_search *dir_srch,
                         const char *dir, size_t dir_len,
						 const char *ext);

char* n2t_dir_get_next_file(struct n2t_dir_search *dir_srch);

struct n2t_path_info {
	size_t path_len;
	size_t base_name_pos;
	size_t base_name_len;
	size_t ext_pos;
	size_t ext_len;
};

void n2t_parse_path(struct n2t_path_info *pinfo, const char *path);

typedef enum n2t_path_type {
	n2t_path_dir,
	n2t_path_file,
	n2t_path_error,
	n2t_path_not_found
} n2t_path_type;

enum n2t_path_type n2t_get_path_type(const char *path);

size_t n2t_getdelim(char **lineptr, size_t *n, int delim, FILE *stream);

size_t n2t_getline(char **lineptr, size_t *n, FILE *stream);

#endif
