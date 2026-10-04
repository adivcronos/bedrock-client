#include "pch.h"
#include "AutoClicker.h"
#include "client/event/events/MouseInjectEvent.h"
#include "client/Latite.h"
#include "client/misc/ClickSource.h"
#include "client/misc/ClientMessageQueue.h"
#include "mc/common/client/game/ClientInstance.h"
#include "mc/common/client/game/MinecraftGame.h"
#include "mc/common/client/game/MouseDevice.h"
#include "mc/common/world/Item.h"
#include "mc/common/world/level/HitResult.h"
#include "mc/common/world/level/Level.h"
#include <set>

AutoClicker::AutoClicker()
    : Module("AutoClicker", L"Auto Clicker", L"Clicks for you. Built for testing your own server's anti-cheat.", GAME,
             0) {
    mode.addEntry({ mode_hold, L"Hold", L"Only clicks while you hold the mouse button" });
    mode.addEntry({ mode_always, L"Always", L"Clicks the whole time the module is on" });
    button.addEntry({ button_left, L"Left", L"Attack / break" });
    button.addEntry({ button_right, L"Right", L"Use / place" });

    preset.addEntry({ preset_custom, L"Custom", L"Use your own settings below" });
    preset.addEntry({ preset_human, L"Human-like", L"8-12 CPS, lots of variation" });
    preset.addEntry({ preset_borderline, L"Borderline", L"13-16 CPS, some variation and bursts" });
    preset.addEntry({ preset_blatant, L"Blatant", L"Fixed 20 CPS, no variation" });
    preset.addEntry({ preset_robot, L"Robot", L"Fixed 12 CPS, perfectly even" });
    auto presetSetting = addEnumSetting("preset", L"Preset", L"Load a ready-made set of clicker settings", preset);
    presetSetting->callback = [this](Setting&) {
        if (!applyingPreset) applyPreset(preset.getSelectedKey(), false);
    };
    addSetting("presetKey", L"Next preset key", L"Press to cycle through the presets", presetKey);

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
    listen<KeyUpdateEvent>(static_cast<EventListenerFunc>(&AutoClicker::onKey), true);

    // Editing any clicker value by hand switches the preset back to Custom.
    settings->forEach([this](std::shared_ptr<Setting> set) {
        static const std::set<std::string> unrelated = { "preset", "presetKey",   "enabled",    "key",
                                                         "mode",   "button",      "onlyWeapons", "breakBlocks" };
        if (unrelated.contains(set->name())) return;
        set->callback = [this](Setting&) {
            if (!applyingPreset) std::get<EnumValue>(*preset.getValue()).val = preset_custom;
        };
    });
}

void AutoClicker::loadConfig(SettingGroup& resolvedGroup) {
    applyingPreset = true;
    Module::loadConfig(resolvedGroup);
    applyingPreset = false;
}

void AutoClicker::applyPreset(int p, bool announce) {
    struct Values {
        bool rnd;
        float cps, lo, hi, jit, pLo, pHi, drop, burst;
    };
    Values v {};
    const wchar_t* name = L"Custom";
    switch (p) {
    case preset_human:
        v = { true, 10.f, 8.f, 12.f, 25.f, 35.f, 90.f, 3.f, 2.f };
        name = L"Human-like";
        break;
    case preset_borderline:
        v = { true, 14.f, 13.f, 16.f, 12.f, 25.f, 60.f, 1.f, 6.f };
        name = L"Borderline";
        break;
    case preset_blatant:
        v = { false, 20.f, 20.f, 20.f, 0.f, 20.f, 25.f, 0.f, 0.f };
        name = L"Blatant";
        break;
    case preset_robot:
        v = { false, 12.f, 12.f, 12.f, 0.f, 40.f, 40.f, 0.f, 0.f };
        name = L"Robot";
        break;
    default:
        return;
    }
    bool wasApplying = applyingPreset;
    applyingPreset = true;
    randomize = BoolValue(v.rnd);
    cps = FloatValue(v.cps);
    minCps = FloatValue(v.lo);
    maxCps = FloatValue(v.hi);
    jitter = FloatValue(v.jit);
    pressMin = FloatValue(v.pLo);
    pressMax = FloatValue(v.pHi);
    dropChance = FloatValue(v.drop);
    burstChance = FloatValue(v.burst);
    applyingPreset = wasApplying;
    std::get<EnumValue>(*preset.getValue()).val = p;

    if (announce) Latite::getClientMessageQueue().push(std::wstring(L"Auto Clicker preset: ") + name);
}

void AutoClicker::onKey(Event& evGeneric) {
    auto& ev = reinterpret_cast<KeyUpdateEvent&>(evGeneric);
    if (!ev.isDown() || ev.inUI()) return;
    int key = std::get<KeyValue>(presetKey);
    if (key == 0 || ev.getKey() != key) return;
    int next = preset.getSelectedKey() % (preset_count - 1) + 1;
    applyPreset(next, true);
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
    ClickSource::noteInjected(btn);

    float pLo = std::get<FloatValue>(pressMin), pHi = std::get<FloatValue>(pressMax);
    if (pLo > pHi) std::swap(pLo, pHi);
    float pressMs = std::uniform_real_distribution<float>(pLo, pHi)(rng);
    nextUp = now + std::chrono::microseconds(static_cast<long long>(pressMs * 1000.f));
    if (nextUp >= nextDown) nextUp = nextDown - std::chrono::milliseconds(5);
    pressed = true;
    pressedButton = btn;
}
