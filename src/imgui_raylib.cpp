#include "imgui_raylib.h"

#include <SDL3/SDL.h>

#include "imgui_app.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace ImGuiRaylib {
	namespace {
		struct Canvas {
			RenderTexture2D target{};
			int width{0};
			int height{0};
			ImVec2 display_size{0.0f, 0.0f};
			bool target_loaded{false};
		};

		static std::unordered_map<std::string, Canvas> canvases{};
		static Canvas* active_canvas{nullptr};
		static SDL_Window* raylib_window{nullptr};
		static SDL_GLContext raylib_gl_context{nullptr};
		static bool raylib_ready{false};

		bool raylib_uses_imgui_opengl_context() {
			SDL_Window* window{nullptr};
			SDL_GLContext gl_context{nullptr};
			ImGuiApp::GetSDLWindowAndGLContext(&window, &gl_context);

			return IsWindowReady() && window != nullptr && gl_context != nullptr &&
				   static_cast<SDL_Window*>(GetWindowHandle()) == window;
		}

		bool make_raylib_context_current() {
			if (raylib_window == nullptr || raylib_gl_context == nullptr) {
				return true;
			}

			return SDL_GL_MakeCurrent(raylib_window, raylib_gl_context);
		}

		void close_raylib_window() {
			if (raylib_ready && IsWindowReady()) {
				CloseWindow();
			}

			raylib_ready = false;
			raylib_window = nullptr;
			raylib_gl_context = nullptr;
		}

		bool ensure_raylib_ready() {
			if (ImGuiApp::GetBackend() != ImGuiApp::Backend::OpenGL) {
				SDL_Log("ImGuiRaylib requires ImGuiApp's OpenGL backend. Use ImGuiRaylib::Init() instead of ImGuiApp::Init().");
				return false;
			}

			if (!raylib_uses_imgui_opengl_context()) {
				SDL_Log("ImGuiRaylib requires raylib InitWindow() before ImGuiApp::AttachOpenGL().");
				return false;
			}

			ImGuiApp::GetSDLWindowAndGLContext(&raylib_window, &raylib_gl_context);
			raylib_ready = true;
			return make_raylib_context_current();
		}

		void unload_canvas(Canvas& canvas) {
			if (canvas.target_loaded) {
				make_raylib_context_current();
				UnloadRenderTexture(canvas.target);
				canvas.target = {};
				canvas.target_loaded = false;
			}

			canvas.width = 0;
			canvas.height = 0;
			canvas.display_size = ImVec2(0.0f, 0.0f);
		}

		bool ensure_canvas(Canvas& canvas, int width, int height, ImVec2 display_size) {
			if (canvas.target_loaded && canvas.width == width && canvas.height == height) {
				canvas.display_size = display_size;
				return true;
			}

			unload_canvas(canvas);

			if (!make_raylib_context_current()) {
				return false;
			}

			canvas.target = LoadRenderTexture(width, height);
			canvas.target_loaded = IsRenderTextureValid(canvas.target);
			if (!canvas.target_loaded) {
				canvas.target = {};
				return false;
			}

			canvas.width = width;
			canvas.height = height;
			canvas.display_size = display_size;
			return true;
		}

		void show_canvas(Canvas& canvas) {
			ImGui::Image(
				static_cast<ImTextureID>(canvas.target.texture.id),
				canvas.display_size,
				ImVec2(0.0f, 1.0f),
				ImVec2(1.0f, 0.0f));
		}

		unsigned char color_component_to_byte(float value) {
			return static_cast<unsigned char>(std::clamp(static_cast<int>(std::lround(value * 255.0f)), 0, 255));
		}

		Color to_raylib_color(const ImColor& color) {
			return Color{
				color_component_to_byte(color.Value.x),
				color_component_to_byte(color.Value.y),
				color_component_to_byte(color.Value.z),
				color_component_to_byte(color.Value.w),
			};
		}

		ImVec2 resolve_size(ImVec2 size) {
			if (size.x <= 0.0f || size.y <= 0.0f) {
				ImVec2 available = ImGui::GetContentRegionAvail();
				if (size.x <= 0.0f) {
					size.x = available.x;
				}
				if (size.y <= 0.0f) {
					size.y = available.y;
				}
			}

			size.x = std::max(size.x, 1.0f);
			size.y = std::max(size.y, 1.0f);
			return size;
		}
	}  // namespace

	bool Init(const char* title, int width, int height, const char* glsl_version, unsigned int config_flags) {
		if (IsWindowReady()) {
			SDL_Log("ImGuiRaylib::Init requires raylib to be uninitialized.");
			return false;
		}

		if (ImGuiApp::GetBackend() != ImGuiApp::Backend::None) {
			SDL_Log("ImGuiRaylib::Init requires ImGuiApp to be uninitialized.");
			return false;
		}

		SetConfigFlags(config_flags);
		InitWindow(std::max(width, 1), std::max(height, 1), title != nullptr ? title : "ImGui + Raylib");
		if (!IsWindowReady()) {
			return false;
		}

		SDL_Window* window = static_cast<SDL_Window*>(GetWindowHandle());
		SDL_GLContext gl_context = SDL_GL_GetCurrentContext();
		if (window == nullptr || gl_context == nullptr) {
			CloseWindow();
			return false;
		}

		if (!ImGuiApp::AttachOpenGL(window, gl_context, glsl_version)) {
			CloseWindow();
			return false;
		}

		raylib_window = window;
		raylib_gl_context = gl_context;
		raylib_ready = true;
		return true;
	}

	bool BeginRaylib(const char* id, ImVec2 size, ImColor clear_color) {
		if (active_canvas != nullptr) {
			return false;
		}

		size = resolve_size(size);
		const int width = std::max(1, static_cast<int>(std::lround(size.x)));
		const int height = std::max(1, static_cast<int>(std::lround(size.y)));

		if (!ensure_raylib_ready()) {
			ImGui::Dummy(size);
			return false;
		}

		Canvas& canvas = canvases[std::string(id != nullptr ? id : "##raylib")];
		if (!ensure_canvas(canvas, width, height, size)) {
			ImGui::Dummy(size);
			return false;
		}

		active_canvas = &canvas;

		make_raylib_context_current();
		BeginTextureMode(canvas.target);
		ClearBackground(to_raylib_color(clear_color));
		return true;
	}

	void EndRaylib() {
		if (active_canvas == nullptr) {
			return;
		}

		make_raylib_context_current();
		EndTextureMode();
		show_canvas(*active_canvas);

		active_canvas = nullptr;
	}

	void Shutdown() {
		if (active_canvas != nullptr) {
			make_raylib_context_current();
			EndTextureMode();
			active_canvas = nullptr;
		}

		for (auto& entry : canvases) {
			unload_canvas(entry.second);
		}
		canvases.clear();

		ImGuiApp::Shutdown();
		close_raylib_window();
	}

	ImVec2 GetCurrentSize() {
		if (active_canvas == nullptr) {
			return ImVec2(0.0f, 0.0f);
		}

		return ImVec2(static_cast<float>(active_canvas->width), static_cast<float>(active_canvas->height));
	}

}  // namespace ImGuiRaylib
