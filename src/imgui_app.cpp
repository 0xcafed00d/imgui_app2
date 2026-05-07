#include <SDL3/SDL.h>

#include "imgui_app.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include <stdio.h>

namespace ImGuiApp {
	static bool running{false};
	static SDL_Window* window{nullptr};
	static SDL_Renderer* renderer{nullptr};
	static SDL_Event event{};
	static bool begin_frame_called{false};

	bool init(const char* title, int width, int height) {
		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
			printf("Error: %s\n", SDL_GetError());
			return false;
		}

		// Create window with SDL_Renderer graphics context
		float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
		SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

		window = SDL_CreateWindow(title, (int)(width * main_scale), (int)(height * main_scale), window_flags);
		if (window == nullptr) {
			printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
			return false;
		}

		renderer = SDL_CreateRenderer(window, nullptr);
		if (renderer == nullptr) {
			SDL_Log("Error creating SDL_Renderer!");
			return false;
		}
		SDL_SetRenderVSync(renderer, 1);
		SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		SDL_ShowWindow(window);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		(void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		// ImGui::StyleColorsLight();

		// Setup Platform/Renderer backends
		ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
		ImGui_ImplSDLRenderer3_Init(renderer);

		running = true;
		return true;
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
		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		return running;
	}

	void end_frame() {
		static ImVec4 clear_color{0.45f, 0.55f, 0.60f, 1.00f};

		ImGui::Render();
		ImGuiIO& io = ImGui::GetIO();
		SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
		SDL_SetRenderDrawColorFloat(renderer, clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
		SDL_RenderPresent(renderer);
	}

	bool loop() {
		if (begin_frame_called) {
			end_frame();
		}
		return begin_frame();
	}

	bool is_running() {
		return running;
	}

	void request_quit() {
		running = false;
	}

	void shutdown() {
		running = false;
		ImGui_ImplSDLRenderer3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();

		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}

	void get_SDL_window_and_renderer(SDL_Window** window, SDL_Renderer** renderer) {
		*window = ImGuiApp::window;
		*renderer = ImGuiApp::renderer;
	}

}  // namespace ImGuiApp
