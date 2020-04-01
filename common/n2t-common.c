#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "n2t-common.h"

/*
char* n2t_make_filename_with_ext(const char *path, const char *ext) {
	
	size_t path_len;
	size_t path_delim_pos = -1;
	size_t file_ext_pos = 0;
	for(path_len = 0; path[path_len]; path_len++) {
		if(path[path_len] == '\\' || path[path_len] == '/') {
			path_delim_pos = path_len;
		}
		else if(path[path_len] == '.') {
			file_ext_pos = path_len;
		}
	}
	size_t ext_len = strlen(ext);
	size_t new_path_len;
	if(!file_ext_pos) file_ext_pos = path_len;
	if(path_delim_pos != (size_t)-1) {
		new_path_len = file_ext_pos - path_delim_pos + ext_len;
	}
	else {
		new_path_len = file_ext_pos + 1 + ext_len;
	}
	
	char *new_path = n2t_malloc(new_path_len + 1);
	if(!new_path) {
		n2t_func_error_f("failed to allocate memory for new path with extension '%s'",
		ext);
		return NULL;
	}

	strncpy(new_path, 
	       &path[path_delim_pos + 1], 
	       file_ext_pos - (path_delim_pos + 1));
	new_path[new_path_len - ext_len - 1] = '.';
	strcpy(&new_path[new_path_len - ext_len], ext);
	
	return new_path;
}

*/

char* n2t_make_filename_with_ext(const char *path, const char *ext) {
	
	struct n2t_path_info pinfo;
	n2t_parse_path(&pinfo, path);
	size_t extless_path_len = pinfo.path_len - (pinfo.path_len - pinfo.ext_pos);
	char *new_path = n2t_malloc(extless_path_len + 1 + strlen(ext) + 1);
	if(!new_path) {
		n2t_debug("failed to allocate memory for new path");
		return NULL;
	}
	memcpy(new_path, path, extless_path_len);
	new_path[extless_path_len] = '.';
	strcpy(&new_path[extless_path_len+1], ext);
	return new_path;
}

#ifdef _WIN32

char* n2t_dir_get_first_file(struct n2t_dir_search *dir_srch,
                         const char *dir, size_t dir_len,
						 const char *ext) {
	
	dir_srch->dir_len = dir_len;
	char *glob = n2t_malloc(dir_len + 3 + strlen(ext) + 1);
	if(!glob) {
		n2t_debug("out of memory");
		dir_srch->state = n2t_dir_oom;
		return NULL;
	}
	strcpy(glob, dir);
	strcpy(&glob[dir_len], "\\*.");
	strcpy(&glob[dir_len+3], ext);
	
	WIN32_FIND_DATAA *ffd = n2t_malloc(sizeof(WIN32_FIND_DATAA));
	if(!ffd) {
		n2t_free(glob);
		n2t_debug("out of memory");
		dir_srch->state = n2t_dir_oom;
		return NULL;
	}

	HANDLE hfind = FindFirstFileA(glob, ffd);
	if(hfind == INVALID_HANDLE_VALUE) {
		DWORD error_code = GetLastError();
		n2t_free(glob);
		n2t_free(ffd);
		if(error_code != ERROR_FILE_NOT_FOUND) {
			n2t_debug("Error while traversing source directory");
			dir_srch->state = n2t_dir_error;
			return NULL;
		}
		else {
			n2t_debug_f("no files with the extension '%s' were found in the directory '%s'",
			ext, dir);
			dir_srch->state = n2t_dir_no_match;
			return NULL;
		}
	}
	dir_srch->search_handle = hfind;
	dir_srch->file_handle = ffd;
	n2t_free(glob);
	//dir_srch->fpath_len = MAX_PATH;
	dir_srch->fpath = n2t_malloc(dir_len + 1 + MAX_PATH + 1);
	if(!dir_srch->fpath) {
		n2t_free(ffd);
		n2t_debug("failed to allocate memory for file path");
		dir_srch->state = n2t_dir_oom;
		return NULL;
	}
	strcpy(dir_srch->fpath, dir);
	dir_srch->fpath[dir_len] = '\\';
	strcpy(&dir_srch->fpath[dir_len+1], dir_srch->file_handle->cFileName);
	dir_srch->state = n2t_dir_success;
	return dir_srch->fpath;
}

