#include "imgui_app.h"
#include "imgui_raylib.h"

#include <cmath>

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

			ImGuiRaylib::EndRaylib();
		}

		if (ImGui::Button("exit")) {
			ImGuiApp::RequestQuit();
		}
		ImGui::End();
	}

	ImGuiRaylib::Shutdown();
	return 0;
}
