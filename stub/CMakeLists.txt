file(GLOB sources *.cpp)
file(GLOB sources-headers *.hpp)
add_library(vulkan-stub STATIC
	${sources}
	${sources-headers})

target_include_directories(vulkan-stub PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(vulkan-stub platform)

