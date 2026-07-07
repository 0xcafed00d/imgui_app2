#include <SDL3/SDL.h>

#include "imgui_app.h"

#include <cstdint>
#include <limits>
#include <vector>

namespace ImGuiApp {
	namespace {
		constexpr unsigned int GL_TEXTURE_2D_VALUE = 0x0DE1;
		constexpr unsigned int GL_TEXTURE_BINDING_2D_VALUE = 0x8069;
		constexpr unsigned int GL_TEXTURE_MIN_FILTER_VALUE = 0x2801;
		constexpr unsigned int GL_TEXTURE_MAG_FILTER_VALUE = 0x2800;
		constexpr unsigned int GL_TEXTURE_WRAP_S_VALUE = 0x2802;
		constexpr unsigned int GL_TEXTURE_WRAP_T_VALUE = 0x2803;
		constexpr unsigned int GL_NEAREST_VALUE = 0x2600;
		constexpr unsigned int GL_LINEAR_VALUE = 0x2601;
		constexpr unsigned int GL_CLAMP_TO_EDGE_VALUE = 0x812F;
		constexpr unsigned int GL_RGBA_VALUE = 0x1908;
		constexpr unsigned int GL_UNSIGNED_BYTE_VALUE = 0x1401;
		constexpr unsigned int GL_UNPACK_ALIGNMENT_VALUE = 0x0CF5;

