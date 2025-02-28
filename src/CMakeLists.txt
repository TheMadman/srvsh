add_executable(srvsh srvsh.c parse.c process.c)

target_link_libraries(srvsh adt scallop-lang)

install(TARGETS srvsh
	DESTINATION bin)
install(DIRECTORY srvsh
	DESTINATION include)
