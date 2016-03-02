file(GLOB sources *.cpp)
file(GLOB sources-headers *.hpp)

add_library(platform-wayland STATIC
	${sources}
	${sources-headers})

target_link_libraries(platform-wayland platform)

if (WAYLAND_LIB_DIR)
	set(WAYLAND_LIB_DIR "-L${WAYLAND_LIB_DIR}")
endif(WAYLAND_LIB_DIR)

target_link_libraries(platform-wayland framework platform-wsi platform-asset-manager ${WAYLAND_LIB_DIR} -lwayland-client)

if(WAYLAND_INCLUDE_DIR)
	target_include_directories(platform-wayland PUBLIC ${WAYLAND_INCLUDE_DIR})
endif(WAYLAND_INCLUDE_DIR)

