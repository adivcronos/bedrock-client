#include "pch.h"
#include "ClickStats.h"
#include "client/misc/ClickSource.h"
#include "util/Util.h"
#include <cmath>
#include <ctime>
#include <iomanip>
#include <mutex>

namespace {
    std::mutex samplesMutex;
}

ClickStats::ClickStats()
    : TextModule("ClickStats", L"Click Stats",
                 L"Shows CPS, the gap between clicks, how much it varies, and how many clicks were automatic", HUD,
                 900.f) {
    button.addEntry({ button_left, L"Left", L"Track left clicks" });
    button.addEntry({ button_right, L"Right", L"Track right clicks" });

    addEnumSetting("button", L"Button", L"Which mouse button to measure", button);
    addSliderSetting("sampleSize", L"Clicks to average", L"How many recent clicks the stats are based on", sampleSize,
                     FloatValue(10.f), FloatValue(200.f), FloatValue(5.f));
    auto logSetting = addSetting("logToFile", L"Log clicks to file",
                                 L"Save every click to a CSV in the Latite/ClickLogs folder", logToFile);
    logSetting->callback = [this](Setting&) {
        if (!std::get<BoolValue>(logToFile)) closeLog();
    };

    storedPos = Vec2Value(0.f, 0.34f);
    enabled = BoolValue(true);

    // Runs while disabled too, so injected presses are always matched up.
    listen<ClickEvent>(static_cast<EventListenerFunc>(&ClickStats::onClick), true);
}

void ClickStats::onDisable() {
    closeLog();
    std::lock_guard lock(samplesMutex);
    samples.clear();
    haveLastAny = false;
}

void ClickStats::onClick(Event& evGeneric) {
    auto& ev = reinterpret_cast<ClickEvent&>(evGeneric);
    int btn = ev.getMouseButton();
    if ((btn != 1 && btn != 2) || !ev.isDown()) return;

    bool injected = ClickSource::consumeInjected(btn);
    if (!isEnabled()) return;
    if (btn != (button.getSelectedKey() == button_left ? 1 : 2)) return;

    auto now = Clock::now();
    double gapMs = haveLastAny ? std::chrono::duration<double, std::milli>(now - lastAny).count() : 0.0;
    lastAny = now;
    haveLastAny = true;

    {
        std::lock_guard lock(samplesMutex);
        samples.push_back({ now, injected });
        auto limit = static_cast<size_t>(std::get<FloatValue>(sampleSize));
        while (samples.size() > limit) samples.pop_front();
    }

    if (std::get<BoolValue>(logToFile)) writeLog(now, gapMs, btn, injected);
}

void ClickStats::writeLog(Clock::time_point t, double gapMs, int btn, bool injected) {
    if (!log.is_open()) {
        auto dir = util::GetLatitePath() / "ClickLogs";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);

        std::time_t wall = std::time(nullptr);
        std::tm tm {};
        localtime_s(&tm, &wall);
        char name[64];
        std::strftime(name, sizeof(name), "clicks-%Y%m%d-%H%M%S.csv", &tm);

        log.open(dir / name);
        if (!log.is_open()) return;
        log << "time_s,gap_ms,button,source\n";
        logStart = t;
        logLines = 0;
    }
    double ts = std::chrono::duration<double>(t - logStart).count();
    log << std::fixed << std::setprecision(4) << ts << ',' << std::setprecision(2) << gapMs << ','
        << (btn == 1 ? "L" : "R") << ',' << (injected ? "auto" : "real") << '\n';
    if (++logLines % 20 == 0) log.flush();
}

void ClickStats::closeLog() {
    if (log.is_open()) log.close();
}

std::wstringstream ClickStats::text(bool isDefault, bool inEditor) {
    std::wstringstream wss;
    if (isDefault) {
        wss << L"CPS 12 | gap 83\u00B114ms (61-112) | auto 100%";
        return wss;
    }

    std::vector<Sample> copy;
    {
        std::lock_guard lock(samplesMutex);
        copy.assign(samples.begin(), samples.end());
    }

    auto now = Clock::now();
    int cps = 0;
    int autoCount = 0;
    for (auto& s : copy) {
        if (now - s.t <= std::chrono::seconds(1)) cps++;
        if (s.injected) autoCount++;
    }

    // Gaps over a second are pauses, not part of a clicking run.
    std::vector<double> gaps;
    for (size_t i = 1; i < copy.size(); i++) {
        double g = std::chrono::duration<double, std::milli>(copy[i].t - copy[i - 1].t).count();
        if (g < 1000.0) gaps.push_back(g);
    }

    wss << L"CPS " << cps;
    if (gaps.empty()) {
        wss << L" | no clicks yet";
        return wss;
    }

    double mean = 0.0;
    for (double g : gaps) mean += g;
    mean /= gaps.size();
    double var = 0.0;
    for (double g : gaps) var += (g - mean) * (g - mean);
    double sd = std::sqrt(var / gaps.size());
    auto [mn, mx] = std::minmax_element(gaps.begin(), gaps.end());

    wss << L" | gap " << std::lround(mean) << L"\u00B1" << std::lround(sd) << L"ms (" << std::lround(*mn) << L"-"
        << std::lround(*mx) << L") | auto " << (copy.empty() ? 0 : autoCount * 100 / static_cast<int>(copy.size()))
        << L"%";
    return wss;
}
