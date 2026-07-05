## IMGUI_APP

Packaging Dear::imgui and SDL 3 into a complete statically linked library to build multi platform GUI apps, with no external dependencies.
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

target_link_libraries(my_app 
  PRIVATE
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

The basic SDLRenderer-backed API is `Init`, `Loop`, `RequestQuit`, and `Shutdown`:
 
```cpp
#include "imgui_app.h"

int main(int argc, char* argv[]) {
	if (!ImGuiApp::Init("Sample Imgui Application", 1024, 768)) {
		return -1;
	}

	std::string text{"includes imgui_stdlib"};

	while (ImGuiApp::Loop()) {
		ImGui::Begin("Hello, world!");
		ImGui::Text("This is some useful text.");
		ImGui::InputText("Input", &text);
		if (ImGui::Button("exit")) {
			ImGuiApp::RequestQuit();
		}
		ImGui::End();
	}

	ImGuiApp::Shutdown();
}
```

There are also helper functions:
```cpp
GetSDLWindowAndRenderer(SDL_Window** window, SDL_Renderer** renderer);
GetSDLWindowAndGLContext(SDL_Window** window, SDL_GLContext* gl_context);
```
These return the SDL objects currently being used by the ImGuiApp. The renderer will be `nullptr` when the OpenGL backend is active, and the GL context will be `nullptr` when the SDLRenderer backend is active.

## ImGui textures

`ImGuiApp` can create RGBA8 textures for the active backend. With the SDLRenderer backend this wraps an `SDL_Texture`; pass `TextureFlagPreserveContents` when you want locks to return a persistent CPU staging buffer. With the OpenGL backend this wraps a GL texture and always uses a CPU staging buffer while locked, so that flag has no extra effect there.

```cpp
ImGuiApp::Texture* texture = ImGuiApp::CreateTexture(128, 128, ImGuiApp::TextureFlagPreserveContents);

void* pixels = nullptr;
int pitch = 0;
if (texture != nullptr && ImGuiApp::LockTexture(texture, &pixels, &pitch)) {
	for (int y = 0; y < 128; ++y) {
		unsigned char* row = static_cast<unsigned char*>(pixels) + y * pitch;
		for (int x = 0; x < 128; ++x) {
			row[x * 4 + 0] = 255;
			row[x * 4 + 1] = 128;
			row[x * 4 + 2] = 32;
			row[x * 4 + 3] = 255;
		}
	}
	ImGuiApp::UnlockTexture(texture);
}

while (ImGuiApp::Loop()) {
	ImGui::Begin("Texture");
	if (texture != nullptr) {
		ImGui::Image(ImGuiApp::GetTextureID(texture), ImVec2(128, 128));
	}
	ImGui::End();
}

ImGuiApp::FreeTexture(texture);
ImGuiApp::Shutdown();
```

Call `FreeTexture()` before `Shutdown()` so the native texture is released while the renderer or OpenGL context is still alive.

If your application or another library already owns an SDL window and OpenGL context, attach ImGui to those instead of letting `imgui_app` create its own SDLRenderer:

```cpp
SDL_Window* window = /* created elsewhere */;
SDL_GLContext gl_context = /* created elsewhere */;

if (!ImGuiApp::AttachOpenGL(window, gl_context, "#version 330")) {
	return -1;
}

while (ImGuiApp::Loop()) {
	ImGui::ShowDemoWindow();
}

ImGuiApp::Shutdown();
```

Objects passed to `AttachSDLRenderer()` or `AttachOpenGL()` are borrowed. `ImGuiApp::Shutdown()` shuts down ImGui backends but does not destroy the supplied SDL window, renderer, GL context, or SDL itself.

## Optional Raylib support

Enable Raylib support before including this library. By default this fetches [raysan5/raylib](https://github.com/raysan5/raylib) and sets Raylib's `PLATFORM` option to `SDL` so Raylib uses its SDL backend.

```cmake
include(FetchContent)

set(IMGUI_APP_ENABLE_RAYLIB ON)

FetchContent_Declare(
	imgui_app
	GIT_REPOSITORY "https://github.com/0xcafed00d/imgui_app.git"
	GIT_TAG main
	GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(imgui_app)

add_executable(my_app src/main.cpp)

target_link_libraries(my_app
	PRIVATE
	imgui_raylib)
```

You can override the Raylib source, version, or platform before `FetchContent_MakeAvailable(imgui_app)`:

```cmake
set(RAYLIB_URL "https://github.com/raysan5/raylib.git")
set(RAYLIB_TAG "6.0")
set(RAYLIB_PLATFORM "SDL")
```

If your application already provides a `raylib` target, `imgui_raylib` will link to it. Set `IMGUI_APP_FETCH_RAYLIB` to `OFF` when you want this library to call `find_package(raylib CONFIG REQUIRED)` instead of downloading Raylib.

## Raylib inside ImGui

Link `imgui_raylib` when you want to use Raylib drawing commands inside an ImGui UI. Raylib creates the SDL window and OpenGL context first, then ImGui attaches to those borrowed objects. `BeginRaylib()` renders to a Raylib `RenderTexture2D` and displays the OpenGL texture directly with `ImGui::Image`.

```cmake
set(IMGUI_APP_ENABLE_RAYLIB ON)

add_executable(my_app src/main.cpp)

target_link_libraries(my_app
	PRIVATE
	imgui_raylib)
```

Use `BeginRaylib()` and `EndRaylib()` inside any ImGui window:

```cpp
#include "imgui_app.h"
#include "imgui_raylib.h"

int main(int argc, char* argv[]) {
	if (!ImGuiRaylib::Init("ImGui + Raylib", 1024, 768)) {
		return -1;
	}

	while (ImGuiApp::Loop()) {
		ImGui::Begin("Raylib panel");

		if (ImGuiRaylib::BeginRaylib("scene", ImVec2(640, 360), ImColor(28, 32, 42, 255))) {
			DrawText("Hello from Raylib", 24, 24, 24, RAYWHITE);
			DrawCircle(320, 180, 48, SKYBLUE);
			ImGuiRaylib::EndRaylib();
		}

		ImGui::End();
	}

	ImGuiRaylib::Shutdown();
}
```

Call `ImGuiRaylib::Shutdown()` at the end of the application. It releases Raylib render targets, shuts down ImGui, and closes the Raylib window in the correct order.

`imgui_raylib` requires the OpenGL backend. Use `ImGuiRaylib::Init()` for Raylib integration; `ImGuiApp::Init()` remains the SDLRenderer-backed path for ImGui-only applications.
