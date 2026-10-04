#pragma once
#include "../../TextModule.h"
#include <chrono>
#include <deque>
#include <fstream>

class ClickStats : public TextModule {
public:
    ClickStats();

    std::wstringstream text(bool isDefault, bool inEditor) override;
    void onDisable() override;

private:
    using Clock = std::chrono::steady_clock;

    void onClick(Event& ev);
    void writeLog(Clock::time_point t, double gapMs, int button, bool injected);
    void closeLog();

    static constexpr int button_left = 0;
    static constexpr int button_right = 1;
    EnumData button;
    ValueType sampleSize = FloatValue(40.f);
    ValueType logToFile = BoolValue(false);

    struct Sample {
        Clock::time_point t;
        bool injected;
    };
    std::deque<Sample> samples;
    Clock::time_point lastAny {};
    bool haveLastAny = false;

    std::ofstream log;
    Clock::time_point logStart {};
    int logLines = 0;
};
