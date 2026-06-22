cmake_minimum_required(VERSION 3.16)

project(imgui_template)

include (cmake/tools.cmake)


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

# ImGui example application
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)

    add_executable(
        ${PROJECT_NAME}
        src/imgui_app.h
        src/imgui_app.cpp
        src/sample_app.cpp
    )

    target_link_libraries(
        ${PROJECT_NAME} 
        lib_imgui
    #	SDL3::SDL3main
        SDL3::SDL3-static
    )

endif()

