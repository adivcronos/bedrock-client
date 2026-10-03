#pragma once
#include "../../Module.h"
#include <chrono>
#include <random>

class AutoClicker : public Module {
public:
    AutoClicker();

private:
    using Clock = std::chrono::steady_clock;

    void onMouseInject(Event& ev);
    bool shouldClick();
    bool holdingWeapon();
    void scheduleNextClick(Clock::time_point from);

    static constexpr int mode_hold = 0;
    static constexpr int mode_always = 1;
    EnumData mode;

    static constexpr int button_left = 0;
    static constexpr int button_right = 1;
    EnumData button;

    ValueType cps = FloatValue(12.f);
    ValueType randomize = BoolValue(true);
    ValueType minCps = FloatValue(9.f);
    ValueType maxCps = FloatValue(14.f);
    ValueType jitter = FloatValue(15.f);
    ValueType pressMin = FloatValue(25.f);
    ValueType pressMax = FloatValue(60.f);
    ValueType dropChance = FloatValue(0.f);
    ValueType burstChance = FloatValue(0.f);
    ValueType onlyWeapons = BoolValue(false);
    ValueType breakBlocks = BoolValue(true);

    std::mt19937 rng { std::random_device {}() };
    Clock::time_point nextDown {};
    Clock::time_point nextUp {};
    bool pressed = false;
    int pressedButton = 0;
    int burstLeft = 0;
};
