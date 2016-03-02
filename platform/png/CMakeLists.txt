file(GLOB sources *.cpp)
file(GLOB sources-headers *.hpp)

add_library(platform-png STATIC
	${sources}
	${sources-headers})

if (${CMAKE_BUILD_TYPE} MATCHES "Rel")
	target_compile_definitions(platform-png PRIVATE FORCE_NO_VALIDATION=1)
endif()

target_link_libraries(platform-png framework platform-asset-manager)
