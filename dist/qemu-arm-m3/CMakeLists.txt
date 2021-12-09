set_property(SOURCE src/startup.S PROPERTY LANGUAGE C)
add_library(arm-bsp src/crt0.c src/startup.S)
set_source_files_properties(src/startup.S PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/arm.ld)

if(CMAKE_VERSION VERSION_GREATER "3.12")
	# Allow the target_link_libraries() below.
	cmake_policy(SET CMP0079 NEW)
endif()
target_link_libraries(libzth INTERFACE arm-bsp)

install(TARGETS arm-bsp EXPORT libzth
	ARCHIVE DESTINATION lib
)
