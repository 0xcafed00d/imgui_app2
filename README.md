## IMGUI_APP

Packaging Dear::imgui and SDL 3, into a complete staticly linked library to build multi platform GUI apps, with no external dependencies. 
Uses SDL renderer to render output so is not dependent on OpenGL, Vulkan, or Direct3d. (although SDL itself may use these)

## Usage: 

Use `FetchContent` to include lib in your project:
 
```cmake
include(FetchContent)
FetchContent_Declare(
	imgui_app
    GIT_REPOSITORY "https://github.com/0xcafed00d/imgui_app.git"
    GIT_TAG main
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(imgui_app)
```

Build your application as usual with CMAKE;
```cmake
add_executable(my_app src/main.cpp)

# set location of SDL
target_link_directories(my_app
	PRIVATE
	${SDL3_LIB_DIR}
)

# link to SDL and imgui_app + add any other libs you need
target_link_libraries(my_app 
  PRIVATE
  SDL3::SDL3main
  SDL3::SDL3-static
  imgui_app)
```

You can overide the location and version of SDL3 that is fetched by setting the variables `SDL_URL` and `SDL_TAG` in your CMakeLists.txt before you call FetchContent: 
```cmake
set(SDL_URL "https://github.com/libsdl-org/SDL.git")
set(SDL_TAG "release-3.4.8")
```

You can also overide the location and version of Dear::imgui that is fetched by setting the variables `IMGUI_URL` and `IMGUI_TAG` in your CMakeLists.txt before you call FetchContent: 
```cmake
set(IMGUI_URL "https://github.com/ocornut/imgui.git")
set(IMGUI_TAG "v1.92.7")
```

There are only 4 api functions, init, loop, request_quit, and shutdown:
 
```cpp
#include "imgui_app.h"

int main(int argc, char* argv[]) {
	if (!ImGuiApp::init("Sample Imgui Application", 1024, 768)) {
		return -1;
	}

	std::string text{"includes imgui_stdlib"};

	while (ImGuiApp::loop()) {
		ImGui::Begin("Hello, world!");
		ImGui::Text("This is some useful text.");
		ImGui::InputText("Input", &text);
		if (ImGui::Button("exit")) {
			ImGuiApp::request_quit();
		}
		ImGui::End();
	}

	ImGuiApp::shutdown();
}
```

There is also a helper function: 
```cpp
get_SDL_window_and_renderer(SDL_Window** window, SDL_Renderer** renderer);
```
That will return the SDL_Window and SDL_Renderer that are being used by the ImGuiApp. This is useful if you want to use SDL functions directly, for example to load textures or handle events.