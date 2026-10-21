#include "stdSDL.h"

#include <std/General/stdUtil.h>

#include <SDL3/SDL_video.h>

typedef struct sScancodeDIK
{
    SDL_Scancode scancode;
    uint8_t dik;
} ScancodeDIK;

static const ScancodeDIK stdSDL_aScancodes[] = {
    { SDL_SCANCODE_ESCAPE, DIK_ESCAPE },
    { SDL_SCANCODE_1, DIK_1 }, { SDL_SCANCODE_2, DIK_2 }, { SDL_SCANCODE_3, DIK_3 }, { SDL_SCANCODE_4, DIK_4 }, { SDL_SCANCODE_5, DIK_5 },
    { SDL_SCANCODE_6, DIK_6 }, { SDL_SCANCODE_7, DIK_7 }, { SDL_SCANCODE_8, DIK_8 }, { SDL_SCANCODE_9, DIK_9 }, { SDL_SCANCODE_0, DIK_0 },
    { SDL_SCANCODE_MINUS, DIK_MINUS },
    { SDL_SCANCODE_EQUALS, DIK_EQUALS },
    { SDL_SCANCODE_BACKSPACE, DIK_BACK },
    { SDL_SCANCODE_TAB, DIK_TAB },
    { SDL_SCANCODE_Q, DIK_Q }, { SDL_SCANCODE_W, DIK_W }, { SDL_SCANCODE_E, DIK_E }, { SDL_SCANCODE_R, DIK_R }, { SDL_SCANCODE_T, DIK_T },
    { SDL_SCANCODE_Y, DIK_Y }, { SDL_SCANCODE_U, DIK_U }, { SDL_SCANCODE_I, DIK_I }, { SDL_SCANCODE_O, DIK_O }, { SDL_SCANCODE_P, DIK_P },
    { SDL_SCANCODE_LEFTBRACKET, DIK_LBRACKET },
    { SDL_SCANCODE_RIGHTBRACKET, DIK_RBRACKET },
    { SDL_SCANCODE_RETURN, DIK_RETURN },
    { SDL_SCANCODE_LCTRL, DIK_LCONTROL },
    { SDL_SCANCODE_A, DIK_A }, { SDL_SCANCODE_S, DIK_S }, { SDL_SCANCODE_D, DIK_D }, { SDL_SCANCODE_F, DIK_F }, { SDL_SCANCODE_G, DIK_G },
    { SDL_SCANCODE_H, DIK_H }, { SDL_SCANCODE_J, DIK_J }, { SDL_SCANCODE_K, DIK_K }, { SDL_SCANCODE_L, DIK_L },
    { SDL_SCANCODE_SEMICOLON, DIK_SEMICOLON },
    { SDL_SCANCODE_APOSTROPHE, DIK_APOSTROPHE },
    { SDL_SCANCODE_GRAVE, DIK_GRAVE },
    { SDL_SCANCODE_LSHIFT, DIK_LSHIFT },
    { SDL_SCANCODE_BACKSLASH, DIK_BACKSLASH },
    { SDL_SCANCODE_Z, DIK_Z }, { SDL_SCANCODE_X, DIK_X }, { SDL_SCANCODE_C, DIK_C }, { SDL_SCANCODE_V, DIK_V }, { SDL_SCANCODE_B, DIK_B },
    { SDL_SCANCODE_N, DIK_N }, { SDL_SCANCODE_M, DIK_M },
    { SDL_SCANCODE_COMMA, DIK_COMMA },
    { SDL_SCANCODE_PERIOD, DIK_PERIOD },
    { SDL_SCANCODE_SLASH, DIK_SLASH },
    { SDL_SCANCODE_RSHIFT, DIK_RSHIFT },
    { SDL_SCANCODE_KP_MULTIPLY, DIK_MULTIPLY },
    { SDL_SCANCODE_LALT, DIK_LMENU },
    { SDL_SCANCODE_SPACE, DIK_SPACE },
    { SDL_SCANCODE_CAPSLOCK, DIK_CAPITAL },
    { SDL_SCANCODE_F1, DIK_F1 }, { SDL_SCANCODE_F2, DIK_F2 }, { SDL_SCANCODE_F3, DIK_F3 }, { SDL_SCANCODE_F4, DIK_F4 }, { SDL_SCANCODE_F5, DIK_F5 },
    { SDL_SCANCODE_F6, DIK_F6 }, { SDL_SCANCODE_F7, DIK_F7 }, { SDL_SCANCODE_F8, DIK_F8 }, { SDL_SCANCODE_F9, DIK_F9 }, { SDL_SCANCODE_F10, DIK_F10 },
    { SDL_SCANCODE_F11, DIK_F11 }, { SDL_SCANCODE_F12, DIK_F12 }, { SDL_SCANCODE_F13, DIK_F13 }, { SDL_SCANCODE_F14, DIK_F14 }, { SDL_SCANCODE_F15, DIK_F15 },
    { SDL_SCANCODE_NUMLOCKCLEAR, DIK_NUMLOCK },
    { SDL_SCANCODE_SCROLLLOCK, DIK_SCROLL },
    { SDL_SCANCODE_KP_7, DIK_NUMPAD7 }, { SDL_SCANCODE_KP_8, DIK_NUMPAD8 }, { SDL_SCANCODE_KP_9, DIK_NUMPAD9 },
    { SDL_SCANCODE_KP_MINUS, DIK_SUBTRACT },
    { SDL_SCANCODE_KP_4, DIK_NUMPAD4 }, { SDL_SCANCODE_KP_5, DIK_NUMPAD5 }, { SDL_SCANCODE_KP_6, DIK_NUMPAD6 },
    { SDL_SCANCODE_KP_PLUS, DIK_ADD },
    { SDL_SCANCODE_KP_1, DIK_NUMPAD1 }, { SDL_SCANCODE_KP_2, DIK_NUMPAD2 }, { SDL_SCANCODE_KP_3, DIK_NUMPAD3 }, { SDL_SCANCODE_KP_0, DIK_NUMPAD0 },
    { SDL_SCANCODE_KP_PERIOD, DIK_DECIMAL },
    { SDL_SCANCODE_NONUSBACKSLASH, DIK_OEM_102 },
    { SDL_SCANCODE_KP_EQUALS, DIK_NUMPADEQUALS },
    { SDL_SCANCODE_KP_COMMA, DIK_NUMPADCOMMA },
    { SDL_SCANCODE_KP_ENTER, DIK_NUMPADENTER },
    { SDL_SCANCODE_RCTRL, DIK_RCONTROL },
    { SDL_SCANCODE_KP_DIVIDE, DIK_DIVIDE },
    { SDL_SCANCODE_PRINTSCREEN, DIK_SYSRQ },
    { SDL_SCANCODE_RALT, DIK_RMENU },
    { SDL_SCANCODE_PAUSE, DIK_PAUSE },
    { SDL_SCANCODE_HOME, DIK_HOME },
    { SDL_SCANCODE_UP, DIK_UP },
    { SDL_SCANCODE_PAGEUP, DIK_PRIOR },
    { SDL_SCANCODE_LEFT, DIK_LEFT },
    { SDL_SCANCODE_RIGHT, DIK_RIGHT },
    { SDL_SCANCODE_END, DIK_END },
    { SDL_SCANCODE_DOWN, DIK_DOWN },
    { SDL_SCANCODE_PAGEDOWN, DIK_NEXT },
    { SDL_SCANCODE_INSERT, DIK_INSERT },
    { SDL_SCANCODE_DELETE, DIK_DELETE },
    { SDL_SCANCODE_LGUI, DIK_LWIN },
    { SDL_SCANCODE_RGUI, DIK_RWIN },
    { SDL_SCANCODE_APPLICATION, DIK_APPS },
};

