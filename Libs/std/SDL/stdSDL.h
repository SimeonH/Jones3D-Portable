#ifndef STD_SDL_STDSDL_H
#define STD_SDL_STDSDL_H
// Helpers of the SDL platform layer (non-Windows builds)
#include <j3dcore/j3d.h>

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>

J3D_EXTERN_C_START

// Returns the DirectInput key code (DIK_*) of an SDL scancode, 0 if there's none.
// The game uses DIK codes as key ids for controls and bindings.
uint8_t stdSDL_ScancodeToDIK(SDL_Scancode scancode);

// Returns the Win32 virtual key code (VK_*) of an SDL key, 0 if there's none
uint32_t stdSDL_KeycodeToVK(SDL_Keycode key);

// Sets the SDL GL attributes of the OpenGL ES 2.0 renderer, before an OpenGL window or context is created
void stdSDL_SetGLAttributes(void);

J3D_EXTERN_C_END
#endif // STD_SDL_STDSDL_H
