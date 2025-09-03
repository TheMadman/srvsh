add_library(srvsh SHARED srvsh.c)

target_link_libraries(srvsh adt scallop-lang)

target_include_directories(srvsh
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/srvsh>
	$<INSTALL_INTERFACE:include>)

add_executable(srvsh-bin main.c parse.c process.c)
target_link_libraries(srvsh-bin srvsh)
set_target_properties(srvsh-bin
	PROPERTIES OUTPUT_NAME srvsh)

install(TARGETS srvsh
	DESTINATION lib)
install(TARGETS srvsh-bin
	DESTINATION bin)
install(FILES srvsh/srvsh.h
	DESTINATION include)
