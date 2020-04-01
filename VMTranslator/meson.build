
hvm_common_src = ['hvm.c']

symbols = [['command-table.gperf', 'command-table.h'],
		   ['segment-table.gperf', 'segment-table.h']]

foreach s : symbols
	hvm_common_src += custom_target(
        s[1],
        input : s[0],
        output : s[1],
        command : [gperf, '@INPUT@', '--output-file', '@OUTPUT@'])
endforeach


hvm_lib = library('hvm',
		sources: hvm_common_src,
		dependencies: libs,
		link_with: n2t_common_lib,
		include_directories : common_dir)

executable('hvm', sources : ['hvm-cmd.c'],
		   dependencies: libs,
		   link_with: [hvm_lib, n2t_common_lib],
		   include_directories : common_dir)


#executable('hvm_test', sources: ['hvm-test.c'],
#		   dependencies: libs,
#		   link_with:  [hvm_lib, n2t_common_lib],
#		   include_directories : common_dir)
		   
