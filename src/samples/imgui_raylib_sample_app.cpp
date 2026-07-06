#include "imgui_app.h"
#include "imgui_raylib.h"

#include <algorithm>
#include <cmath>

namespace {
	void ShowTextureCoordinateTooltip(ImVec2 texture_size) {
		if (!ImGui::IsItemHovered()) {
			return;
		}

		const ImVec2 item_min = ImGui::GetItemRectMin();
		const ImVec2 item_size = ImGui::GetItemRectSize();
		if (item_size.x <= 0.0f || item_size.y <= 0.0f || texture_size.x <= 0.0f || texture_size.y <= 0.0f) {
			return;
		}

		const ImVec2 mouse = ImGui::GetMousePos();
		const float u = std::clamp((mouse.x - item_min.x) / item_size.x, 0.0f, 1.0f);
		const float v = std::clamp((mouse.y - item_min.y) / item_size.y, 0.0f, 1.0f);
		const float texture_x = u * texture_size.x;
		const float texture_y = v * texture_size.y;
		const int pixel_x = std::clamp(static_cast<int>(std::floor(texture_x)), 0, std::max(0, static_cast<int>(texture_size.x) - 1));
		const int pixel_y = std::clamp(static_cast<int>(std::floor(texture_y)), 0, std::max(0, static_cast<int>(texture_size.y) - 1));

		ImGui::SetTooltip("texture: %d, %d\nuv: %.3f, %.3f", pixel_x, pixel_y, u, v);
	}
}  // namespace

int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;

	if (!ImGuiRaylib::Init("Sample ImGui + Raylib Application", 1024, 768)) {
		return -1;
	}

	while (ImGuiApp::Loop()) {
		ImGui::Begin("Raylib in ImGui");
		ImGui::Text("Raylib draws into the panel below.");

		const ImVec2 size{640.0f, 360.0f};
		if (ImGuiRaylib::BeginRaylib("demo-scene", size, ImColor(28, 32, 42, 255))) {
			const float t = static_cast<float>(ImGui::GetTime());
			const float x = 320.0f + std::cos(t) * 180.0f;
			const float y = 180.0f + std::sin(t * 1.4f) * 100.0f;

			DrawText("Raylib render target", 24, 24, 24, RAYWHITE);
			DrawCircle(static_cast<int>(x), static_cast<int>(y), 42.0f, SKYBLUE);
			DrawCircleLines(320, 180, 120.0f, Fade(ORANGE, 0.75f));
			DrawRectangleLines(0, 0, static_cast<int>(size.x), static_cast<int>(size.y), Fade(RAYWHITE, 0.45f));

			const ImVec2 texture_size = ImGuiRaylib::GetCurrentSize();
			ImGuiRaylib::EndRaylib();
			ShowTextureCoordinateTooltip(texture_size);
		}

		if (ImGui::Button("exit")) {
			ImGuiApp::RequestQuit();
		}
		ImGui::End();
	}

	ImGuiRaylib::Shutdown();
	return 0;
}
