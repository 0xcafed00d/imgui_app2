#pragma once

#include "imgui.h"
#include "raylib.h"

namespace ImGuiRaylib {
	constexpr unsigned int DefaultConfigFlags = FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI;

	bool Init(
		const char* title,
		int width,
		int height,
		const char* glsl_version = "#version 330",
		unsigned int config_flags = DefaultConfigFlags);

	bool BeginRaylib(const char* id, ImVec2 size = ImVec2(0.0f, 0.0f), ImColor clear_color = ImColor(0, 0, 0, 255));
	void EndRaylib();
	void Shutdown();

	ImVec2 GetCurrentSize();

}  // namespace ImGuiRaylib
