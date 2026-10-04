#include "pch.h"
#include "Keystrokes.h"
#include "client/input/Keyboard.h"
#include <array>
#include <client/Latite.h>

Keystrokes::Keystrokes()
    : HUDModule("Keystrokes", LocalizeString::get("client.hudmodule.keystrokes.name"),
                LocalizeString::get("client.hudmodule.keystrokes.desc"), HUD) {
    enabled = BoolValue(true);
    addSetting("mouseButtons", LocalizeString::get("client.hudmodule.keystrokes.mouseButtons.name"),
               LocalizeString::get("client.hudmodule.keystrokes.mouseButtons.desc"), mouseButtons);
    addSetting("showCps", LocalizeString::get("client.hudmodule.keystrokes.showCps.name"),
               LocalizeString::get("client.hudmodule.keystrokes.showCps.desc"), cps);
    addSetting("spaceBar", LocalizeString::get("client.hudmodule.keystrokes.spaceBar.name"),
               LocalizeString::get("client.hudmodule.keystrokes.spaceBar.desc"), spaceBar);
    addSetting("border", LocalizeString::get("client.hudmodule.keystrokes.border.name"),
               LocalizeString::get("client.hudmodule.keystrokes.border.desc"), border);
    addSetting("showSneak", LocalizeString::get("client.hudmodule.keystrokes.showSneak.name"),
               LocalizeString::get("client.hudmodule.keystrokes.showSneak.desc"), shiftKey);

    addSliderSetting("radius", LocalizeString::get("client.hudmodule.keystrokes.radius.name"),
                     LocalizeString::get("client.hudmodule.keystrokes.radius.desc"), radius, FloatValue(0.f),
                     FloatValue(10.f), FloatValue(1.f));
    addSliderSetting("textSize", LocalizeString::get("client.hudmodule.keystrokes.textSize.name"),
                     LocalizeString::get("client.hudmodule.keystrokes.textSize.desc"), textSize, FloatValue(2.f),
                     FloatValue(40.f), FloatValue(0.2f));
    addSliderSetting("keySize", LocalizeString::get("client.hudmodule.keystrokes.keySize.name"),
                     LocalizeString::get("client.hudmodule.keystrokes.keySize.desc"), keystrokeSize, FloatValue(15.f),
                     FloatValue(90.f), FloatValue(2.f));
    addSliderSetting("padding", LocalizeString::get("client.hudmodule.keystrokes.padding.name"),
                     LocalizeString::get("client.hudmodule.keystrokes.padding.desc"), padding, FloatValue(0.f),
                     FloatValue(6.f), FloatValue(0.25f));
    addSliderSetting("borderLength", LocalizeString::get("client.hudmodule.keystrokes.borderLength.name"),
                     LocalizeString::get("client.hudmodule.keystrokes.borderLength.desc"), borderLength,
                     FloatValue(0.f), FloatValue(6.f), FloatValue(0.25f), "border"_istrue);
    addSliderSetting("transition", LocalizeString::get("client.hudmodule.keystrokes.transition.name"),
                     LocalizeString::get("client.hudmodule.keystrokes.transition.desc"), lerpSpeed, FloatValue(0.f),
                     FloatValue(3.f), FloatValue(0.05f));

    addSetting("borderCol", LocalizeString::get("client.hudmodule.keystrokes.borderCol.name"),
               LocalizeString::get("client.hudmodule.keystrokes.borderCol.desc"), borderColor);
    addSetting("pressedCol", LocalizeString::get("client.hudmodule.keystrokes.pressedCol.name"),
               LocalizeString::get("client.hudmodule.keystrokes.pressedCol.desc"), pressedColor);
    addSetting("unpressedCol", LocalizeString::get("client.hudmodule.keystrokes.unpressedCol.name"),
               LocalizeString::get("client.hudmodule.keystrokes.unpressedCol.desc"), unpressedColor);
    addSetting("ptCol", LocalizeString::get("client.hudmodule.keystrokes.ptCol.name"),
               LocalizeString::get("client.hudmodule.keystrokes.ptCol.desc"), pressedTextColor);
    addSetting("uptCol", LocalizeString::get("client.hudmodule.keystrokes.uptCol.name"),
               LocalizeString::get("client.hudmodule.keystrokes.uptCol.desc"), unpressedTextColor);

    listen<ClickEvent>((EventListenerFunc)&Keystrokes::onClick);
}

