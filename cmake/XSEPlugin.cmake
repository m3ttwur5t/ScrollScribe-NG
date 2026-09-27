option(BUILD_SKYRIM "Build for Skyrim" OFF)
option(BUILD_FALLOUT4 "Build for Fallout 4" OFF)

if(BUILD_SKYRIM)
	add_compile_definitions(SKYRIM)
	set(CommonLibName "CommonLibSSE")
	set(GameVersion "Skyrim")
elseif(BUILD_FALLOUT4)
	add_compile_definitions(FALLOUT4)
	set(CommonLibPath "CommonLibF4/CommonLibF4")
	set(CommonLibName "external/CommonLibF4")
	set(GameVersion "Fallout 4")
else()
	message(
	FATAL_ERROR
		"A game must be selected."
	)
endif()

add_library("${PROJECT_NAME}" SHARED)

target_compile_features(
	"${PROJECT_NAME}"
	PRIVATE
		cxx_std_23
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(AddCXXFiles)
add_cxx_files("${PROJECT_NAME}")

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/Plugin.h.in
	${CMAKE_CURRENT_BINARY_DIR}/cmake/Plugin.h
	@ONLY
)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/cmake/version.rc
	@ONLY
)

target_sources(
	"${PROJECT_NAME}"
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}/cmake/Plugin.h
		${CMAKE_CURRENT_BINARY_DIR}/cmake/version.rc
)

target_precompile_headers(
	"${PROJECT_NAME}"
	PRIVATE
		include/PCH.h
)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

if (CMAKE_GENERATOR MATCHES "Visual Studio")
	add_compile_definitions(_UNICODE)

	target_compile_definitions(${PROJECT_NAME} PRIVATE "$<$<CONFIG:DEBUG>:DEBUG>")

	set(SC_RELEASE_OPTS "/Zi;/fp:fast;/GL;/Gy-;/Gm-;/Gw;/sdl-;/GS-;/guard:cf-;/O2;/Ob2;/Oi;/Ot;/Oy;/fp:except-")	
	
	target_compile_options(
		"${PROJECT_NAME}"
		PRIVATE
			/MP
			/W4
			/WX
			/permissive-
			/Zc:alignedNew
			/Zc:auto
			/Zc:__cplusplus
			/Zc:externC
			/Zc:externConstexpr
			/Zc:forScope
			/Zc:hiddenFriend
			/Zc:implicitNoexcept
			/Zc:lambda
			/Zc:noexceptTypes
			/Zc:preprocessor
			/Zc:referenceBinding
			/Zc:rvalueCast
			/Zc:sizedDealloc
			/Zc:strictStrings
			/Zc:ternary
			/Zc:threadSafeInit
			/Zc:trigraphs
			/Zc:wchar_t
			/wd4200 # nonstandard extension used : zero-sized array in struct/union
	)

	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:DEBUG>:/fp:strict>")
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:DEBUG>:/ZI>")
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:DEBUG>:/Od>")
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:DEBUG>:/Gy>")
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:${SC_RELEASE_OPTS}>")

	target_link_options(
		${PROJECT_NAME}
		PRIVATE
			/WX
			"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
			"$<$<CONFIG:RELEASE>:/LTCG;/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
endif()

if (BUILD_SKYRIM)
	# CommonLibSSE-NG is consumed from the lib/commonlibsse-ng git submodule
	# (alandtse/CommonLibSSE-NG, tracks current Skyrim SE/AE/VR 1.7.x runtimes).
	# The old vcpkg-registry distribution (colorglass registry, 3.x line) is
	# no longer maintained.
	if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlibsse-ng/CMakeLists.txt")
		message(FATAL_ERROR
			"CommonLibSSE-NG is not checked out. Run:\n"
			"  git submodule update --init --recursive")
	endif()

	# We only want the library itself, not its unit tests.
	set(BUILD_TESTS OFF CACHE BOOL "Build CommonLibSSE unit tests" FORCE)

	add_subdirectory(
		"${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlibsse-ng"
		"${CMAKE_CURRENT_BINARY_DIR}/commonlibsse-ng"
		EXCLUDE_FROM_ALL)
else()
	add_subdirectory(${CommonLibPath} ${CommonLibName} EXCLUDE_FROM_ALL)
endif()

find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(SimpleIni CONFIG REQUIRED)

# include/PCH.h includes xbyak directly (legacy trampoline boilerplate).
find_path(XBYAK_INCLUDE_DIR "xbyak/xbyak.h" REQUIRED)

target_include_directories(
	"${PROJECT_NAME}"
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}/cmake
		${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_include_directories(
	"${PROJECT_NAME}"
	SYSTEM PRIVATE
		${XBYAK_INCLUDE_DIR}
)

target_link_libraries(
	"${PROJECT_NAME}" 
	PUBLIC 
		CommonLibSSE::CommonLibSSE
	PRIVATE
		fmt::fmt
)

# The exported target name for the SimpleIni port varies; link whichever exists.
if(TARGET SimpleIni::SimpleIni)
	target_link_libraries(${PROJECT_NAME} PRIVATE SimpleIni::SimpleIni)
elseif(TARGET SimpleIni)
	target_link_libraries(${PROJECT_NAME} PRIVATE SimpleIni)
else()
	message(FATAL_ERROR "SimpleIni package found but no linkable target (expected SimpleIni::SimpleIni or SimpleIni)")
endif()
