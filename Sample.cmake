function(add_shader TARGET SHADER)

	# Find glslc shader compiler.
	# On Android, the NDK includes the binary, so no external dependency.
	if(ANDROID)
		file(GLOB glslc-folders ${ANDROID_NDK}/shader-tools/*)
		find_program(GLSLC glslc HINTS ${glslc-folders})
	else()
		find_program(GLSLC glslc)
	endif()

	# All shaders for a sample are found here.
	set(current-shader-path ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER})

	# For Android, write SPIR-V files to app/assets which is then packaged into the APK.
	# Otherwise, output in the binary directory.
	if(ANDROID)
		set(current-output-path ${CMAKE_CURRENT_SOURCE_DIR}/app/assets/shaders/${SHADER}.spv)
	else(ANDROID)
		set(current-output-path ${CMAKE_BINARY_DIR}/samples/${TARGET}/assets/shaders/${SHADER}.spv)
	endif(ANDROID)

	# Add a custom command to compile GLSL to SPIR-V.
	get_filename_component(current-output-dir ${current-output-path} DIRECTORY)
	file(MAKE_DIRECTORY ${current-output-dir})
	add_custom_command(
		OUTPUT ${current-output-path}
		COMMAND ${GLSLC} -o ${current-output-path} ${current-shader-path}
		DEPENDS ${current-shader-path}
		IMPLICIT_DEPENDS CXX ${current-shader-path}
		VERBATIM)

	# Make sure our native build depends on this output.
	set_source_files_properties(${current-output-path} PROPERTIES GENERATED TRUE)
	target_sources(${TARGET} PRIVATE ${current-output-path})
endfunction(add_shader)

function(add_sample_inner TARGET SOURCES)
	# For Android, always emit libnative.so since we only build one sample per APK.
	# Otherwise, we want to control our output folder so we can pick up assets without any problems.
	if(ANDROID)
		add_library(${TARGET} SHARED ${SOURCES})
		target_link_libraries(${TARGET} vulkan-sdk)
		set_target_properties(${TARGET} PROPERTIES LIBRARY_OUTPUT_NAME native)
	else(ANDROID)
		add_executable(${TARGET} ${SOURCES})
		target_link_libraries(${TARGET} vulkan-sdk)
		set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/samples/${TARGET}")
	endif(ANDROID)

	# Find all shaders.
	file(GLOB vertex-shaders ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert)
	file(GLOB fragment-shaders ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag)
	file(GLOB compute-shaders ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp)

	# Add them to the build.
	foreach(vertex-shader ${vertex-shaders})
		get_filename_component(p ${vertex-shader} NAME)
		add_shader(${TARGET} ${p})
	endforeach(vertex-shader)

	foreach(fragment-shader ${fragment-shaders})
		get_filename_component(p ${fragment-shader} NAME)
		add_shader(${TARGET} ${p})
	endforeach(fragment-shader)

	foreach(compute-shader ${compute-shaders})
		get_filename_component(p ${compute-shader} NAME)
		add_shader(${TARGET} ${p})
	endforeach(compute-shader)

	if(ANDROID)
		set(output-assets ${CMAKE_CURRENT_SOURCE_DIR}/app/assets)
	else(ANDROID)
		set(output-assets ${CMAKE_BINARY_DIR}/samples/${TARGET}/assets)
	endif(ANDROID)

	# Copy assets from sample to the appropriate location.
	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures)
		file(MAKE_DIRECTORY ${output-assets})
		add_custom_command(TARGET ${TARGET} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures ${output-assets}/textures)
	endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures)

	# Make a test out of every sample if not on Android.
	if(NOT ANDROID)
		add_test(NAME ${TARGET} COMMAND $<TARGET_FILE:${TARGET}> 200)
	endif(NOT ANDROID)
endfunction(add_sample_inner)

function(add_sample TARGET SOURCES)
	# If FILTER_TARGET is used, we only build a particular sample, this is used for Android.
	if (FILTER_TARGET)
		if (${FILTER_TARGET} STREQUAL ${TARGET})
			add_sample_inner(${TARGET} "${SOURCES}")
		endif(${FILTER_TARGET} STREQUAL ${TARGET})
	else(FILTER_TARGET)
		add_sample_inner(${TARGET} "${SOURCES}")
	endif(FILTER_TARGET)
endfunction(add_sample)