		using GlBindTextureProc = void (*)(unsigned int, unsigned int);
		using GlDeleteTexturesProc = void (*)(int, const unsigned int*);
		using GlGenTexturesProc = void (*)(int, unsigned int*);
		using GlGetIntegervProc = void (*)(unsigned int, int*);
		using GlPixelStoreiProc = void (*)(unsigned int, int);
		using GlTexImage2DProc = void (*)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);
		using GlTexParameteriProc = void (*)(unsigned int, unsigned int, int);
		using GlTexSubImage2DProc = void (*)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);

		struct OpenGLTextureProcs {
			GlBindTextureProc BindTexture{nullptr};
			GlDeleteTexturesProc DeleteTextures{nullptr};
			GlGenTexturesProc GenTextures{nullptr};
			GlGetIntegervProc GetIntegerv{nullptr};
			GlPixelStoreiProc PixelStorei{nullptr};
			GlTexImage2DProc TexImage2D{nullptr};
			GlTexParameteriProc TexParameteri{nullptr};
			GlTexSubImage2DProc TexSubImage2D{nullptr};
			bool loaded{false};
		};

		static OpenGLTextureProcs gl_texture_procs{};

		SDL_Renderer* current_renderer() {
			SDL_Window* current_window{nullptr};
			SDL_Renderer* current_renderer{nullptr};
			GetSDLWindowAndRenderer(&current_window, &current_renderer);
			return current_renderer;
		}

		bool make_opengl_current() {
			SDL_Window* current_window{nullptr};
			SDL_GLContext current_context{nullptr};
			GetSDLWindowAndGLContext(&current_window, &current_context);
			if (current_window == nullptr || current_context == nullptr) {
				return false;
			}

			return SDL_GL_MakeCurrent(current_window, current_context);
		}

		bool load_opengl_texture_procs() {
			if (gl_texture_procs.loaded) {
				return true;
			}

			gl_texture_procs.BindTexture = reinterpret_cast<GlBindTextureProc>(SDL_GL_GetProcAddress("glBindTexture"));
			gl_texture_procs.DeleteTextures = reinterpret_cast<GlDeleteTexturesProc>(SDL_GL_GetProcAddress("glDeleteTextures"));
			gl_texture_procs.GenTextures = reinterpret_cast<GlGenTexturesProc>(SDL_GL_GetProcAddress("glGenTextures"));
			gl_texture_procs.GetIntegerv = reinterpret_cast<GlGetIntegervProc>(SDL_GL_GetProcAddress("glGetIntegerv"));
			gl_texture_procs.PixelStorei = reinterpret_cast<GlPixelStoreiProc>(SDL_GL_GetProcAddress("glPixelStorei"));
			gl_texture_procs.TexImage2D = reinterpret_cast<GlTexImage2DProc>(SDL_GL_GetProcAddress("glTexImage2D"));
			gl_texture_procs.TexParameteri = reinterpret_cast<GlTexParameteriProc>(SDL_GL_GetProcAddress("glTexParameteri"));
			gl_texture_procs.TexSubImage2D = reinterpret_cast<GlTexSubImage2DProc>(SDL_GL_GetProcAddress("glTexSubImage2D"));

			gl_texture_procs.loaded =
				gl_texture_procs.BindTexture != nullptr &&
				gl_texture_procs.DeleteTextures != nullptr &&
				gl_texture_procs.GenTextures != nullptr &&
				gl_texture_procs.GetIntegerv != nullptr &&
				gl_texture_procs.PixelStorei != nullptr &&
				gl_texture_procs.TexImage2D != nullptr &&
				gl_texture_procs.TexParameteri != nullptr &&
				gl_texture_procs.TexSubImage2D != nullptr;

			if (!gl_texture_procs.loaded) {
				SDL_Log("ImGuiApp failed to load the required OpenGL texture functions.");
			}

			return gl_texture_procs.loaded;
		}

		SDL_ScaleMode to_sdl_scale_mode(TextureFilter filter) {
			switch (filter) {
				case TextureFilter::Nearest:
					return SDL_SCALEMODE_NEAREST;
				case TextureFilter::Linear:
					return SDL_SCALEMODE_LINEAR;
			}

			return SDL_SCALEMODE_LINEAR;
		}

		unsigned int to_gl_filter_value(TextureFilter filter) {
			switch (filter) {
				case TextureFilter::Nearest:
					return GL_NEAREST_VALUE;
				case TextureFilter::Linear:
					return GL_LINEAR_VALUE;
			}

			return GL_LINEAR_VALUE;
		}
	}  // namespace

	struct Texture {
		Backend backend{Backend::None};
		unsigned int flags{TextureFlagNone};
		TextureFilter filter{TextureFilter::Linear};
		int width{0};
		int height{0};
		int pitch{0};
		SDL_Texture* sdl_texture{nullptr};
		unsigned int gl_texture{0};
		std::vector<unsigned char> pixels{};
		bool locked{false};
	};

	Texture* CreateTexture(int width, int height, TextureFilter filter) {
		return CreateTexture(width, height, TextureFlagNone, filter);
	}

	Texture* CreateTexture(int width, int height, unsigned int flags, TextureFilter filter) {
		if (width <= 0 || height <= 0 || width > std::numeric_limits<int>::max() / 4) {
			SDL_Log("ImGuiApp::CreateTexture requires a positive RGBA texture size.");
			return nullptr;
		}

		const Backend active_backend = GetBackend();
		if (active_backend == Backend::None) {
			SDL_Log("ImGuiApp::CreateTexture requires ImGuiApp to be initialized.");
			return nullptr;
		}

		Texture* texture = new Texture{};
		texture->backend = active_backend;
		texture->flags = flags;
		texture->filter = filter;
		texture->width = width;
		texture->height = height;
		texture->pitch = width * 4;

		switch (active_backend) {
			case Backend::SDLRenderer: {
				SDL_Renderer* renderer = current_renderer();
				if (renderer == nullptr) {
					delete texture;
					return nullptr;
				}

				texture->sdl_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width, height);
				if (texture->sdl_texture == nullptr) {
					SDL_Log("ImGuiApp failed to create SDL texture: %s", SDL_GetError());
					delete texture;
					return nullptr;
				}

				SDL_SetTextureBlendMode(texture->sdl_texture, SDL_BLENDMODE_BLEND);
				SDL_SetTextureScaleMode(texture->sdl_texture, to_sdl_scale_mode(filter));
				if ((texture->flags & TextureFlagPreserveContents) != 0) {
					texture->pixels.resize(static_cast<std::size_t>(texture->pitch) * static_cast<std::size_t>(height));
					if (!SDL_UpdateTexture(texture->sdl_texture, nullptr, texture->pixels.data(), texture->pitch)) {
						SDL_Log("ImGuiApp failed to initialize SDL texture data: %s", SDL_GetError());
					}
				}
				return texture;
			}

			case Backend::OpenGL: {
				if (!make_opengl_current() || !load_opengl_texture_procs()) {
					delete texture;
					return nullptr;
				}

				texture->pixels.resize(static_cast<std::size_t>(texture->pitch) * static_cast<std::size_t>(height));

				int previous_texture = 0;
				gl_texture_procs.GetIntegerv(GL_TEXTURE_BINDING_2D_VALUE, &previous_texture);
				gl_texture_procs.GenTextures(1, &texture->gl_texture);
				if (texture->gl_texture == 0) {
					SDL_Log("ImGuiApp failed to create OpenGL texture.");
					delete texture;
					return nullptr;
				}

				gl_texture_procs.BindTexture(GL_TEXTURE_2D_VALUE, texture->gl_texture);
				const unsigned int gl_filter = to_gl_filter_value(filter);
				gl_texture_procs.TexParameteri(GL_TEXTURE_2D_VALUE, GL_TEXTURE_MIN_FILTER_VALUE, static_cast<int>(gl_filter));
				gl_texture_procs.TexParameteri(GL_TEXTURE_2D_VALUE, GL_TEXTURE_MAG_FILTER_VALUE, static_cast<int>(gl_filter));
				gl_texture_procs.TexParameteri(GL_TEXTURE_2D_VALUE, GL_TEXTURE_WRAP_S_VALUE, GL_CLAMP_TO_EDGE_VALUE);
				gl_texture_procs.TexParameteri(GL_TEXTURE_2D_VALUE, GL_TEXTURE_WRAP_T_VALUE, GL_CLAMP_TO_EDGE_VALUE);
				gl_texture_procs.PixelStorei(GL_UNPACK_ALIGNMENT_VALUE, 1);
				gl_texture_procs.TexImage2D(
					GL_TEXTURE_2D_VALUE,
					0,
					static_cast<int>(GL_RGBA_VALUE),
					width,
					height,
					0,
					GL_RGBA_VALUE,
					GL_UNSIGNED_BYTE_VALUE,
					texture->pixels.data());
				gl_texture_procs.BindTexture(GL_TEXTURE_2D_VALUE, static_cast<unsigned int>(previous_texture));
				return texture;
			}

			case Backend::None:
				break;
		}

		delete texture;
		return nullptr;
	}

	bool LockTexture(Texture* texture, void** pixels, int* pitch) {
		if (texture == nullptr || pixels == nullptr || pitch == nullptr) {
			SDL_Log("ImGuiApp::LockTexture requires a texture, pixels pointer, and pitch pointer.");
			return false;
		}

		if (texture->locked) {
			SDL_Log("ImGuiApp::LockTexture called on a texture that is already locked.");
			return false;
		}

		if (texture->backend != GetBackend()) {
			SDL_Log("ImGuiApp::LockTexture requires the texture to match the active backend.");
			return false;
		}

		switch (texture->backend) {
			case Backend::SDLRenderer:
				if ((texture->flags & TextureFlagPreserveContents) != 0) {
					if (texture->pixels.empty()) {
						SDL_Log("ImGuiApp SDL texture staging buffer is unavailable.");
						return false;
					}
					*pixels = texture->pixels.data();
					*pitch = texture->pitch;
					texture->locked = true;
					return true;
				}

				if (texture->sdl_texture == nullptr || !SDL_LockTexture(texture->sdl_texture, nullptr, pixels, pitch)) {
					SDL_Log("ImGuiApp failed to lock SDL texture: %s", SDL_GetError());
					return false;
				}
				texture->locked = true;
				return true;

			case Backend::OpenGL:
				if (texture->pixels.empty()) {
					SDL_Log("ImGuiApp OpenGL texture staging buffer is unavailable.");
					return false;
				}
				*pixels = texture->pixels.data();
				*pitch = texture->pitch;
				texture->locked = true;
				return true;

			case Backend::None:
				break;
		}

		return false;
	}

	void UnlockTexture(Texture* texture) {
		if (texture == nullptr || !texture->locked) {
			return;
		}

		switch (texture->backend) {
			case Backend::SDLRenderer:
				if (texture->sdl_texture != nullptr) {
					if ((texture->flags & TextureFlagPreserveContents) != 0) {
						if (!SDL_UpdateTexture(texture->sdl_texture, nullptr, texture->pixels.data(), texture->pitch)) {
							SDL_Log("ImGuiApp failed to upload SDL texture data: %s", SDL_GetError());
						}
					} else {
						SDL_UnlockTexture(texture->sdl_texture);
					}
				}
				break;

			case Backend::OpenGL:
				if (texture->gl_texture != 0 && make_opengl_current() && load_opengl_texture_procs()) {
					int previous_texture = 0;
					gl_texture_procs.GetIntegerv(GL_TEXTURE_BINDING_2D_VALUE, &previous_texture);
					gl_texture_procs.PixelStorei(GL_UNPACK_ALIGNMENT_VALUE, 1);
					gl_texture_procs.BindTexture(GL_TEXTURE_2D_VALUE, texture->gl_texture);
					gl_texture_procs.TexSubImage2D(
						GL_TEXTURE_2D_VALUE,
						0,
						0,
						0,
						texture->width,
						texture->height,
						GL_RGBA_VALUE,
						GL_UNSIGNED_BYTE_VALUE,
						texture->pixels.data());
					gl_texture_procs.BindTexture(GL_TEXTURE_2D_VALUE, static_cast<unsigned int>(previous_texture));
				} else {
					SDL_Log("ImGuiApp failed to upload OpenGL texture data.");
				}
				break;

			case Backend::None:
				break;
		}

		texture->locked = false;
	}

	void FreeTexture(Texture* texture) {
		if (texture == nullptr) {
			return;
		}

		if (texture->locked &&
			texture->backend == Backend::SDLRenderer &&
			(texture->flags & TextureFlagPreserveContents) == 0 &&
			texture->sdl_texture != nullptr) {
			SDL_UnlockTexture(texture->sdl_texture);
		}

		switch (texture->backend) {
			case Backend::SDLRenderer:
				if (texture->sdl_texture != nullptr) {
					SDL_DestroyTexture(texture->sdl_texture);
				}
				break;

			case Backend::OpenGL:
				if (texture->gl_texture != 0 && make_opengl_current() && load_opengl_texture_procs()) {
					gl_texture_procs.DeleteTextures(1, &texture->gl_texture);
				} else if (texture->gl_texture != 0) {
					SDL_Log("ImGuiApp could not delete OpenGL texture because no OpenGL context is current.");
				}
				break;

			case Backend::None:
				break;
		}

		delete texture;
	}

	ImTextureID GetTextureID(const Texture* texture) {
		if (texture == nullptr || texture->backend != GetBackend()) {
			return ImTextureID_Invalid;
		}

		switch (texture->backend) {
			case Backend::SDLRenderer:
				return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture->sdl_texture));
			case Backend::OpenGL:
				return static_cast<ImTextureID>(static_cast<std::uintptr_t>(texture->gl_texture));
			case Backend::None:
				break;
		}

		return ImTextureID_Invalid;
	}

}  // namespace ImGuiApp
