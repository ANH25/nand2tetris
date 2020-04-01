
hasm_common_src = ['hasm.c']

symbols = [['comp-symbols.gperf', 'comp-symbols.h'],
		   ['jump-symbols.gperf', 'jump-symbols.h'],
		   ['predefined-symbols.gperf', 'predefined-symbols.h']]

foreach s : symbols
	hasm_common_src += custom_target(
        s[1],
        input : s[0],
        output : s[1],
        command : [gperf, '@INPUT@', '--output-file', '@OUTPUT@'])
endforeach


hasm_lib = library('hasm',
		sources: hasm_common_src,
		dependencies: libs,
		link_with: n2t_common_lib,
		include_directories : common_dir)

executable('hasm', sources: ['hasm-cmd.c'],
		   dependencies: libs,
		   link_with: [hasm_lib, n2t_common_lib],
		   include_directories : common_dir)

executable('hasm_test', sources: ['hasm-test.c'],
		   dependencies: libs,
		   link_with: hasm_lib,
		   include_directories : common_dir)
		   
