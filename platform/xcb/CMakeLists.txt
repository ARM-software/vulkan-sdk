file(GLOB sources *.cpp)
file(GLOB sources-headers *.hpp)

add_library(platform-xcb STATIC
	${sources}
	${sources-headers})

target_link_libraries(platform-xcb platform)

if (XCB_LIB_DIR)
	set(XCB_LIB_DIR "-L${XCB_LIB_DIR}")
endif(XCB_LIB_DIR)

target_link_libraries(platform-xcb framework platform-wsi platform-asset-manager ${XCB_LIB_DIR} -lxcb-util -lxcb)

if(XCB_INCLUDE_DIR)
	target_include_directories(platform-xcb PUBLIC ${XCB_INCLUDE_DIR})
endif(XCB_INCLUDE_DIR)

