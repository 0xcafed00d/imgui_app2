cmake_minimum_required(VERSION 3.25)

project(imgui_app VERSION 1.0.0 LANGUAGES CXX)

include (cmake/tools.cmake)

option(IMGUI_APP_ENABLE_RAYLIB "Build the optional raylib application helper." OFF)
option(IMGUI_APP_FETCH_RAYLIB "Fetch raylib when raylib support is enabled and no raylib target exists." ON)

if(IMGUI_APP_ENABLE_RAYLIB AND CMAKE_VERSION VERSION_LESS 3.25)
	message(FATAL_ERROR "IMGUI_APP_ENABLE_RAYLIB uses raylib 6.0 by default, which requires CMake 3.25 or newer.")
endif()


if ("${SDL_URL}" STREQUAL "")
	set(SDL_URL "https://github.com/libsdl-org/SDL.git")
endif()

if ("${SDL_TAG}" STREQUAL "")
	set(SDL_TAG "release-3.4.8")
endif()

if ("${IMGUI_URL}" STREQUAL "")
	set(IMGUI_URL "https://github.com/ocornut/imgui.git")
endif()

if ("${IMGUI_TAG}" STREQUAL "")
	set(IMGUI_TAG "v1.92.7")
endif()

if ("${RAYLIB_URL}" STREQUAL "")
	set(RAYLIB_URL "https://github.com/raysan5/raylib.git")
endif()

if ("${RAYLIB_TAG}" STREQUAL "")
	set(RAYLIB_TAG "6.0")
endif()

if ("${RAYLIB_PLATFORM}" STREQUAL "")
	set(RAYLIB_PLATFORM "SDL")
endif()

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY ${IMGUI_URL}
    GIT_TAG ${IMGUI_TAG}
    GIT_PROGRESS TRUE
	OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(imgui)

set(SDL_SHARED FALSE CACHE BOOL "Build a SDL shared library (if available)")
set(SDL_STATIC TRUE CACHE BOOL "Build a SDL static library (if available)")

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY ${SDL_URL}
    GIT_TAG ${SDL_TAG}
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
	OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(SDL3)

set (SDL3_INCLUDE_DIR ${SDL3_SOURCE_DIR}/include)
set (SDL3_LIB_DIR ${SDL3_BINARY_DIR})


add_library(
	lib_imgui
	STATIC
	${imgui_SOURCE_DIR}/imconfig.h
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_internal.h
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/imstb_rectpack.h
	${imgui_SOURCE_DIR}/imstb_textedit.h
	${imgui_SOURCE_DIR}/imstb_truetype.h
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.h
	${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.h
	${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.h
)


target_include_directories(
	lib_imgui
	PUBLIC
	${imgui_SOURCE_DIR}
	${imgui_SOURCE_DIR}/backends
	${SDL3_INCLUDE_DIR}
)

if(CMAKE_DL_LIBS)
	target_link_libraries(
		lib_imgui
		PUBLIC
		${CMAKE_DL_LIBS}
	)
endif()

add_library(
	imgui_app
	STATIC
	src/imgui_app.h
	src/imgui_app.cpp
	src/imgui_app_dialogs.cpp
	src/imgui_app_textures.cpp
)

add_library(imgui_app::imgui_app ALIAS imgui_app)

target_include_directories(
	imgui_app
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
	imgui_app
	PUBLIC
	lib_imgui
	SDL3::SDL3-static
)

if(IMGUI_APP_ENABLE_RAYLIB)
	if(NOT TARGET raylib)
		if(IMGUI_APP_FETCH_RAYLIB)
			set(PLATFORM ${RAYLIB_PLATFORM} CACHE STRING "Platform to build raylib for." FORCE)

			FetchContent_Declare(
				raylib
				GIT_REPOSITORY ${RAYLIB_URL}
				GIT_TAG ${RAYLIB_TAG}
				GIT_SHALLOW TRUE
				GIT_PROGRESS TRUE
				OVERRIDE_FIND_PACKAGE
			)
			FetchContent_MakeAvailable(raylib)
		else()
			find_package(raylib CONFIG REQUIRED)
		endif()
	endif()

	add_library(
		imgui_raylib
		STATIC
		src/imgui_raylib.h
		src/imgui_raylib.cpp
	)

	add_library(imgui_app::imgui_raylib ALIAS imgui_raylib)

	target_include_directories(
		imgui_raylib
		PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/src
	)

	target_link_libraries(
		imgui_raylib
		PUBLIC
		imgui_app
		raylib
	)
endif()

# ImGui example application
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)

    add_executable(
        imgui_sample_app
        src/samples/imgui_sample_app.cpp
    )

    target_link_libraries(
        imgui_sample_app
        imgui_app
    )

	if(IMGUI_APP_ENABLE_RAYLIB)
		add_executable(
			imgui_raylib_sample_app
			src/samples/imgui_raylib_sample_app.cpp
		)

		target_link_libraries(
			imgui_raylib_sample_app
			imgui_raylib
		)
	endif()
endif()
