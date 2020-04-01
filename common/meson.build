
cc = meson.get_compiler('c')
libs = []

if get_option('buildtype') == 'debug'
	add_project_arguments(
	'-DN2T_DEBUG',
	language: 'c')
endif

if host_machine.system() == 'windows'
	libs += cc.find_library('user32')
	libs += cc.find_library('shlwapi')
	add_project_arguments(
	'-D_CRT_SECURE_NO_WARNINGS',
	language: 'c')
	
	if cc.get_id() == 'gcc'
		# mingw most likely. use mingw-provided stdio lib instead of Windows's MSVCRT lib
		# since the latter does not support C99 format specifiers like %zu
		add_project_arguments(
		'-D__USE_MINGW_ANSI_STDIO',
		language: 'c')
	endif
endif

n2t_common_lib = library('n2t-common', 'n2t-common.c', dependencies: libs)

common_dir = include_directories('.')

gperf = find_program('gperf')
