file(GLOB sources *.cpp)
file(GLOB sources-headers *.hpp)

add_library(platform-android STATIC
	${sources}
	${sources-headers}
	${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
	${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c)

find_library(log-lib log)
find_library(android-lib android)
target_link_libraries(platform-android framework platform-wsi platform-asset-manager ${log-lib} ${android-lib})
target_include_directories(platform-android
    PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
    ${ANDROID_NDK}/sources/android/cpufeatures)

# Include Validation layers in the APK
set(layer-files ${ANDROID_NDK}/sources/third_party/vulkan/src/build-android/jniLibs/${ANDROID_ABI})
file(GLOB layers ${layer-files}/*.so)
file(MAKE_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
foreach(layer ${layers})
    get_filename_component(layer-basename ${layer} NAME)
    add_custom_command(TARGET platform-android POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${layer} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${layer-basename})
endforeach()
