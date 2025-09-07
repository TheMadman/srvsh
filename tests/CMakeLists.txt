function(testcase target)
	add_executable(${target}_test ${target}.c)
	target_link_libraries(${target}_test srvsh)
	add_test(NAME ${target} COMMAND ${target}_test)
endfunction(testcase)

testcase(srvsh_srvsh)
