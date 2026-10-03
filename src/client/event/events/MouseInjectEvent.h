#pragma once
#include "client/event/Event.h"
#include "mc/common/client/game/MouseAction.h"
#include "util/Crypto.h"

class MouseInjectEvent : public Event {
public:
    static const uint32_t hash = TOHASH(MouseInjectEvent);

    // Never reallocates: the vector belongs to the game.
    bool push(SDK::MouseAction const& action) {
        if (inputs.size() >= inputs.capacity()) return false;
        inputs.push_back(action);
        return true;
    }

    explicit MouseInjectEvent(std::vector<SDK::MouseAction>& inputs)
        : inputs(inputs) {}

private:
    std::vector<SDK::MouseAction>& inputs;
};
