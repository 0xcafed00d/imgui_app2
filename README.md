## IMGUI_APP

`imgui_app` packages Dear ImGui and SDL 3 into a small static library for building GUI applications. The default path uses SDL's renderer backend, and the library can also run Dear ImGui on an SDL-owned OpenGL context. Optional Raylib support builds on that OpenGL path so Raylib drawing commands can render into an ImGui UI.

The project requires CMake 3.25 or newer.

## Including The Library

Use `FetchContent` to include the library in your project:

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

Link the core app library when you only need Dear ImGui with SDL:

```cmake
add_executable(my_app src/main.cpp)

target_link_libraries(my_app
	PRIVATE
	imgui_app::imgui_app)
```

You can override the fetched SDL and Dear ImGui sources before `FetchContent_MakeAvailable(imgui_app)`:

```cmake
set(SDL_URL "https://github.com/libsdl-org/SDL.git")
set(SDL_TAG "release-3.4.8")

set(IMGUI_URL "https://github.com/ocornut/imgui.git")
set(IMGUI_TAG "v1.92.7")
```

## Basic App

`ImGuiApp::Init()` creates an SDL window and SDL renderer, then initializes Dear ImGui with the SDLRenderer backend. The render loop is driven by `ImGuiApp::Loop()`, and returns `false` when the app should exit.

```cpp
#include "imgui_app.h"

int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;

	if (!ImGuiApp::Init("Sample ImGui Application", 1024, 768)) {
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
	return 0;
}
```

## Backends

The core library supports two Dear ImGui renderer backends:

```cpp
bool Init(const char* title, int width, int height);
bool InitSDLRenderer(const char* title, int width, int height);
bool InitOpenGL(const char* title, int width, int height, const char* glsl_version = nullptr);
bool AttachSDLRenderer(SDL_Window* window, SDL_Renderer* renderer);
bool AttachOpenGL(SDL_Window* window, SDL_GLContext gl_context, const char* glsl_version = nullptr);
Backend GetBackend();
```

`Init()` is the same as `InitSDLRenderer()`. Use `InitOpenGL()` when your application wants `ImGuiApp` to create the SDL window and OpenGL context. Use `AttachSDLRenderer()` or `AttachOpenGL()` when another system already owns the SDL objects.

Objects passed to `AttachSDLRenderer()` or `AttachOpenGL()` are borrowed. `ImGuiApp::Shutdown()` shuts down ImGui backends but does not destroy borrowed SDL windows, renderers, GL contexts, or SDL itself.

You can retrieve the active native objects when you need to integrate with other SDL or OpenGL code:

```cpp
void GetSDLWindowAndRenderer(SDL_Window** window, SDL_Renderer** renderer);
void GetSDLWindowAndGLContext(SDL_Window** window, SDL_GLContext* gl_context);
```

The renderer is `nullptr` when the OpenGL backend is active. The GL context is `nullptr` when the SDLRenderer backend is active.

## ImGui Textures

`ImGuiApp` can create RGBA8 textures for the active backend and return the correct `ImTextureID` for `ImGui::Image()`.

```cpp
ImGuiApp::Texture* texture = ImGuiApp::CreateTexture(
	128,
	128,
	ImGuiApp::TextureFlagPreserveContents,
	ImGuiApp::TextureFilter::Nearest);

void* pixels = nullptr;
int pitch = 0;

if (texture != nullptr && ImGuiApp::LockTexture(texture, &pixels, &pitch)) {
	for (int y = 0; y < 128; ++y) {
		unsigned char* row = static_cast<unsigned char*>(pixels) + y * pitch;
		for (int x = 0; x < 128; ++x) {
			const bool light = ((x / 16) + (y / 16)) % 2 == 0;
			row[x * 4 + 0] = light ? 240 : 60;
			row[x * 4 + 1] = light ? 160 : 90;
			row[x * 4 + 2] = light ? 70 : 160;
			row[x * 4 + 3] = 255;
		}
	}
	ImGuiApp::UnlockTexture(texture);
}

while (ImGuiApp::Loop()) {
	ImGui::Begin("Texture");
	if (texture != nullptr) {
		ImGui::Image(ImGuiApp::GetTextureID(texture), ImVec2(128.0f, 128.0f));
	}
	ImGui::End();
}

ImGuiApp::FreeTexture(texture);
ImGuiApp::Shutdown();
```

Texture API:

```cpp
enum TextureFlags : unsigned int {
	TextureFlagNone = 0,
	TextureFlagPreserveContents = 1u << 0,
};

enum class TextureFilter {
	Nearest,
	Linear,
};

Texture* CreateTexture(
	int width,
	int height,
	unsigned int flags = TextureFlagNone,
	TextureFilter filter = TextureFilter::Linear);
Texture* CreateTexture(int width, int height, TextureFilter filter);
bool LockTexture(Texture* texture, void** pixels, int* pitch);
void UnlockTexture(Texture* texture);
void FreeTexture(Texture* texture);
ImTextureID GetTextureID(const Texture* texture);
```