uint8_t stdSDL_ScancodeToDIK(SDL_Scancode scancode)
{
    for ( size_t i = 0; i < STD_ARRAYLEN(stdSDL_aScancodes); ++i )
    {
        if ( stdSDL_aScancodes[i].scancode == scancode ) {
            return stdSDL_aScancodes[i].dik;
        }
    }

    return 0;
}

uint32_t stdSDL_KeycodeToVK(SDL_Keycode key)
{
    if ( key >= SDLK_A && key <= SDLK_Z ) {
        return 'A' + (key - SDLK_A);
    }

    if ( key >= SDLK_0 && key <= SDLK_9 ) {
        return '0' + (key - SDLK_0);
    }

    if ( key >= SDLK_F1 && key <= SDLK_F12 ) {
        return VK_F1 + (key - SDLK_F1);
    }

    switch ( key )
    {
        case SDLK_BACKSPACE: return VK_BACK;
        case SDLK_TAB:       return VK_TAB;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:  return VK_RETURN;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:    return VK_SHIFT;
        case SDLK_LCTRL:
        case SDLK_RCTRL:     return VK_CONTROL;
        case SDLK_LALT:
        case SDLK_RALT:      return VK_MENU;
        case SDLK_PAUSE:     return VK_PAUSE;
        case SDLK_CAPSLOCK:  return VK_CAPITAL;
        case SDLK_ESCAPE:    return VK_ESCAPE;
        case SDLK_SPACE:     return VK_SPACE;
        case SDLK_PAGEUP:    return VK_PRIOR;
        case SDLK_PAGEDOWN:  return VK_NEXT;
        case SDLK_END:       return VK_END;
        case SDLK_HOME:      return VK_HOME;
        case SDLK_LEFT:      return VK_LEFT;
        case SDLK_UP:        return VK_UP;
        case SDLK_RIGHT:     return VK_RIGHT;
        case SDLK_DOWN:      return VK_DOWN;
        case SDLK_PRINTSCREEN: return VK_SNAPSHOT;
        case SDLK_INSERT:    return VK_INSERT;
        case SDLK_DELETE:    return VK_DELETE;
        default:             return 0;
    }
}

void stdSDL_SetGLAttributes(void)
{
    // The depth buffer is in the renderer's offscreen back buffer
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
}
