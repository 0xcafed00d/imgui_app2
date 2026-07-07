#include <SDL3/SDL.h>

#include "imgui_app.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include <stdio.h>

namespace ImGuiApp {
	namespace {
		constexpr unsigned int GL_COLOR_BUFFER_BIT_VALUE = 0x00004000;
		constexpr unsigned int GL_FRAMEBUFFER_VALUE = 0x8D40;

		using GlBindFramebufferProc = void (*)(unsigned int, unsigned int);
		using GlClearColorProc = void (*)(float, float, float, float);
		using GlClearProc = void (*)(unsigned int);
	}

	static bool running{false};
	static Backend backend{Backend::None};
	static SDL_Window* window{nullptr};
	static SDL_Renderer* renderer{nullptr};
	static SDL_GLContext gl_context{nullptr};
	static SDL_Event event{};
	static bool begin_frame_called{false};
	static bool owns_sdl{false};
	static bool owns_window{false};
	static bool owns_renderer{false};
	static bool owns_gl_context{false};
	static bool imgui_context_ready{false};
	static bool platform_backend_ready{false};
	static bool renderer_backend_ready{false};

	static bool ensure_not_initialized() {
		if (backend != Backend::None || window != nullptr || renderer != nullptr || gl_context != nullptr) {
			SDL_Log("ImGuiApp is already initialized.");
			return false;
		}

		return true;
	}

	static bool setup_imgui_context() {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		imgui_context_ready = true;
		ImGuiIO& io = ImGui::GetIO();
		(void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

		ImGui::StyleColorsDark();
		return true;
	}

	static bool make_opengl_current() {
		if (window == nullptr || gl_context == nullptr) {
			return false;
		}

		return SDL_GL_MakeCurrent(window, gl_context);
	}

	static void clear_opengl_framebuffer(const ImVec4& clear_color) {
		static GlBindFramebufferProc gl_bind_framebuffer = reinterpret_cast<GlBindFramebufferProc>(SDL_GL_GetProcAddress("glBindFramebuffer"));
		static GlClearColorProc gl_clear_color = reinterpret_cast<GlClearColorProc>(SDL_GL_GetProcAddress("glClearColor"));
		static GlClearProc gl_clear = reinterpret_cast<GlClearProc>(SDL_GL_GetProcAddress("glClear"));

		if (gl_bind_framebuffer != nullptr) {
			gl_bind_framebuffer(GL_FRAMEBUFFER_VALUE, 0);
		}

		if (gl_clear_color == nullptr || gl_clear == nullptr) {
			return;
		}

		gl_clear_color(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		gl_clear(GL_COLOR_BUFFER_BIT_VALUE);
	}

	static bool setup_sdl_renderer_backend(SDL_Window* new_window, SDL_Renderer* new_renderer) {
		if (new_window == nullptr || new_renderer == nullptr) {
			SDL_Log("ImGuiApp SDLRenderer backend requires a valid SDL_Window and SDL_Renderer.");
			return false;
		}

		window = new_window;
		renderer = new_renderer;
		gl_context = nullptr;
		backend = Backend::SDLRenderer;

		setup_imgui_context();

		if (!ImGui_ImplSDL3_InitForSDLRenderer(window, renderer)) {
			Shutdown();
			return false;
		}
		platform_backend_ready = true;

		if (!ImGui_ImplSDLRenderer3_Init(renderer)) {
			Shutdown();
			return false;
		}
		renderer_backend_ready = true;

		running = true;
		begin_frame_called = false;
		return true;
	}

	static bool setup_opengl_backend(SDL_Window* new_window, SDL_GLContext new_gl_context, const char* glsl_version) {
		if (new_window == nullptr || new_gl_context == nullptr) {
			SDL_Log("ImGuiApp OpenGL backend requires a valid SDL_Window and SDL_GLContext.");
			return false;
		}

		window = new_window;
		renderer = nullptr;
		gl_context = new_gl_context;
		backend = Backend::OpenGL;

		if (!make_opengl_current()) {
			SDL_Log("ImGuiApp failed to make the supplied OpenGL context current: %s", SDL_GetError());
			Shutdown();
			return false;
		}

		setup_imgui_context();

		if (!ImGui_ImplSDL3_InitForOpenGL(window, gl_context)) {
			Shutdown();
			return false;
		}
		platform_backend_ready = true;

		if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
			Shutdown();
			return false;
		}
		renderer_backend_ready = true;

		running = true;
		begin_frame_called = false;
		return true;
	}

	bool Init(const char* title, int width, int height) {
		return InitSDLRenderer(title, width, height);
	}

	bool InitSDLRenderer(const char* title, int width, int height) {
		if (!ensure_not_initialized()) {
			return false;
		}

		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
			printf("Error: %s\n", SDL_GetError());
			return false;
		}
		owns_sdl = true;

		// Create window with SDL_Renderer graphics context
		float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
		SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

		window = SDL_CreateWindow(title, (int)(width * main_scale), (int)(height * main_scale), window_flags);
		if (window == nullptr) {
			printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
			Shutdown();
			return false;
		}
		owns_window = true;

		renderer = SDL_CreateRenderer(window, nullptr);
		if (renderer == nullptr) {
			SDL_Log("Error creating SDL_Renderer!");
			Shutdown();
			return false;
		}
		owns_renderer = true;
		SDL_SetRenderVSync(renderer, 1);
		SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		SDL_ShowWindow(window);

		return setup_sdl_renderer_backend(window, renderer);
	}

