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
        const core::KeyAction keyAction =
            action == GLFW_REPEAT ? core::KeyAction::Repeat : core::KeyAction::Press;
        const auto queueKey = [&](core::InputKey inputKey) {
            core::queueKeyInput(currentWindow, {inputKey, keyAction, {ctrl, shift}});
        };
        core::detail::setComposing(currentWindow, eui_ime_is_composing(currentWindow) != 0);
        switch (key) {
        case GLFW_KEY_BACKSPACE: queueKey(core::InputKey::Backspace); break;
        case GLFW_KEY_DELETE: queueKey(core::InputKey::Delete); break;
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER: queueKey(core::InputKey::Enter); break;
        case GLFW_KEY_LEFT: queueKey(core::InputKey::Left); break;
        case GLFW_KEY_RIGHT: queueKey(core::InputKey::Right); break;
        case GLFW_KEY_UP: queueKey(core::InputKey::Up); break;
        case GLFW_KEY_DOWN: queueKey(core::InputKey::Down); break;
        case GLFW_KEY_HOME: queueKey(core::InputKey::Home); break;
        case GLFW_KEY_END: queueKey(core::InputKey::End); break;
        case GLFW_KEY_ESCAPE: queueKey(core::InputKey::Escape); break;
        case GLFW_KEY_A: queueKey(core::InputKey::A); break;
        case GLFW_KEY_C: queueKey(core::InputKey::C); break;
        case GLFW_KEY_V: queueKey(core::InputKey::V); break;
        case GLFW_KEY_X: queueKey(core::InputKey::X); break;
        case GLFW_KEY_Y: queueKey(core::InputKey::Y); break;
        case GLFW_KEY_Z: queueKey(core::InputKey::Z); break;
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
