#include "pch.h"
#include "AutoClicker.h"
#include "client/event/events/MouseInjectEvent.h"
#include "mc/common/client/game/ClientInstance.h"
#include "mc/common/client/game/MinecraftGame.h"
#include "mc/common/client/game/MouseDevice.h"
#include "mc/common/world/Item.h"
#include "mc/common/world/level/HitResult.h"
#include "mc/common/world/level/Level.h"

AutoClicker::AutoClicker()
    : Module("AutoClicker", L"Auto Clicker", L"Clicks for you. Built for testing your own server's anti-cheat.", GAME,
             0) {
    mode.addEntry({ mode_hold, L"Hold", L"Only clicks while you hold the mouse button" });
    mode.addEntry({ mode_always, L"Always", L"Clicks the whole time the module is on" });
    button.addEntry({ button_left, L"Left", L"Attack / break" });
    button.addEntry({ button_right, L"Right", L"Use / place" });

    addEnumSetting("mode", L"Mode", L"When to click", mode);
    addEnumSetting("button", L"Button", L"Which mouse button to click", button);
    addSetting("randomize", L"Randomize", L"Pick a random CPS between Min and Max for every click", randomize);
    addSliderSetting("cps", L"CPS", L"Clicks per second", cps, FloatValue(1.f), FloatValue(30.f), FloatValue(1.f),
                     "randomize"_isfalse);
    addSliderSetting("minCps", L"Min CPS", L"Lowest clicks per second", minCps, FloatValue(1.f), FloatValue(30.f),
                     FloatValue(1.f), "randomize"_istrue);
    addSliderSetting("maxCps", L"Max CPS", L"Highest clicks per second", maxCps, FloatValue(1.f), FloatValue(30.f),
                     FloatValue(1.f), "randomize"_istrue);
    addSliderSetting("jitter", L"Timing jitter %", L"Random wobble added to the gap between clicks", jitter,
                     FloatValue(0.f), FloatValue(50.f), FloatValue(1.f));
    addSliderSetting("pressMin", L"Press time min (ms)", L"Shortest time the button is held down per click", pressMin,
                     FloatValue(5.f), FloatValue(150.f), FloatValue(1.f));
    addSliderSetting("pressMax", L"Press time max (ms)", L"Longest time the button is held down per click", pressMax,
                     FloatValue(5.f), FloatValue(150.f), FloatValue(1.f));
    addSliderSetting("dropChance", L"Drop chance %", L"Chance to skip a click", dropChance, FloatValue(0.f),
                     FloatValue(30.f), FloatValue(1.f));
    addSliderSetting("burstChance", L"Burst chance %", L"Chance to start a short burst of faster clicks",
                     burstChance, FloatValue(0.f), FloatValue(30.f), FloatValue(1.f));
    addSetting("onlyWeapons", L"Only weapons", L"Only click while holding a sword, axe, mace or trident",
               onlyWeapons);
    addSetting("breakBlocks", L"Break blocks", L"Pause left-clicking while aiming at a block so you can mine",
               breakBlocks);

    // Runs while disabled too, so a click in progress always gets its release.
    listen<MouseInjectEvent>(static_cast<EventListenerFunc>(&AutoClicker::onMouseInject), true);
}

bool AutoClicker::holdingWeapon() {
    auto* player = SDK::ClientInstance::get()->getLocalPlayer();
    if (!player || !player->supplies || !player->supplies->inventory) return false;
    auto* stack = player->supplies->inventory->getItem(player->supplies->selectedSlot);
    if (!stack || !stack->getItem()) return false;
    std::string name = stack->getItem()->translateName;
    for (auto kw : { "sword", "_axe", "mace", "trident" })
        if (name.find(kw) != std::string::npos) return true;
    return false;
}

bool AutoClicker::shouldClick() {
    auto* ci = SDK::ClientInstance::get();
    if (!ci || !ci->getLocalPlayer() || !ci->minecraftGame || !ci->minecraftGame->isCursorGrabbed()) return false;

    bool left = button.getSelectedKey() == button_left;
    if (mode.getSelectedKey() == mode_hold && !(GetAsyncKeyState(left ? VK_LBUTTON : VK_RBUTTON) & 0x8000))
        return false;

    if (std::get<BoolValue>(onlyWeapons) && !holdingWeapon()) return false;

    if (left && std::get<BoolValue>(breakBlocks) && ci->minecraft) {
        auto* level = ci->minecraft->getLevel();
        if (level && level->getHitResult() && level->getHitResult()->hitType == SDK::HitType::BLOCK) return false;
    }
    return true;
}

void AutoClicker::scheduleNextClick(Clock::time_point from) {
    float lo = std::get<FloatValue>(cps);
    float hi = lo;
    if (std::get<BoolValue>(randomize)) {
        lo = std::get<FloatValue>(minCps);
        hi = std::get<FloatValue>(maxCps);
        if (lo > hi) std::swap(lo, hi);
    }
    float target = std::uniform_real_distribution<float>(lo, hi)(rng);

    if (burstLeft > 0) {
        burstLeft--;
        target *= 1.35f;
    } else if (std::uniform_real_distribution<float>(0.f, 100.f)(rng) < std::get<FloatValue>(burstChance)) {
        burstLeft = std::uniform_int_distribution<int>(2, 5)(rng);
    }

    float intervalMs = 1000.f / (std::max)(target, 0.5f);
    float spread = intervalMs * std::get<FloatValue>(jitter) / 100.f;
    if (spread > 0.f) intervalMs += std::normal_distribution<float>(0.f, spread / 2.f)(rng);
    intervalMs = std::clamp(intervalMs, 20.f, 2000.f);

    nextDown = from + std::chrono::microseconds(static_cast<long long>(intervalMs * 1000.f));
}

void AutoClicker::onMouseInject(Event& evGeneric) {
    auto& ev = reinterpret_cast<MouseInjectEvent&>(evGeneric);
    auto* mouse = SDK::MouseDevice::get();
    if (!mouse) return;
    auto now = Clock::now();

    auto makeAction = [&](int btn, bool down) {
        SDK::MouseAction a {};
        a.x = mouse->x;
        a.y = mouse->y;
        a.action = static_cast<int8_t>(btn);
        a.data = down ? 1 : 0;
        return a;
    };

    if (pressed) {
        if ((now >= nextUp || !isEnabled()) && ev.push(makeAction(pressedButton, false))) pressed = false;
        return;
    }
    if (!isEnabled()) return;

    if (!shouldClick()) {
        nextDown = now;
        burstLeft = 0;
        return;
    }
    if (now < nextDown) return;

    scheduleNextClick(now);
    if (std::uniform_real_distribution<float>(0.f, 100.f)(rng) < std::get<FloatValue>(dropChance)) return;

    int btn = button.getSelectedKey() == button_left ? 1 : 2;
    if (!ev.push(makeAction(btn, true))) return;

    float pLo = std::get<FloatValue>(pressMin), pHi = std::get<FloatValue>(pressMax);
    if (pLo > pHi) std::swap(pLo, pHi);
    float pressMs = std::uniform_real_distribution<float>(pLo, pHi)(rng);
    nextUp = now + std::chrono::microseconds(static_cast<long long>(pressMs * 1000.f));
    if (nextUp >= nextDown) nextUp = nextDown - std::chrono::milliseconds(5);
    pressed = true;
    pressedButton = btn;
}
