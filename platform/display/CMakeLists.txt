file(GLOB sources *.cpp)
file(GLOB sources-headers *.hpp)

add_library(platform-display STATIC
	${sources}
	${sources-headers})

target_link_libraries(platform-display framework platform-wsi platform-asset-manager)

