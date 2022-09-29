cmake_minimum_required(VERSION 3.16.3)
project(nar_disk_image)


set(CMAKE_CXX_STANDARD 17)


# default build type is debug.
if (NOT CMAKE_BUILD_TYPE)
	
	# check env variable first
	if (DEFINED ENV{BG_NAR_WEBSERVICE_BUILD_TYPE})
		message(STATUS "Found BG_NAR_WEBSERVICE_BUILD_TYPE env variable")
		set(CMAKE_BUILD_TYPE "$ENV{BG_NAR_WEBSERVICE_BUILD_TYPE}")
	else()
		set(CMAKE_BUILD_TYPE "Debug")
	endif()

endif()


message(STATUS "Build type is ${CMAKE_BUILD_TYPE}")
include_directories(thirdparty)
include_directories(include)


if (UNIX) 
	# implement!
endif ()

if (WIN32) 
	
	# option(BUILD_STATIC_LIBS TRUE)

	add_library(nar_disk_image_native SHARED)
	add_subdirectory(${CMAKE_SOURCE_DIR}\\thirdparty\\lz4\\build\\cmake\\)
	add_subdirectory(${CMAKE_SOURCE_DIR}\\thirdparty\\zstd\\build\\cmake\\)

	target_compile_definitions(nar_disk_image_native PUBLIC 
		_CRT_SECURE_NO_WARNINGS=1
		$<$<CONFIG:Debug>:BG_ENABLE_LEAKCHECK=1>
		$<$<CONFIG:Debug>:BG_DEBUG_SETUP=1>
		$<$<CONFIG:Debug>:DEBUG=1>
		
		$<$<CONFIG:Release>:BG_RELEASE_SETUP=1>
		$<$<CONFIG:Release>:BG_FLUSH_LOGS_TO_STDOUT=0>
	)

	target_compile_definitions(nar_disk_image_native PRIVATE
		BG_USE_MIMALLOC=0
		BG_BUILD_AS_DLL=1
	)


	target_compile_options(nar_disk_image_native PUBLIC 
		-W4
		-WX
		/wd4201
		#/w36001
		# unreferenced internal function removed
		/wd4505 

		-nologo
		/std:c++17
		/MP		
		/LD
		-GS- 

		$<$<CONFIG:Debug>:-Od> 
		$<$<CONFIG:Debug>:-DEBUG:FULL> 
		$<$<CONFIG:Debug>:-Zi>
		$<$<CONFIG:Debug>:-FC>

		#$<$<CONFIG:Release>:-DEBUG:FULL> 
		#$<$<CONFIG:Release>:-Zi>
		#$<$<CONFIG:Release>:-FC>
		$<$<CONFIG:Release>:-O2> 
		#$<$<CONFIG:Debug>:/fsanitize=address>
	)

	target_compile_options(nar_disk_image_native PRIVATE 
		$<$<CONFIG:Debug>:/MDd>
		$<$<CONFIG:Release>:/MD>
		-GR- 
		-EHa- 
		-Gm-
	)


    target_include_directories(nar_disk_image_native PUBLIC thirdparty/lz4/lib)
    target_include_directories(nar_disk_image_native PUBLIC thirdparty/zstd/lib)
    target_include_directories(nar_disk_image_native PUBLIC thirdparty/mimalloc/include)
	
	target_sources(nar_disk_image_native PRIVATE 
		src/bg.cpp
		src/nar.cpp
		src/nar_win32.cpp
		src/package.cpp	
		src/platform_io.cpp
		src/file_explorer.cpp
		#src/nar_compression.cpp
	)


	target_link_options(nar_disk_image_native PRIVATE 
		-incremental:no
		-opt:ref 
		-machine:x64 
		/INCREMENTAL:NO
		$<$<CONFIG:Release>:/GL>
		$<$<CONFIG:Release>:/LTCG>
		#-manifest:no
	)

	#windows stuff
    target_link_libraries(nar_disk_image_native PUBLIC 
		user32.lib
		kernel32.lib
		vssapi.lib
		Dbghelp.lib
	)

	# thirdparty
	target_link_libraries(nar_disk_image_native PUBLIC 
		libzstd_static
		lz4_static
	)

endif ()

