#pragma once
#include <array>

// The AutoClicker records each press it injects; the next press ClickEvent for
// that button is then known to be synthetic. Both run on the input thread.
namespace ClickSource {
    inline std::array<int, 3> pendingInjected {};

    inline void noteInjected(int button) {
        if (button >= 1 && button <= 2) pendingInjected[button]++;
    }

    inline bool consumeInjected(int button) {
        if (button < 1 || button > 2 || pendingInjected[button] <= 0) return false;
        pendingInjected[button]--;
        return true;
    }
}
