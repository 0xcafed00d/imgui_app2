#include "imgui_app.h"

namespace {
	std::string DescribeDialogResult(const char* action, const ImGuiApp::FileDialogResult& result) {
		if (result.error) {
			return std::string(action) + " error: " + result.error_message;
		}

		if (result.canceled) {
			return std::string(action) + " canceled";
		}

		if (!result.paths.empty()) {
			return std::string(action) + ": " + result.paths.front();
		}

		return std::string(action) + " completed with no selection";
	}
}  // namespace

int main(int argc, char* argv[]) {
	if (!ImGuiApp::Init("Sample Imgui Application", 1024, 768)) {
		return -1;
	}

	std::string text{"includes imgui_stdlib"};
	std::string file_dialog_status{"No file selected"};
	ImGuiApp::Texture* texture = ImGuiApp::CreateTexture(
		128,
		128,
		ImGuiApp::TextureFlagPreserveContents,
		ImGuiApp::TextureFilter::Nearest);
	bool texture_ready = false;

	if (texture != nullptr) {
		void* pixels = nullptr;
		int pitch = 0;
		if (ImGuiApp::LockTexture(texture, &pixels, &pitch)) {
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
			texture_ready = true;
		}
	}

	while (ImGuiApp::Loop()) {
		ImGui::Begin("Hello, world!");
		ImGui::Text("This is some useful text.");
		ImGui::InputText("Input", &text);

		const ImGuiApp::FileDialogFilter text_filters[] = {
			{"Text files", "txt;md"},
			{"All files", "*"},
		};

		if (ImGui::Button("open file")) {
			const ImGuiApp::FileDialogResult result = ImGuiApp::OpenFileDialog(text_filters, 2);
			file_dialog_status = DescribeDialogResult("Open", result);
		}
		ImGui::SameLine();
		if (ImGui::Button("save file")) {
			const ImGuiApp::FileDialogResult result = ImGuiApp::SaveFileDialog(text_filters, 2, "untitled.txt");
			file_dialog_status = DescribeDialogResult("Save", result);
		}
		ImGui::TextWrapped("%s", file_dialog_status.c_str());

		if (texture_ready) {
			ImGui::Image(ImGuiApp::GetTextureID(texture), ImVec2(128.0f, 128.0f));
		}
		if (ImGui::Button("exit")) {
			ImGuiApp::RequestQuit();
		}
		ImGui::End();
	}

	ImGuiApp::FreeTexture(texture);
	ImGuiApp::Shutdown();
}