Texture filtering defaults to `TextureFilter::Linear`. Use `TextureFilter::Nearest` for pixel art, checkerboards, or other textures that should stay sharp when scaled. The filter is applied through `SDL_SetTextureScaleMode()` on the SDLRenderer backend and `GL_TEXTURE_MIN_FILTER`/`GL_TEXTURE_MAG_FILTER` on the OpenGL backend.

For the SDLRenderer backend, the default lock path uses SDL's streaming texture lock and should be treated as write-only. Pass `TextureFlagPreserveContents` when you want lock/unlock to preserve pixels between edits. That flag makes `ImGuiApp` keep a CPU staging buffer and upload it on `UnlockTexture()`.

For the OpenGL backend, textures always use a CPU staging buffer and upload to the GL texture on `UnlockTexture()`. `TextureFlagPreserveContents` has no additional effect there.

Call `FreeTexture()` before `Shutdown()` so the native texture is released while the SDL renderer or OpenGL context is still alive.

## Optional Raylib Support

Enable Raylib support before including this library:

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
	imgui_app::imgui_raylib)
```

By default, this fetches [raysan5/raylib](https://github.com/raysan5/raylib) and sets Raylib's `PLATFORM` option to `SDL`, so Raylib creates an SDL window and OpenGL context that ImGui can attach to.

You can override the Raylib source, version, or platform before `FetchContent_MakeAvailable(imgui_app)`:

```cmake
set(RAYLIB_URL "https://github.com/raysan5/raylib.git")
set(RAYLIB_TAG "6.0")
set(RAYLIB_PLATFORM "SDL")
```

If your application already provides a `raylib` target, `imgui_raylib` links to it. Set `IMGUI_APP_FETCH_RAYLIB` to `OFF` when you want this library to call `find_package(raylib CONFIG REQUIRED)` instead of fetching Raylib.

## Raylib Inside ImGui

`ImGuiRaylib::Init()` initializes Raylib first, then attaches `ImGuiApp` to Raylib's SDL window and OpenGL context. The application render loop is still controlled by `ImGuiApp::Loop()`.

Use `BeginRaylib()` and `EndRaylib()` inside any ImGui window. Raylib commands between them render into a Raylib `RenderTexture2D`, and `EndRaylib()` displays that OpenGL texture with `ImGui::Image()`.

```cpp
#include "imgui_app.h"
#include "imgui_raylib.h"

int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;

	if (!ImGuiRaylib::Init("ImGui + Raylib", 1024, 768)) {
		return -1;
	}

	while (ImGuiApp::Loop()) {
		ImGui::Begin("Raylib panel");

		if (ImGuiRaylib::BeginRaylib("scene", ImVec2(640, 360), ImColor(28, 32, 42, 255))) {
			DrawText("Hello from Raylib", 24, 24, 24, RAYWHITE);
			DrawCircle(320, 180, 48, SKYBLUE);

			const ImVec2 texture_size = ImGuiRaylib::GetCurrentSize();
			ImGuiRaylib::EndRaylib();

			if (ImGui::IsItemHovered()) {
				const ImVec2 item_min = ImGui::GetItemRectMin();
				const ImVec2 item_size = ImGui::GetItemRectSize();
				const ImVec2 mouse = ImGui::GetMousePos();
				const float u = (mouse.x - item_min.x) / item_size.x;
				const float v = (mouse.y - item_min.y) / item_size.y;
				ImGui::SetTooltip(
					"texture: %.0f, %.0f\nuv: %.3f, %.3f",
					u * texture_size.x,
					v * texture_size.y,
					u,
					v);
			}
		}

		ImGui::End();
	}

	ImGuiRaylib::Shutdown();
	return 0;
}
```

`GetCurrentSize()` returns the current Raylib render texture size while a Raylib canvas is active. After `EndRaylib()`, the rendered image is the last ImGui item, so standard ImGui item APIs such as `IsItemHovered()`, `GetItemRectMin()`, and `GetItemRectSize()` can be used to map mouse position back to texture coordinates.

Call `ImGuiRaylib::Shutdown()` at the end of the application. It releases Raylib render targets, shuts down ImGui through `ImGuiApp::Shutdown()`, and closes the Raylib window in the correct order.

`imgui_raylib` requires the OpenGL backend. Use `ImGuiRaylib::Init()` for Raylib integration; `ImGuiApp::Init()` remains the SDLRenderer-backed path for ImGui-only applications.
