
jc_common_src = ['jack-tokenizer.c']


jc_common_src += custom_target(
	'jack-literals.h',
	input : 'jack-literals.gperf',
	output : 'jack-literals.h',
	command : [gperf, '@INPUT@', '--output-file', '@OUTPUT@'])


#add_project_arguments('-DJC_XML', language: 'c')


jc_lib = library('jc',
		sources: jc_common_src,
		dependencies: libs,
		link_with: n2t_common_lib,
		include_directories : common_dir)

executable('jc', sources : ['jack-compiler.c', 'jack-compiler-cmd.c'],
		   dependencies: libs,
		   link_with: [jc_lib, n2t_common_lib],
		   include_directories : common_dir)


executable('jtkn', sources : ['jack-tokenizer-cmd.c'],
		   dependencies: libs,
		   link_with: [jc_lib, n2t_common_lib],
		   include_directories : common_dir)