void Keystrokes::drawKey(DrawUtil& dc, d2d::Rect const& rc, Stroke& stroke, std::wstring const& label,
                         std::wstring const& sub) {
    float rad = (std::min)(std::get<FloatValue>(radius).value, (std::min)(rc.getWidth(), rc.getHeight()) / 2.f);
    dc.fillRoundedRectangle(rc, stroke.col, rad);
    if (std::get<BoolValue>(border))
        dc.drawRoundedRectangle(rc, std::get<ColorValue>(borderColor).getMainColor(), rad,
                                std::get<FloatValue>(borderLength));

    float ts = std::get<FloatValue>(textSize);
    if (sub.empty()) {
        dc.drawText(rc, label, stroke.textCol, Renderer::FontSelection::PrimaryRegular, ts,
                    DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        return;
    }
    // Flarial puts the label in the top 65% and the CPS line in the bottom 35%.
    d2d::Rect top = { rc.left, rc.top, rc.right, rc.top + rc.getHeight() * 0.65f };
    d2d::Rect bottom = { rc.left, rc.top + rc.getHeight() * 0.55f, rc.right, rc.bottom - rc.getHeight() * 0.05f };
    dc.drawText(top, label, stroke.textCol, Renderer::FontSelection::PrimaryRegular, ts, DWRITE_TEXT_ALIGNMENT_CENTER,
                DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    dc.drawText(bottom, sub, stroke.textCol, Renderer::FontSelection::PrimaryRegular, ts * 0.6f,
                DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

void Keystrokes::onClick(Event& evG) {
    auto& ev = reinterpret_cast<ClickEvent&>(evG);

    if (ev.getMouseButton() == 1) {
        primaryClickState = ev.isDown();
    } else if (ev.getMouseButton() == 2) {
        secondaryClickState = ev.isDown();
    }
}

void Keystrokes::render(DrawUtil& dc, bool, bool inEditor) {
    // W, S, A, D keys
    // + sneak, space, LMB, RMB

    // I fucking hate this, but Mojang decided to turn the input states into bits...
#define CREATE_GET(field) \
    [&] -> bool {         \
        return field;     \
    }

    static std::array<Stroke, 2> mouseButtons = { Stroke(CREATE_GET(primaryClickState)),
                                                  Stroke(CREATE_GET(secondaryClickState)) };

    static std::array<Keystroke, 6> keystrokes = {
        Keystroke("forward",
                  CREATE_GET(SDK::ClientInstance::get()->getLocalPlayer()->getMoveInputComponent()->rawInputState.up)),
        Keystroke(
            "left",
            CREATE_GET(SDK::ClientInstance::get()->getLocalPlayer()->getMoveInputComponent()->rawInputState.left)),
        Keystroke(
            "back",
            CREATE_GET(SDK::ClientInstance::get()->getLocalPlayer()->getMoveInputComponent()->rawInputState.down)),
        Keystroke(
            "right",
            CREATE_GET(SDK::ClientInstance::get()->getLocalPlayer()->getMoveInputComponent()->rawInputState.right)),
        Keystroke(
            "sneak",
            CREATE_GET(SDK::ClientInstance::get()->getLocalPlayer()->getMoveInputComponent()->rawInputState.sneakDown)),
        Keystroke(
            "jump",
            CREATE_GET(SDK::ClientInstance::get()->getLocalPlayer()->getMoveInputComponent()->rawInputState.jumpDown))
    };

#undef CREATE_GET

    float ls = std::get<FloatValue>(lerpSpeed);
    float lerpT = SDK::ClientInstance::get()->minecraft->timer->alpha * ls;

    for (auto& key : keystrokes) {
        if (ls > 0.01f) {
            key.col = util::LerpColorState(key.col, d2d::Color(std::get<ColorValue>(this->pressedColor).getMainColor()),
                                           d2d::Color(std::get<ColorValue>(this->unpressedColor).getMainColor()),
                                           key.get(), lerpT);
            key.textCol = util::LerpColorState(
                key.textCol, d2d::Color(std::get<ColorValue>(this->pressedTextColor).getMainColor()),
                d2d::Color(std::get<ColorValue>(this->unpressedTextColor).getMainColor()), key.get(), lerpT);
        } else {
            key.col = key.get() ? d2d::Color(std::get<ColorValue>(this->pressedColor).getMainColor())
                                : d2d::Color(std::get<ColorValue>(this->unpressedColor).getMainColor());
            key.textCol = key.get() ? d2d::Color(std::get<ColorValue>(this->pressedTextColor).getMainColor())
                                    : d2d::Color(std::get<ColorValue>(this->unpressedTextColor).getMainColor());
        }
    }

    for (auto& btn : mouseButtons) {
        if (ls > 0.01f) {
            btn.col = util::LerpColorState(btn.col, d2d::Color(std::get<ColorValue>(this->pressedColor).getMainColor()),
                                           d2d::Color(std::get<ColorValue>(this->unpressedColor).getMainColor()),
                                           btn.get(), lerpT);
            btn.textCol = util::LerpColorState(
                btn.textCol, d2d::Color(std::get<ColorValue>(this->pressedTextColor).getMainColor()),
                d2d::Color(std::get<ColorValue>(this->unpressedTextColor).getMainColor()), btn.get(), lerpT);
        } else {
            btn.col = btn.get() ? d2d::Color(std::get<ColorValue>(this->pressedColor).getMainColor())
                                : d2d::Color(std::get<ColorValue>(this->unpressedColor).getMainColor());
            btn.textCol = btn.get() ? d2d::Color(std::get<ColorValue>(this->pressedTextColor).getMainColor())
                                    : d2d::Color(std::get<ColorValue>(this->unpressedTextColor).getMainColor());
        }
    }

    // Flarial layout: W / A S D / LMB RMB / space bar
    float key = std::get<FloatValue>(keystrokeSize);
    float gap = std::get<FloatValue>(padding);
    float row = key + gap;
    float width = key * 3.f + gap * 2.f;
    float y = 0.f;

    drawKey(dc, { row, y, row + key, y + key }, keystrokes[0], keystrokes[0].keyName);
    y += row;
    drawKey(dc, { 0.f, y, key, y + key }, keystrokes[1], keystrokes[1].keyName);
    drawKey(dc, { row, y, row + key, y + key }, keystrokes[2], keystrokes[2].keyName);
    drawKey(dc, { row * 2.f, y, row * 2.f + key, y + key }, keystrokes[3], keystrokes[3].keyName);
    y += row;

    if (std::get<BoolValue>(this->mouseButtons)) {
        float mbW = (width - gap) / 2.f;
        float mbH = key * 0.95f;
        bool showCps = std::get<BoolValue>(cps);
        std::wstring lCps = showCps ? std::to_wstring(inEditor ? 0 : Latite::get().getTimings().getCPSL()) + L" CPS" : L"";
        std::wstring rCps = showCps ? std::to_wstring(inEditor ? 0 : Latite::get().getTimings().getCPSR()) + L" CPS" : L"";
        drawKey(dc, { 0.f, y, mbW, y + mbH }, mouseButtons[0], L"LMB", lCps);
        drawKey(dc, { mbW + gap, y, width, y + mbH }, mouseButtons[1], L"RMB", rCps);
        y += mbH + gap;
    }

    if (std::get<BoolValue>(spaceBar)) {
        float spH = key * 0.55f;
        d2d::Rect sp = { 0.f, y, width, y + spH };
        Stroke& jump = keystrokes[5];
        float rad = (std::min)(std::get<FloatValue>(radius).value, spH / 2.f);
        dc.fillRoundedRectangle(sp, jump.col, rad);
        if (std::get<BoolValue>(border))
            dc.drawRoundedRectangle(sp, std::get<ColorValue>(borderColor).getMainColor(), rad,
                                    std::get<FloatValue>(borderLength));
        float barW = width * 0.5f;
        float barH = (std::max)(spH * 0.09f, 1.f);
        float cx = (width - barW) / 2.f;
        float cy = y + (spH - barH) / 2.f;
        dc.fillRectangle({ cx, cy, cx + barW, cy + barH }, jump.textCol);
        y += spH + gap;
    }

    if (std::get<BoolValue>(shiftKey)) {
        float shH = key * 0.55f;
        drawKey(dc, { 0.f, y, width, y + shH }, keystrokes[4], keystrokes[4].keyName);
        y += shH + gap;
    }

    this->rect.right = rect.left + width;
    this->rect.bottom = rect.top + (std::max)(y - gap, 0.f);
};

Keystrokes::Keystroke::Keystroke(std::string const& inputMapping, GetInputFunc getInput)
    : Stroke(getInput)
    , mapping(inputMapping) {
    vKey = Latite::getKeyboard().getMappedKey(inputMapping);
    keyName = util::StrToWStr(util::KeyToString(vKey));
}

void Keystrokes::Keystroke::updateKeyName() {
    vKey = Latite::getKeyboard().getMappedKey(mapping);
    keyName = util::StrToWStr(util::KeyToString(vKey));
}
