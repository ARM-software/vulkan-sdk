file(GLOB sources-headers *.hpp)

add_library(platform STATIC ../.dummy.cpp
	${sources-headers})

add_library(platform-wsi STATIC
    wsi/wsi.cpp
    wsi/wsi.hpp)

add_library(platform-asset-manager STATIC
    asset_manager.cpp
    asset_manager.hpp)

if (${CMAKE_BUILD_TYPE} MATCHES "Rel")
    target_compile_definitions(platform-wsi PRIVATE FORCE_NO_VALIDATION=1)
endif()

add_subdirectory(${PLATFORM})

target_include_directories(platform
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_link_libraries(platform framework platform-${PLATFORM})
target_link_libraries(platform-wsi vulkan-stub)
target_link_libraries(platform-asset-manager framework)
