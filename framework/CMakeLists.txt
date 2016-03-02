file(GLOB sources *.cpp)
file(GLOB sources-headers *.hpp)
file(GLOB device-sources device/*.cpp)
file(GLOB device-sources-headers device/*.hpp)
add_library(framework STATIC
	${sources}
	${sources-headers}
	${device-sources}
	${device-sources-headers})

if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/glm/glm/glm.hpp)
	message(FATAL_ERROR "GLM submodule has not been checked out in framework/glm, please run git submodule init and git submodule update first.")
endif()
if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/stb/stb_image.h)
	message(FATAL_ERROR "STB submodule has not been checked out in framework/stb, please run git submodule init and git submodule update first.")
endif()

target_include_directories(framework PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/glm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(framework vulkan-stub)