char* n2t_dir_get_next_file(struct n2t_dir_search *dir_srch) {
		
	if(FindNextFile(dir_srch->search_handle, dir_srch->file_handle)) {
		strcpy(&dir_srch->fpath[dir_srch->dir_len+1], dir_srch->file_handle->cFileName);
		dir_srch->state = n2t_dir_success;
		return dir_srch->fpath;
	}
	else {
		DWORD error_code = GetLastError();
		if(error_code == ERROR_NO_MORE_FILES) {
			FindClose(dir_srch->search_handle);
			n2t_free(dir_srch->file_handle);
			n2t_free(dir_srch->fpath);
			dir_srch->state = n2t_dir_no_more;
			return NULL;
		}
		else if(error_code != ERROR_SUCCESS) {
			FindClose(dir_srch->search_handle);
			n2t_debug("error while traversing source directory");
			dir_srch->state = n2t_dir_error;
			return NULL;
		}
	}
	return NULL;
}

#else

char* n2t_dir_get_first_file(struct n2t_dir_search *dir_srch,
                         const char *dir, size_t dir_len,
						 const char *ext) {
	
	dir_srch->dir_len = dir_len;
	dir_srch->dot_ext = n2t_malloc(strlen(ext) + 2);
	if(!dir_srch->dot_ext) {
		n2t_debug("out of memory");
		dir_srch->state = n2t_dir_oom;
		return NULL;
	}
	dir_srch->dot_ext[0] = '.';
	strcpy(dir_srch->dot_ext+1, ext);
	
	dir_srch->fpath = n2t_malloc(dir_len + NAME_MAX + 1);
	if(!dir_srch->fpath) {
		n2t_free(dir_srch->dot_ext);
		n2t_debug("failed to allocate memory for file path");
		dir_srch->state = n2t_dir_oom;
		return NULL;
	}
	strcpy(dir_srch->fpath, dir);
	dir_srch->fpath[dir_len] = '/';
	
	dir_srch->dp = opendir(dir);
	if(dir_srch->dp) {
		while((dir_srch->ep = readdir(dir_srch->dp))) {
			char *c = strstr(dir_srch->ep->d_name, dir_srch->dot_ext);
			if(c) {
				strcpy(&dir_srch->fpath[dir_len+1], dir_srch->ep->d_name);
				dir_srch->state = n2t_dir_success;
				return dir_srch->fpath;
			}
		}
		n2t_debug_f("No files with the extension '%s' were found in the directory '%s'",
		ext, dir);
		n2t_free(dir_srch->dot_ext);
		n2t_free(dir_srch->fpath);
	}
	
	n2t_debug("couldn't open source directory");
	n2t_free(dir_srch->dot_ext);
	n2t_free(dir_srch->fpath);
	dir_srch->state = n2t_dir_open_failed;
	return NULL;
}

n2t_dir_state n2t_dir_get_next_file(struct n2t_dir_search *dir_srch) {
	
	while((dir_srch->ep = readdir(dir_srch->dp))) {
		char *c = strstr(dir_srch->ep->d_name, dir_srch->dot_ext);
		if(c) {
			strcpy(&dir_srch->fpath[dir_srch->dir_len+1], dir_srch->ep->d_name);
			dir_srch->state = n2t_dir_success;
			return dir_srch->fpath;
		}
	}
	closedir(dir_srch->dp);
	n2t_free(dir_srch->fpath);
	dir_srch->fpath = NULL;
	n2t_free(dir_srch->dot_ext);
	dir_srch->state = n2t_dir_no_more;
	return NULL;
}
#endif


