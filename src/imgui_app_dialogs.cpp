#include <SDL3/SDL.h>

#include "imgui_app.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <utility>

namespace ImGuiApp {
	namespace {
		struct BlockingFileDialogState {
			std::mutex mutex{};
			std::condition_variable completed_changed{};
			FileDialogResult result{};
		};

		SDL_Window* current_window() {
			SDL_Window* current_window{nullptr};
			SDL_Renderer* current_renderer{nullptr};
			GetSDLWindowAndRenderer(&current_window, &current_renderer);
			return current_window;
		}

		void SDLCALL blocking_file_dialog_callback(void* userdata, const char* const* filelist, int filter) {
			BlockingFileDialogState* state = static_cast<BlockingFileDialogState*>(userdata);
			if (state == nullptr) {
				return;
			}

			FileDialogResult result{};
			result.completed = true;
			result.filter = filter;

			if (filelist == nullptr) {
				result.error = true;
				result.error_message = SDL_GetError();
			} else if (*filelist == nullptr) {
				result.canceled = true;
			} else {
				result.accepted = true;
				for (int index = 0; filelist[index] != nullptr; ++index) {
					result.paths.emplace_back(filelist[index]);
				}
			}

			{
				std::lock_guard<std::mutex> lock(state->mutex);
				state->result = std::move(result);
			}
			state->completed_changed.notify_one();
		}

		FileDialogResult wait_for_file_dialog(BlockingFileDialogState& state) {
			for (;;) {
				{
					std::lock_guard<std::mutex> lock(state.mutex);
					if (state.result.completed) {
						return state.result;
					}
				}

				SDL_PumpEvents();

				std::unique_lock<std::mutex> lock(state.mutex);
				if (state.result.completed) {
					return state.result;
				}
				state.completed_changed.wait_for(lock, std::chrono::milliseconds(10));
			}
		}
	}  // namespace

	bool ShowOpenFileDialog(
		FileDialogCallback callback,
		void* userdata,
		const FileDialogFilter* filters,
		int nfilters,
		const char* default_location,
		bool allow_many) {
		if (callback == nullptr) {
			SDL_Log("ImGuiApp::ShowOpenFileDialog requires a callback.");
			return false;
		}

		SDL_ShowOpenFileDialog(callback, userdata, current_window(), filters, nfilters, default_location, allow_many);
		return true;
	}

	bool ShowSaveFileDialog(
		FileDialogCallback callback,
		void* userdata,
		const FileDialogFilter* filters,
		int nfilters,
		const char* default_location) {
		if (callback == nullptr) {
			SDL_Log("ImGuiApp::ShowSaveFileDialog requires a callback.");
			return false;
		}

		SDL_ShowSaveFileDialog(callback, userdata, current_window(), filters, nfilters, default_location);
		return true;
	}

	FileDialogResult OpenFileDialog(
		const FileDialogFilter* filters,
		int nfilters,
		const char* default_location,
		bool allow_many) {
		BlockingFileDialogState state{};
		SDL_ShowOpenFileDialog(blocking_file_dialog_callback, &state, current_window(), filters, nfilters, default_location, allow_many);
		return wait_for_file_dialog(state);
	}

	FileDialogResult SaveFileDialog(
		const FileDialogFilter* filters,
		int nfilters,
		const char* default_location) {
		BlockingFileDialogState state{};
		SDL_ShowSaveFileDialog(blocking_file_dialog_callback, &state, current_window(), filters, nfilters, default_location);
		return wait_for_file_dialog(state);
	}

}  // namespace ImGuiApp
