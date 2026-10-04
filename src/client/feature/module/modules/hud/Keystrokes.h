#pragma once
#include "../../HUDModule.h"

class Keystrokes : public HUDModule {
public:
    Keystrokes();

    void render(DrawUtil& dc, bool, bool) override;

private:
    ValueType mouseButtons = BoolValue(true);
    ValueType cps = BoolValue(true);
    ValueType spaceBar = BoolValue(true);
    ValueType border = BoolValue(false);
    ValueType shiftKey = BoolValue(false);

    // Layout and colours follow Flarial's Keystrokes (flarialmc/dll-oss, AGPL-3.0).
    ValueType textSize = FloatValue(20.f);
    ValueType keystrokeSize = FloatValue(60.f);
    ValueType padding = FloatValue(2.5f);
    ValueType borderLength = FloatValue(1.f);
    ValueType lerpSpeed = FloatValue(1.f);
    ValueType radius = FloatValue(4.5f);

    ValueType borderColor = ColorValue(0.f, 0.f, 0.f, 1.f);
    ValueType pressedColor = ColorValue(0.98f, 0.98f, 0.98f, 0.55f);
    ValueType unpressedColor = ColorValue(0.f, 0.f, 0.f, 0.55f);
    ValueType pressedTextColor = ColorValue(0.98f, 0.98f, 0.98f, 1.f);
    ValueType unpressedTextColor = ColorValue(0.98f, 0.98f, 0.98f, 1.f);

    typedef std::function<bool()> GetInputFunc;

    struct Stroke {
        d2d::Color col;
        d2d::Color textCol;
        std::wstring keyName;
        GetInputFunc getInput;

        Stroke(GetInputFunc getInput)
            : getInput(getInput) {}

        [[nodiscard]] bool get() const { return getInput(); }
    };

    struct Keystroke : public Stroke {
        std::string mapping;
        int vKey;

        Keystroke(std::string const& inputMapping, GetInputFunc getInput);

        void updateKeyName();
    };

    void drawKey(DrawUtil& dc, d2d::Rect const& rc, Stroke& stroke, std::wstring const& label,
                 std::wstring const& sub = L"");

    void onClick(Event& evG);

    bool primaryClickState = false;
    bool secondaryClickState = false;
};