void n2t_parse_path(struct n2t_path_info *pinfo, const char *path) {
	
	size_t path_len;
	size_t base_name_pos = 0;
	for(path_len = 0; path[path_len]; path_len++) {
		if((path[path_len] == '\\' || path[path_len] == '/') &&
		    path[path_len+1] && path[path_len+1] != '\\' &&
		    path[path_len+1] != '/') {
			
			base_name_pos = path_len+1;
		}
	}
	size_t trailing_delimiters = 0;
	for(size_t i = path_len-1; path[i] == '\\' || path[i] == '/'; i--) {
		trailing_delimiters++;
	}
	size_t ext_pos = path_len;
	for(size_t i = path_len-1; i > base_name_pos; i--) {
		if(path[i] == '.') {
			//ext_pos = i+1;
			ext_pos = i;
			break;
		}
	}
	size_t base_name_len;
	size_t ext_len;
	if(ext_pos == path_len) {
		base_name_len = ext_pos - base_name_pos - trailing_delimiters;
		ext_len = 0;
	}
	else {
		base_name_len = ext_pos - base_name_pos - /*1 -*/ trailing_delimiters;
		ext_len = path_len - ext_pos;
	}
	pinfo->path_len = path_len;
	pinfo->base_name_pos = base_name_pos;
	pinfo->base_name_len = base_name_len;
	pinfo->ext_pos = ext_pos;
	pinfo->ext_len = ext_len;
}


#ifdef _WIN32
#include <Shlwapi.h>

enum n2t_path_type n2t_get_path_type(const char *path) {

	if(PathFileExistsA(path) == TRUE) {
		if(PathIsDirectoryA(path) == FILE_ATTRIBUTE_DIRECTORY) {
			return n2t_path_dir;
		}
		return n2t_path_file;
	}
	n2t_debug_f("path '%s' not found", path);
	return n2t_path_not_found;
}

#else
#include <sys/stat.h>

enum n2t_path_type n2t_get_path_type(const char *path) {
	
	struct stat sb;
	if(!stat(path, &sb)) {
		
		if(S_ISDIR(sb.st_mode)) {
			return n2t_path_dir;
		}
		return n2t_path_file;
	}
	n2t_debug("could not get path type");
	return n2t_path_error;
}
#endif


size_t n2t_getdelim(char **lineptr, size_t *n, int delim, FILE *stream) {
	
	if(feof(stream)) return -1;
	if(!lineptr || !n) {
		errno = EINVAL;
		return -1;
	}
	
	VEC(char) buf;
	if(!*lineptr && !*n) {
		if(VEC_INIT(buf) != VEC_SUCCESS) {
			n2t_debug("VEC_INIT(buf) failed");
			errno = ENOMEM;
			return -1;
		}
	}
	else {
		memset(&buf, 0, sizeof(buf));
		buf.data = *lineptr;
		buf.capacity = *n;
	}
	
	int c;
	while((c = getc(stream)) != EOF && c != delim) {
		//printf("buf.length: %zu\n", buf.length);
		int ret = VEC_PUSH(buf, c);
		//printf("ret = %d\n", ret);
		if(ret != VEC_SUCCESS) {
			n2t_debug("VEC_PUSH(buf, c) failed");
			if(ret == VEC_NOMEM) errno = ENOMEM;
			else if(ret == VEC_OVERFLOW) errno = ERANGE;
			return -1;
		}
	}
	if(ferror(stream)) {
		n2t_debug("ferror(stream)");
		return -1;
	}
	int ret = VEC_PUSH(buf, '\0');
	if(ret != VEC_SUCCESS) {
		n2t_debug("VEC_PUSH(buf, '\0')");
		if(ret == VEC_NOMEM) errno = ENOMEM;
		else if(ret == VEC_OVERFLOW) errno = ERANGE;
		return -1;
	}
	*lineptr = buf.data;
	*n = buf.capacity;
	return buf.length-1;
}

size_t n2t_getline(char **lineptr, size_t *n, FILE *stream) {
	return n2t_getdelim(lineptr, n, '\n', stream);
}