	bool InitOpenGL(const char* title, int width, int height, const char* glsl_version) {
		if (!ensure_not_initialized()) {
			return false;
		}

		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
			printf("Error: %s\n", SDL_GetError());
			return false;
		}
		owns_sdl = true;

		float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
		SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

		window = SDL_CreateWindow(title, (int)(width * main_scale), (int)(height * main_scale), window_flags);
		if (window == nullptr) {
			printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
			Shutdown();
			return false;
		}
		owns_window = true;

		gl_context = SDL_GL_CreateContext(window);
		if (gl_context == nullptr) {
			printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
			Shutdown();
			return false;
		}
		owns_gl_context = true;

		SDL_GL_MakeCurrent(window, gl_context);
		SDL_GL_SetSwapInterval(1);
		SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		SDL_ShowWindow(window);

		return setup_opengl_backend(window, gl_context, glsl_version);
	}

	bool AttachSDLRenderer(SDL_Window* new_window, SDL_Renderer* new_renderer) {
		if (!ensure_not_initialized()) {
			return false;
		}

		return setup_sdl_renderer_backend(new_window, new_renderer);
	}

	bool AttachOpenGL(SDL_Window* new_window, SDL_GLContext new_gl_context, const char* glsl_version) {
		if (!ensure_not_initialized()) {
			return false;
		}

		return setup_opengl_backend(new_window, new_gl_context, glsl_version);
	}

	bool begin_frame() {
		begin_frame_called = true;

		// Poll and handle events (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui
		// wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main
		// application, or clear/overwrite your copy of the mouse data.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main
		// application, or clear/overwrite your copy of the keyboard data. Generally you may always
		// pass all inputs to dear imgui, and hide them from your application based on those two
		// flags.

		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT)
				running = false;
			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
				running = false;
		}

		// [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate() function]
		if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
			SDL_Delay(10);
		}

		// Start the Dear ImGui frame
		switch (backend) {
			case Backend::SDLRenderer:
				ImGui_ImplSDLRenderer3_NewFrame();
				break;
			case Backend::OpenGL:
				if (!make_opengl_current()) {
					running = false;
					return false;
				}
				ImGui_ImplOpenGL3_NewFrame();
				break;
			case Backend::None:
				running = false;
				return false;
		}

		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		return running;
	}

	void end_frame() {
		static ImVec4 clear_color{0.45f, 0.55f, 0.60f, 1.00f};

		ImGui::Render();
		ImGuiIO& io = ImGui::GetIO();

		switch (backend) {
			case Backend::SDLRenderer:
				SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
				SDL_SetRenderDrawColorFloat(renderer, clear_color.x, clear_color.y, clear_color.z, clear_color.w);
				SDL_RenderClear(renderer);
				ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
				SDL_RenderPresent(renderer);
				break;
			case Backend::OpenGL:
				if (make_opengl_current()) {
					clear_opengl_framebuffer(clear_color);
					ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
					SDL_GL_SwapWindow(window);
				}
				break;
			case Backend::None:
				break;
		}
	}

	bool Loop() {
		if (begin_frame_called) {
			end_frame();
		}
		return begin_frame();
	}

	bool is_running() {
		return running;
	}

	void RequestQuit() {
		running = false;
	}

	void Shutdown() {
		running = false;

		if (renderer_backend_ready && backend == Backend::OpenGL) {
			make_opengl_current();
			ImGui_ImplOpenGL3_Shutdown();
		} else if (renderer_backend_ready && backend == Backend::SDLRenderer) {
			ImGui_ImplSDLRenderer3_Shutdown();
		}

		if (platform_backend_ready) {
			ImGui_ImplSDL3_Shutdown();
		}

		if (imgui_context_ready && ImGui::GetCurrentContext() != nullptr) {
			ImGui::DestroyContext();
		}

		if (owns_renderer && renderer != nullptr) {
			SDL_DestroyRenderer(renderer);
		}

		if (owns_gl_context && gl_context != nullptr) {
			SDL_GL_DestroyContext(gl_context);
		}

		if (owns_window && window != nullptr) {
			SDL_DestroyWindow(window);
		}

		if (owns_sdl) {
			SDL_Quit();
		}

		backend = Backend::None;
		window = nullptr;
		renderer = nullptr;
		gl_context = nullptr;
		begin_frame_called = false;
		owns_sdl = false;
		owns_window = false;
		owns_renderer = false;
		owns_gl_context = false;
		imgui_context_ready = false;
		platform_backend_ready = false;
		renderer_backend_ready = false;
	}

	Backend GetBackend() {
		return backend;
	}

	void GetSDLWindowAndRenderer(SDL_Window** window, SDL_Renderer** renderer) {
		*window = ImGuiApp::window;
		*renderer = ImGuiApp::renderer;
	}

	void GetSDLWindowAndGLContext(SDL_Window** window, SDL_GLContext* gl_context) {
		*window = ImGuiApp::window;
		*gl_context = ImGuiApp::gl_context;
	}

}  // namespace ImGuiApp
