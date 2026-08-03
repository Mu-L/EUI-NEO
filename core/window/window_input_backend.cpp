#include "core/window/window_backend.h"

#if defined(EUI_WINDOW_BACKEND_SDL2)

namespace core::window {

void installInputCallbacks(Handle) {}
void uninstallInputCallbacks(Handle) {}
bool queryImeComposition(Handle, std::string&, bool&) { return false; }

} // namespace core::window

#else

#include "core/input/input_state.h"
#include "core/platform/ime_bridge.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

namespace core::window {

void installInputCallbacks(Handle window) {
    if (window == nullptr) {
        return;
    }

    auto* glfwWindow = static_cast<GLFWwindow*>(window);
    eui_ime_install_message_filter(glfwWindow);
    glfwSetCharCallback(glfwWindow, [](GLFWwindow* currentWindow, unsigned int codepoint) {
        core::detail::InputQueue& queue = core::detail::inputQueue(currentWindow);
        core::detail::appendUtf8(queue.text, codepoint);
        eui_ime_clear_composition(currentWindow);
        queue.compositionText.clear();
        queue.compositionChanged = true;
        core::detail::setComposing(currentWindow, false);
        core::detail::setCompositionText(currentWindow, {});
    });
    glfwSetScrollCallback(glfwWindow, [](GLFWwindow* currentWindow, double xoffset, double yoffset) {
        core::queueScrollInput(currentWindow, xoffset, yoffset);
    });
    glfwSetKeyCallback(glfwWindow, [](GLFWwindow* currentWindow, int key, int, int action, int mods) {
        if (action != GLFW_PRESS && action != GLFW_REPEAT) {
            return;
        }

        const bool ctrl = (mods & GLFW_MOD_CONTROL) != 0 || (mods & GLFW_MOD_SUPER) != 0;
        const bool shift = (mods & GLFW_MOD_SHIFT) != 0;
        core::detail::setComposing(currentWindow, eui_ime_is_composing(currentWindow) != 0);
        switch (key) {
        case GLFW_KEY_BACKSPACE: core::queueKeyInput(currentWindow, core::InputKey::Backspace, ctrl, shift); break;
        case GLFW_KEY_DELETE: core::queueKeyInput(currentWindow, core::InputKey::Delete, ctrl, shift); break;
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER: core::queueKeyInput(currentWindow, core::InputKey::Enter, ctrl, shift); break;
        case GLFW_KEY_LEFT: core::queueKeyInput(currentWindow, core::InputKey::Left, ctrl, shift); break;
        case GLFW_KEY_RIGHT: core::queueKeyInput(currentWindow, core::InputKey::Right, ctrl, shift); break;
        case GLFW_KEY_UP: core::queueKeyInput(currentWindow, core::InputKey::Up, ctrl, shift); break;
        case GLFW_KEY_DOWN: core::queueKeyInput(currentWindow, core::InputKey::Down, ctrl, shift); break;
        case GLFW_KEY_HOME: core::queueKeyInput(currentWindow, core::InputKey::Home, ctrl, shift); break;
        case GLFW_KEY_END: core::queueKeyInput(currentWindow, core::InputKey::End, ctrl, shift); break;
        case GLFW_KEY_ESCAPE: core::queueKeyInput(currentWindow, core::InputKey::Escape, ctrl, shift); break;
        case GLFW_KEY_A: core::queueKeyInput(currentWindow, core::InputKey::A, ctrl, shift); break;
        case GLFW_KEY_C: core::queueKeyInput(currentWindow, core::InputKey::C, ctrl, shift); break;
        case GLFW_KEY_V: core::queueKeyInput(currentWindow, core::InputKey::V, ctrl, shift); break;
        case GLFW_KEY_X: core::queueKeyInput(currentWindow, core::InputKey::X, ctrl, shift); break;
        case GLFW_KEY_Y: core::queueKeyInput(currentWindow, core::InputKey::Y, ctrl, shift); break;
        case GLFW_KEY_Z: core::queueKeyInput(currentWindow, core::InputKey::Z, ctrl, shift); break;
        default: break;
        }
    });
}

void uninstallInputCallbacks(Handle window) {
    if (window != nullptr) {
        eui_ime_uninstall_message_filter(static_cast<GLFWwindow*>(window));
    }
}

bool queryImeComposition(Handle window, std::string& text, bool& composing) {
#if defined(_WIN32) || defined(__APPLE__)
    char compositionBuffer[512]{};
    const int compositionLength = eui_ime_get_composition_string_utf8(
        static_cast<GLFWwindow*>(window),
        compositionBuffer,
        static_cast<int>(sizeof(compositionBuffer)));
    composing = compositionLength > 0;
    text = composing ? std::string(compositionBuffer) : std::string{};
    return true;
#else
    (void)window;
    (void)text;
    (void)composing;
    return false;
#endif
}

} // namespace core::window

#endif
