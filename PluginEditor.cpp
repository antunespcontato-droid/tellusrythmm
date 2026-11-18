#include "PluginEditor.h"
#include "../Engine/PercEngine.h"

#if PSY_HAS_LOGO
 #include <BinaryData.h>
#endif

using namespace ps;
using juce::Colour; using juce::Rectangle; using juce::String;

namespace ui
{
    static const Colour bg (0xff0e1117), panel (0xff171c26), panel2 (0xff202838), line (0xff2c3548), text (0xffe6eaf2), dim (0xff8a93a6),
                        teal (0xff21d4c4), magenta (0xffe9498c), amber (0xffffb020), violet (0xff8c7bff);
}

// ============================================================================ Look & Feel
class PsyLnF : public juce::LookAndFeel_V4
{
public:
    PsyLnF()
    {
        setColour (juce::Slider::textBoxTextColourId, ui::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, ui::text);
        setColour (juce::ComboBox::backgroundColourId, ui::panel2);
        setColour (juce::ComboBox::outlineColourId, ui::line);
        setColour (juce::ComboBox::textColourId, ui::text);
        setColour (juce::ComboBox::arrowColourId, ui::dim);
        setColour (juce::PopupMenu::backgroundColourId, ui::panel);
        setColour (juce::PopupMenu::textColourId, ui::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::teal.darker (0.5f));
        setColour (juce::TextButton::buttonColourId, ui::panel2);
        setColour (juce::TextButton::textColourOffId, ui::text);
        setColour (juce::TextButton::textColourOnId, ui::bg);
        setColour (juce::AlertWindow::backgroundColourId, ui::panel);
        setColour (juce::AlertWindow::textColourId, ui::text);
        setColour (juce::TextEditor::backgroundColourId, ui::panel2);
        setColour (juce::TextEditor::textColourId, ui::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.f);
        const float r = std::min (b.getWidth(), b.getHeight()) / 2.f - 2.f;
        const auto c = b.getCentre();
        juce::Path track; track.addCentredArc (c.x, c.y, r, r, 0.f, a0, a1, true);
        g.setColour (ui::line); g.strokePath (track, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float ang = a0 + pos * (a1 - a0);
        juce::Path val; val.addCentredArc (c.x, c.y, r, r, 0.f, a0, ang, true);
        g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (val, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (ui::text);
        g.fillEllipse (c.x + (r - 10.f) * std::sin (ang) - 2.5f, c.y - (r - 10.f) * std::cos (ang) - 2.5f, 5.f, 5.f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hl, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.f);
        const bool on = b.getToggleState();
        g.setColour (on ? b.findColour (juce::ToggleButton::tickColourId) : ui::panel2);
        g.fillRoundedRectangle (r, 5.f);
        g.setColour (hl ? ui::text.withAlpha (0.6f) : ui::line); g.drawRoundedRectangle (r, 5.f, 1.f);
        g.setColour (on ? ui::bg : ui::text); g.setFont (juce::FontOptions (12.f, juce::Font::bold));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const Colour& bc, bool hl, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.f);
        auto col = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : bc;
        if (hl) col = col.brighter (0.15f); if (down) col = col.brighter (0.3f);
        g.setColour (col); g.fillRoundedRectangle (r, 5.f);
        g.setColour (ui::line); g.drawRoundedRectangle (r, 5.f, 1.f);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (12.f, juce::Font::bold)); }
};

static void drawPanel (juce::Graphics& g, Rectangle<int> r, const String& title, Colour accent)
{
    g.setColour (ui::panel); g.fillRoundedRectangle (r.toFloat(), 8.f);
    g.setColour (ui::line); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.f, 1.f);
    g.setColour (accent); g.fillRoundedRectangle ((float) r.getX() + 10.f, (float) r.getY() + 12.f, 3.f, 12.f, 1.5f);
    g.setColour (ui::text); g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText (title, r.getX() + 18, r.getY() + 8, r.getWidth() - 24, 20, juce::Justification::centredLeft);
}
static Rectangle<int> inner (Rectangle<int> r) { return r.reduced (10, 8).withTrimmedTop (24); }

// ============================================================================ Controles ligados a parâmetros
class ParamKnob : public juce::Component
{
public:
    ParamKnob (PsyProcessor& p, int idx, const String& label, Colour c) : name (label)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 14);
        slider.setColour (juce::Slider::rotarySliderFillColourId, c);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        const auto& d = PsyProcessor::def (idx);
        if (d.kind == KFloat) slider.setNumDecimalPlacesToDisplay (2);
        addAndMakeVisible (slider);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, d.id, slider);
        slider.setDoubleClickReturnValue (true, d.def);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (ui::dim); g.setFont (juce::FontOptions (11.f, juce::Font::bold));
        g.drawText (name, getLocalBounds().removeFromTop (14), juce::Justification::centred);
    }
    void resized() override { slider.setBounds (getLocalBounds().withTrimmedTop (13)); }
private:
    String name; juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class ParamCombo : public juce::Component
{
public:
    ParamCombo (PsyProcessor& p, int idx, const String& label) : name (label)
    {
        const auto& d = PsyProcessor::def (idx);
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (d.id))) combo.addItemList (cp->choices, 1);
        addAndMakeVisible (combo);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, d.id, combo);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (ui::dim); g.setFont (juce::FontOptions (11.f, juce::Font::bold));
        g.drawText (name, getLocalBounds().removeFromTop (14), juce::Justification::centredLeft);
    }
    void resized() override { combo.setBounds (getLocalBounds().withTrimmedTop (15).withHeight (24)); }
private:
    String name; juce::ComboBox combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

class ParamToggle : public juce::Component
{
public:
    ParamToggle (PsyProcessor& p, int idx, const String& label, Colour c)
    {
        button.setButtonText (label); button.setColour (juce::ToggleButton::tickColourId, c);
        addAndMakeVisible (button);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, PsyProcessor::def (idx).id, button);
    }
    void resized() override { button.setBounds (getLocalBounds()); }
private:
    juce::ToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// slider linear compacto (para CC map)
class ParamBar : public juce::Component
{
public:
    ParamBar (PsyProcessor& p, int idx, const String& suffixLabel)
    {
        slider.setSliderStyle (juce::Slider::LinearBar); slider.setTextBoxStyle (juce::Slider::TextBoxLeft, true, 0, 0);
        slider.setColour (juce::Slider::trackColourId, ui::violet.withAlpha (0.55f)); slider.setColour (juce::Slider::textBoxTextColourId, ui::text);
        slider.setTextValueSuffix (suffixLabel);
        if (PsyProcessor::def (idx).kind == KFloat) slider.setNumDecimalPlacesToDisplay (2);
        addAndMakeVisible (slider);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, PsyProcessor::def (idx).id, slider);
        slider.setDoubleClickReturnValue (true, PsyProcessor::def (idx).def);
    }
    void resized() override { slider.setBounds (getLocalBounds()); }
private:
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// botão "segurar" (momentâneo) ligado a um parâmetro booleano – performance em tempo real
class HoldButton : public juce::TextButton
{
public:
    HoldButton (PsyProcessor& p, int idx, const String& label) : proc (p), pidx (idx) { setButtonText (label); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (auto* prm = proc.apvts.getParameter (PsyProcessor::def (pidx).id)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (1.f); held = true; }
        juce::TextButton::mouseDown (e);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (held) if (auto* prm = proc.apvts.getParameter (PsyProcessor::def (pidx).id)) { prm->setValueNotifyingHost (0.f); prm->endChangeGesture(); }
        held = false; juce::TextButton::mouseUp (e);
    }
private:
    PsyProcessor& proc; int pidx; bool held = false;
};

// ============================================================================ Grade de passos
class StepGrid : public juce::Component, private juce::Timer
{
public:
    enum Prop { Vel, Gate, Prob, Pitch, Ratchet, Pan };

    StepGrid (PsyProcessor& p, int whichPattern, Colour c, bool compactMode)
        : proc (p), which (whichPattern), accent (c), compact (compactMode) { startTimerHz (20); }

    void setProp (Prop pr) { prop = pr; repaint(); }
    int selected = -1;
    std::function<void()> onSelect;

    int length() const { return clampi ((int) proc.getParam (which < 0 ? (int) pSeqLen : laneParam (which, lpLen)), 1, kMaxSteps); }

    void paint (juce::Graphics& g) override
    {
        const auto pat = proc.copyPattern (which);
        const int N = length();
        const float cw = (float) getWidth() / (float) N;
        const int cellH = compact ? getHeight() : (int) (getHeight() * 0.40f);
        const bool running = proc.uiRunning.load();
        const int cur = which < 0 ? proc.uiMainStep.load() : proc.uiLaneStep[(size_t) which].load();
        int ls = 0, le = N - 1;
        if (which < 0) { ls = clampi ((int) proc.getParam (pSeqStart) - 1, 0, N - 1); le = clampi ((int) proc.getParam (pSeqEnd) - 1, 0, N - 1); if (ls > le) std::swap (ls, le); }

        for (int i = 0; i < N; ++i)
        {
            const float x = (float) i * cw;
            const auto cell = Rectangle<float> (x + 1.f, 1.f, cw - 2.f, (float) cellH - 2.f);
            const Step& st = pat.s[i];
            g.setColour (((i / 4) % 2) ? ui::panel2 : ui::panel2.darker (0.25f)); g.fillRoundedRectangle (cell, 3.f);
            if (st.mode == 1)
            {
                const float f = 0.25f + 0.75f * (float) st.vel / 127.f;
                g.setColour (accent.withAlpha (0.35f)); g.fillRoundedRectangle (cell, 3.f);
                g.setColour (accent.withAlpha (f)); g.fillRoundedRectangle (cell.withTrimmedTop (cell.getHeight() * (1.f - f)), 3.f);
                if (st.accent) { g.setColour (ui::text); g.fillEllipse (cell.getCentreX() - 2.5f, cell.getY() + 3.f, 5.f, 5.f); }
            }
            else if (st.mode == 2)
            {
                g.setColour (accent); g.drawRoundedRectangle (cell.reduced (1.f), 3.f, 1.6f);
                if (! compact) { g.setFont (juce::FontOptions (11.f, juce::Font::bold)); g.drawText (which < 0 ? "G" : "V", cell.toNearestInt(), juce::Justification::centred); }
            }
            if (which < 0 && (i < ls || i > le)) { g.setColour (ui::bg.withAlpha (0.55f)); g.fillRoundedRectangle (cell, 3.f); }
            if (running && i == cur) { g.setColour (juce::Colours::white.withAlpha (0.28f)); g.fillRoundedRectangle (cell, 3.f); g.setColour (juce::Colours::white); g.drawRoundedRectangle (cell, 3.f, 1.5f); }
            if (i == selected) { g.setColour (ui::amber); g.drawRoundedRectangle (cell.expanded (0.5f), 3.f, 1.5f); }
            if (! compact && i % 4 == 0) { g.setColour (ui::dim); g.setFont (juce::FontOptions (10.f)); g.drawText (String (i + 1), (int) x, cellH, (int) cw, 12, juce::Justification::centredLeft); }
        }
        if (compact) return;

        // área de barras (propriedade selecionada)
        const auto bars = barArea();
        g.setColour (ui::panel2.darker (0.3f)); g.fillRoundedRectangle (bars.toFloat(), 4.f);
        for (int i = 0; i < N; ++i)
        {
            const Step& st = pat.s[i]; float v = 0.f, base = 0.f;
            switch (prop)
            {
                case Vel:     v = (float) st.vel / 127.f; break;
                case Gate:    v = st.gate / 2.f; break;
                case Prob:    v = st.prob; break;
                case Pitch:   v = ((float) st.pitch + 24.f) / 48.f; base = 0.5f; break;
                case Ratchet: v = (float) st.ratchet / 4.f; break;
                case Pan:     v = (st.pan + 1.f) * 0.5f; base = 0.5f; break;
            }
            const float x = (float) i * cw, h = (float) bars.getHeight();
            const float y0 = (float) bars.getY() + h * (1.f - base), y1 = (float) bars.getY() + h * (1.f - v);
            g.setColour (accent.withAlpha (st.mode == 0 ? 0.25f : 0.9f));
            g.fillRect (Rectangle<float> (x + 2.f, std::min (y0, y1), cw - 4.f, std::max (2.f, std::abs (y1 - y0))));
        }
        if (prop == Pitch || prop == Pan) { g.setColour (ui::line); g.drawHorizontalLine (bars.getY() + bars.getHeight() / 2, 0.f, (float) getWidth()); }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragIdx = -1; dragIsBar = barArea().contains (e.getPosition());
        const int i = indexAt (e.x); if (i < 0) return;
        selected = i; if (onSelect) onSelect();
        if (e.mods.isPopupMenu()) { repaint(); return; }
        if (dragIsBar) { setPropFromY (i, e.y); dragIdx = i; return; }
        auto pat = proc.copyPattern (which); const auto& st = pat.s[i];
        if (e.mods.isAltDown()) { const bool ac = ! st.accent; proc.editPattern (which, [=] (Pattern& p) { p.s[i].accent = ac; if (ac && p.s[i].mode == 0) p.s[i].mode = 1; }); paintMode = -1; }
        else if (e.mods.isShiftDown()) { paintMode = st.mode == 2 ? 0 : 2; apply (i); }
        else { paintMode = st.mode == 0 ? 1 : 0; apply (i); }
        dragIdx = i; repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.x); if (i < 0) return;
        if (dragIsBar) { setPropFromY (i, e.y); return; }
        if (i != dragIdx && paintMode >= 0) { apply (i); dragIdx = i; selected = i; if (onSelect) onSelect(); repaint(); }
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int i = indexAt (e.x); if (i < 0) return;
        const int d = w.deltaY > 0 ? 6 : -6;
        proc.editPattern (which, [=] (Pattern& p) { p.s[i].vel = (uint8_t) clampi ((int) p.s[i].vel + d, 1, 127); });
        repaint();
    }

private:
    PsyProcessor& proc; int which; Colour accent; bool compact; Prop prop = Vel;
    int dragIdx = -1, paintMode = 0; bool dragIsBar = false;

    void timerCallback() override { if (isShowing()) repaint(); }
    Rectangle<int> barArea() const { if (compact) return {}; const int top = (int) (getHeight() * 0.40f) + 14; return { 0, top, getWidth(), std::max (10, getHeight() - top) }; }
    int indexAt (int x) const { const int N = length(); const int i = (int) std::floor ((float) x / ((float) getWidth() / (float) N)); return (i >= 0 && i < N) ? i : -1; }
    void apply (int i) { const int m = paintMode; proc.editPattern (which, [=] (Pattern& p) { p.s[i].mode = (uint8_t) m; }); }
    void setPropFromY (int i, int y)
    {
        const auto b = barArea(); const float f = clampf (1.f - (float) (y - b.getY()) / (float) b.getHeight(), 0.f, 1.f); const Prop pr = prop;
        proc.editPattern (which, [=] (Pattern& p)
        {
            Step& s = p.s[i];
            switch (pr)
            {
                case Vel:     s.vel = (uint8_t) clampi (1 + (int) std::lround (f * 126.f), 1, 127); break;
                case Gate:    s.gate = clampf (0.05f + f * 1.95f, 0.05f, 2.f); break;
                case Prob:    s.prob = clampf (f, 0.f, 1.f); break;
                case Pitch:   s.pitch = (int8_t) clampi ((int) std::lround (-24.f + f * 48.f), -24, 24); break;
                case Ratchet: s.ratchet = (uint8_t) clampi (1 + (int) std::lround (f * 3.f), 1, 4); break;
                case Pan:     s.pan = clampf (f * 2.f - 1.f, -1.f, 1.f); if (std::abs (s.pan) < 0.06f) s.pan = 0.f; break;
            }
        });
        repaint();
    }
};

// seletor de propriedade para o editor de barras
class PropSelector : public juce::Component
{
public:
    PropSelector (StepGrid& g, bool withPan, Colour c) : grid (g)
    {
        std::vector<std::pair<const char*, StepGrid::Prop>> items { { "VEL", StepGrid::Vel }, { "GATE", StepGrid::Gate }, { "PROB", StepGrid::Prob }, { "PITCH", StepGrid::Pitch }, { "RATCH", StepGrid::Ratchet } };
        if (withPan) items.push_back ({ "PAN", StepGrid::Pan });
        for (auto& it : items)
        {
            auto* b = buttons.add (new juce::TextButton (it.first));
            b->setClickingTogglesState (true); b->setRadioGroupId (777 + (int) (size_t) &g % 997); b->setColour (juce::TextButton::buttonOnColourId, c);
            const auto pr = it.second; b->onClick = [this, pr] { grid.setProp (pr); };
            addAndMakeVisible (b);
        }
        buttons[0]->setToggleState (true, juce::dontSendNotification);
    }
    void resized() override { auto r = getLocalBounds(); const int w = r.getWidth() / buttons.size(); for (auto* b : buttons) b->setBounds (r.removeFromLeft (w).reduced (1)); }
private:
    StepGrid& grid; juce::OwnedArray<juce::TextButton> buttons;
};

// teclado de 12 notas para a escala custom
class MaskKeys : public juce::Component, private juce::Timer
{
public:
    explicit MaskKeys (PsyProcessor& p) : proc (p)
    {
        static const char* nm[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < 12; ++i)
        {
            auto* b = keys.add (new juce::TextButton (nm[i])); b->setClickingTogglesState (true); b->setColour (juce::TextButton::buttonOnColourId, ui::teal);
            b->onClick = [this, i] { int m = (int) proc.getParam (pCustomMask); if (keys[i]->getToggleState()) m |= (1 << i); else m &= ~(1 << i); if (m == 0) m = 1; proc.setParamReal (pCustomMask, (float) m); };
            addAndMakeVisible (b);
        }
        startTimerHz (8); timerCallback();
    }
    void resized() override { auto r = getLocalBounds(); const int w = r.getWidth() / 12; for (auto* b : keys) b->setBounds (r.removeFromLeft (w).reduced (1)); }
private:
    PsyProcessor& proc; juce::OwnedArray<juce::TextButton> keys;
    void timerCallback() override { const int m = (int) proc.getParam (pCustomMask); for (int i = 0; i < 12; ++i) keys[i]->setToggleState ((m >> i) & 1, juce::dontSendNotification); }
};

// ============================================================================ Páginas
class PageBase : public juce::Component
{
public:
    explicit PageBase (PsyProcessor& p) : proc (p) {}
    PsyProcessor& proc;
    juce::OwnedArray<juce::Component> owned;

    template <class T, class... A> T* make (A&&... a) { auto* c = new T (std::forward<A> (a)...); owned.add (c); addAndMakeVisible (c); return c; }
    ParamKnob* knob (int idx, const String& l, Colour c) { return make<ParamKnob> (proc, idx, l, c); }
    ParamCombo* combo (int idx, const String& l) { return make<ParamCombo> (proc, idx, l); }
    ParamToggle* toggle (int idx, const String& l, Colour c) { return make<ParamToggle> (proc, idx, l, c); }

    static void placeRow (Rectangle<int> area, std::initializer_list<juce::Component*> comps)
    {
        const int n = (int) comps.size(); if (n == 0) return; const int w = area.getWidth() / n; int i = 0;
        for (auto* c : comps) { c->setBounds (area.getX() + i * w, area.getY(), w, area.getHeight()); ++i; }
    }
    static void placeGrid (Rectangle<int> area, std::vector<juce::Component*> comps, int cols, int rowH)
    {
        const int w = area.getWidth() / cols; int i = 0;
        for (auto* c : comps) { c->setBounds (area.getX() + (i % cols) * w, area.getY() + (i / cols) * rowH, w, rowH); ++i; }
    }
};

// ----------------------------------------------------------------------------- GENERATOR
class GeneratorPage : public PageBase
{
public:
    explicit GeneratorPage (PsyProcessor& p) : PageBase (p)
    {
        pitch = { combo (pMode, "MODE"), combo (pScale, "SCALE"), combo (pRoot, "ROOT"), combo (pShape, "SHAPE"), combo (pDirection, "DIRECTION") };
        keyFreeze = toggle (pKeyFreeze, "KEY FREEZE", ui::teal); keys = make<MaskKeys> (p);
        melody = { knob (pMelodyProb, "PROB", ui::teal), knob (pSteps, "STEPS", ui::teal), knob (pOctRange, "OCT RANGE", ui::teal), knob (pOctChance, "OCT CHANCE", ui::teal), knob (pChance, "CHANCE", ui::teal), knob (pExtend, "EXTEND", ui::teal) };
        poly = { knob (pChordBlend, "FUSION", ui::teal), knob (pFlux, "FLUX", ui::teal), knob (pSpread, "SPREAD", ui::teal), knob (pVoices, "VOICES", ui::teal), knob (pStackProb, "POLY STACK", ui::teal) };
        dyn = { knob (pVelocity, "VELOCITY", ui::amber), knob (pVelRand, "VEL RND", ui::amber), knob (pAccentAmt, "ACCENT", ui::amber), knob (pGate, "GATE", ui::amber), knob (pGateRand, "GATE RND", ui::amber), knob (pPbRand, "PITCH-BEND", ui::amber), knob (pIntensity, "INTENSITY", ui::amber) };
        loopOn = toggle (pLoopOn, "LOOP", ui::violet); loopLen = combo (pLoopLen, "LENGTH");
        const int num[4] = { pCc1Num, pCc2Num, pCc3Num, pCc4Num }, src[4] = { pCc1Src, pCc2Src, pCc3Src, pCc4Src }, amt[4] = { pCc1Amt, pCc2Amt, pCc3Amt, pCc4Amt };
        for (int i = 0; i < 4; ++i) { ccNum[i] = make<ParamBar> (p, num[i], " CC#"); ccSrc[i] = make<ParamCombo> (p, src[i], ""); ccAmt[i] = make<ParamBar> (p, amt[i], " amt"); }
    }
    void paint (juce::Graphics& g) override
    {
        drawPanel (g, rPitch, "PITCH & SCALE", ui::teal); drawPanel (g, rMelody, "MELODY", ui::teal); drawPanel (g, rPoly, "POLY / CHORDS", ui::teal);
        drawPanel (g, rDyn, "DYNAMICS", ui::amber); drawPanel (g, rLoop, "LOOP", ui::violet); drawPanel (g, rCc, "CC MAP  (randomness -> synth parameters)", ui::violet);
    }
    void resized() override
    {
        auto a = getLocalBounds().reduced (14, 8); auto row1 = a.removeFromTop (270); a.removeFromTop (14); auto row2 = a.removeFromTop (240);
        rPitch = row1.removeFromLeft (340); row1.removeFromLeft (16); rMelody = row1.removeFromLeft (300); row1.removeFromLeft (16); rPoly = row1;
        auto c = inner (rPitch); auto top = c.removeFromTop (46);
        placeRow (top, { pitch[0], pitch[1], pitch[2] }); c.removeFromTop (4); auto mid = c.removeFromTop (46); placeRow (mid, { pitch[3], pitch[4] });
        c.removeFromTop (8); keyFreeze->setBounds (c.removeFromTop (28).removeFromLeft (150)); c.removeFromTop (6); keys->setBounds (c.removeFromTop (28));
        placeGrid (inner (rMelody), { melody[0], melody[1], melody[2], melody[3], melody[4], melody[5] }, 3, 100);
        placeGrid (inner (rPoly), { poly[0], poly[1], poly[2], poly[3], poly[4] }, 5, 110);
        rDyn = row2.removeFromLeft (590); row2.removeFromLeft (16); rLoop = row2.removeFromLeft (200); row2.removeFromLeft (16); rCc = row2;
        placeGrid (inner (rDyn).withHeight (110), { dyn[0], dyn[1], dyn[2], dyn[3], dyn[4], dyn[5], dyn[6] }, 7, 110);
        auto l = inner (rLoop); loopOn->setBounds (l.removeFromTop (34)); l.removeFromTop (10); loopLen->setBounds (l.removeFromTop (44));
        auto cc = inner (rCc);
        for (int i = 0; i < 4; ++i) { auto r = cc.removeFromTop (30).reduced (0, 2); ccNum[i]->setBounds (r.removeFromLeft (150)); r.removeFromLeft (8); ccSrc[i]->setBounds (r.removeFromLeft (130).withY (r.getY() - 15).withHeight (40)); r.removeFromLeft (8); ccAmt[i]->setBounds (r); }
    }
private:
    std::vector<ParamCombo*> pitch; ParamToggle* keyFreeze; MaskKeys* keys; std::vector<ParamKnob*> melody, poly, dyn;
    ParamToggle* loopOn; ParamCombo* loopLen; ParamBar* ccNum[4]; ParamCombo* ccSrc[4]; ParamBar* ccAmt[4];
    Rectangle<int> rPitch, rMelody, rPoly, rDyn, rLoop, rCc;
};

// ----------------------------------------------------------------------------- RHYTHM & SEQUENCER
class RhythmPage : public PageBase
{
public:
    explicit RhythmPage (PsyProcessor& p) : PageBase (p)
    {
        src = combo (pSource, "RHYTHM SOURCE"); res = combo (pRes, "RESOLUTION"); spd = combo (pSpeed, "SPEED"); trip = toggle (pTriplet, "TRIPLETS", ui::teal);
        k = { knob (pSwing, "SHUFFLE", ui::teal), knob (pHuman, "HUMAN", ui::teal), knob (pDensity, "DENSITY", ui::teal), knob (pGroove8, "GROOVE 1/8", ui::teal), knob (pGroove4, "GROOVE 1/4", ui::teal), knob (pGroove2, "GROOVE 1/2", ui::teal), knob (pGlitch, "GLITCH", ui::teal) };
        sk = { knob (pSeqLen, "LENGTH", ui::amber), knob (pSeqStart, "LOOP START", ui::amber), knob (pSeqEnd, "LOOP END", ui::amber) };
        dir = combo (pSeqDir, "DIRECTION"); rop = toggle (pSeqResetOnPlay, "RESET ON PLAY", ui::amber);
        grid = make<StepGrid> (p, -1, ui::amber, false); props = make<PropSelector> (*grid, false, ui::amber);
        grid->onSelect = [this] { repaint(); };
        auto btn = [this] (const char* t, std::function<void()> fn) { auto* b = make<juce::TextButton> (t); b->onClick = std::move (fn); return b; };
        auto& pr = proc;
        bRandom = btn ("RANDOM FILL", [&pr] { Rng r ((uint64_t) juce::Time::currentTimeMillis()); pr.editPattern (-1, [&] (Pattern& pt) { pt.randomFill (r, 0.45f, true); }); });
        bMutate = btn ("MUTATE",      [&pr] { Rng r ((uint64_t) juce::Time::currentTimeMillis()); pr.editPattern (-1, [&] (Pattern& pt) { pt.mutate (r, 0.25f); }); });
        bClear  = btn ("CLEAR",       [&pr] { pr.editPattern (-1, [] (Pattern& pt) { pt.clear(); }); });
        bRotL   = btn ("<<",          [&pr] { const int n = (int) pr.getParam (pSeqLen); pr.editPattern (-1, [n] (Pattern& pt) { pt.rotate (n, -1); }); });
        bRotR   = btn (">>",          [&pr] { const int n = (int) pr.getParam (pSeqLen); pr.editPattern (-1, [n] (Pattern& pt) { pt.rotate (n, 1); }); });
        bRev    = btn ("REVERSE",     [&pr] { const int n = (int) pr.getParam (pSeqLen); pr.editPattern (-1, [n] (Pattern& pt) { pt.reverse (n); }); });
        bReset  = btn ("RESET",       [&pr] { pr.requestSeqReset(); });
    }
    void paint (juce::Graphics& g) override
    {
        drawPanel (g, rRhythm, "RHYTHM / GROOVE GENERATOR", ui::teal); drawPanel (g, rSeq, "STEP SEQUENCER", ui::amber);
        drawPanel (g, rGrid, "PATTERN", ui::amber);
        g.setColour (ui::dim); g.setFont (juce::FontOptions (11.f));
        g.drawText ("Click: on/off   |   Shift+Click: Groove step (generator decides)   |   Alt+Click: Accent   |   Drag in bars: edit   |   Wheel: velocity",
                    rGrid.getX() + 18, rGrid.getBottom() - 20, rGrid.getWidth() - 30, 16, juce::Justification::centredLeft);
    }
    void resized() override
    {
        auto a = getLocalBounds().reduced (14, 8); auto top = a.removeFromTop (190); a.removeFromTop (12);
        rRhythm = top.removeFromLeft (680); top.removeFromLeft (14); rSeq = top; rGrid = a;
        auto c = inner (rRhythm); auto r1 = c.removeFromTop (46); placeRow (r1.withTrimmedRight (120), { src, res, spd });
        auto r1b = Rectangle<int> (rRhythm.getRight() - 110, rRhythm.getY() + 8, 100, 24); trip->setBounds (r1b);
        c.removeFromTop (2); placeGrid (c, { k[0], k[1], k[2], k[3], k[4], k[5], k[6] }, 7, 92);
        auto s = inner (rSeq); auto sr = s.removeFromTop (92); placeRow (sr, { sk[0], sk[1], sk[2] });
        auto s2 = s.removeFromTop (46); placeRow (s2.removeFromLeft (s2.getWidth() / 2), { dir }); rop->setBounds (s2.withTrimmedLeft (s2.getWidth() / 2 - 0).withTrimmedTop (18).reduced (4, 0).translated (-4, 0));
        auto g = rGrid.reduced (10, 0); g.removeFromTop (28);
        auto toolbar = g.removeFromTop (26);
        placeRow (toolbar.removeFromLeft (330), { props });
        auto bw = toolbar.removeFromRight (560); placeRow (bw, { bRandom, bMutate, bClear, bRotL, bRotR, bRev, bReset });
        g.removeFromTop (6); g.removeFromBottom (22); grid->setBounds (g);
        (void) bw;
    }
private:
    ParamCombo *src, *res, *spd, *dir; ParamToggle *trip, *rop; std::vector<ParamKnob*> k, sk; StepGrid* grid; PropSelector* props;
    juce::TextButton *bRandom, *bMutate, *bClear, *bRotL, *bRotR, *bRev, *bReset;
    Rectangle<int> rRhythm, rSeq, rGrid;
};

// ----------------------------------------------------------------------------- PERC
class PercPage : public PageBase
{
public:
    explicit PercPage (PsyProcessor& p) : PageBase (p)
    {
        on = toggle (pPercOn, "PERC ON", ui::magenta); midi = toggle (pPercMidi, "MIDI OUT", ui::magenta); audio = toggle (pPercAudio, "INTERNAL AUDIO", ui::magenta);
        gk = { knob (pPercSwing, "SWING", ui::magenta), knob (pPercInt, "INTENSITY", ui::magenta), knob (pPercRandom, "RANDOM", ui::magenta), knob (pPercVar, "VARIATION", ui::magenta), knob (pPercLevel, "LEVEL", ui::magenta), knob (pPercPitch, "PITCH", ui::magenta) };
        fill = combo (pPercFill, "FILL EVERY");
        auto& pr = proc;
        const char* tn[4] = { "FULL-ON", "PROGRESSIVE", "MINIMAL", "ROLLING" };
        for (int v = 0; v < 4; ++v) { auto* b = make<juce::TextButton> (tn[v]); b->onClick = [&pr, v] { Rng r ((uint64_t) juce::Time::currentTimeMillis()); auto bank = pr.copyBank(); applyPsyGroove (bank, v, r); pr.setBank (bank); }; tmpl.push_back (b); }
        { auto* b = make<juce::TextButton> ("RANDOMIZE ALL"); b->onClick = [&pr] { Rng r ((uint64_t) juce::Time::currentTimeMillis()); auto bank = pr.copyBank(); for (int l = 0; l < kNumLanes; ++l) randomizeLane (bank.lane[l], l, (int) pr.getParam (laneParam (l, lpLen)), r); pr.setBank (bank); }; tmpl.push_back (b); }
        for (int l = 0; l < kNumLanes; ++l)
        {
            auto* sel = make<juce::TextButton> (laneName (l)); sel->setClickingTogglesState (true); sel->setRadioGroupId (4242); sel->setColour (juce::TextButton::buttonOnColourId, ui::magenta);
            sel->onClick = [this, l] { selectLane (l); }; laneBtn[l] = sel;
            mute[l] = toggle (laneParam (l, lpMute), "M", ui::amber);
            mini[l] = make<StepGrid> (p, l, ui::magenta, true); mini[l]->onSelect = [this, l] { if (lane != l) { selectLane (l); } };
        }
        big = make<StepGrid> (p, 0, ui::magenta, false); props = make<PropSelector> (*big, true, ui::magenta);
        laneBtn[0]->setToggleState (true, juce::dontSendNotification); rebuildLaneControls();
    }
    void paint (juce::Graphics& g) override
    {
        drawPanel (g, rGlobal, "PERCUSSION  (extra module – not part of the original plugin)", ui::magenta);
        drawPanel (g, rOverview, "LANES  (each lane has its own length & resolution = polyrhythm)", ui::magenta);
        drawPanel (g, rEdit, String ("LANE EDITOR  -  ") + laneName (lane), ui::magenta);
    }
    void resized() override
    {
        auto a = getLocalBounds().reduced (14, 6);
        rGlobal = a.removeFromTop (118); a.removeFromTop (8); rOverview = a.removeFromTop (8 * 27 + 38); a.removeFromTop (8); rEdit = a;
        auto c = inner (rGlobal);
        on->setBounds (Rectangle<int> (c.getX(), c.getY(), 94, 28)); midi->setBounds (Rectangle<int> (c.getX() + 98, c.getY(), 94, 28)); audio->setBounds (Rectangle<int> (c.getX(), c.getY() + 34, 192, 28));
        auto kb = Rectangle<int> (c.getX() + 205, c.getY() - 4, 470, 90); placeGrid (kb, { gk[0], gk[1], gk[2], gk[3], gk[4], gk[5] }, 6, 90);
        fill->setBounds (Rectangle<int> (c.getX() + 690, c.getY(), 110, 46));
        auto tb = Rectangle<int> (c.getRight() - 260, c.getY(), 260, 90); const int bh = 28; int i = 0;
        for (auto* b : tmpl) { b->setBounds (tb.getX() + (i % 2) * 130, tb.getY() + (i / 2) * (bh + 4), 126, bh); ++i; }
        auto o = inner (rOverview).withTrimmedTop (-18); int y = o.getY();
        for (int l = 0; l < kNumLanes; ++l) { laneBtn[l]->setBounds (o.getX(), y, 76, 24); mute[l]->setBounds (o.getX() + 80, y, 30, 24); mini[l]->setBounds (o.getX() + 116, y, o.getWidth() - 116, 24); y += 27; }
        auto e = inner (rEdit); auto left2 = e.removeFromLeft (e.getWidth() - 440); auto tool = left2.removeFromTop (26); props->setBounds (tool.removeFromLeft (360));
        left2.removeFromTop (4); big->setBounds (left2); e.removeFromLeft (14);
        placeGrid (e, laneCtl, 4, 82);
    }
private:
    ParamToggle *on, *midi, *audio, *mute[kNumLanes]; ParamCombo* fill; std::vector<ParamKnob*> gk; std::vector<juce::TextButton*> tmpl;
    juce::TextButton* laneBtn[kNumLanes]; StepGrid* mini[kNumLanes]; StepGrid* big; PropSelector* props;
    std::vector<juce::Component*> laneCtl; int lane = 0; Rectangle<int> rGlobal, rOverview, rEdit;

    void selectLane (int l)
    {
        lane = l; laneBtn[l]->setToggleState (true, juce::dontSendNotification);
        owned.removeObject (big, true); owned.removeObject (props, true);
        big = make<StepGrid> (proc, l, ui::magenta, false); props = make<PropSelector> (*big, true, ui::magenta);
        rebuildLaneControls(); resized(); repaint();
    }
    void rebuildLaneControls()
    {
        for (auto* c : laneCtl) owned.removeObject (c, true);
        laneCtl.clear(); const int l = lane;
        laneCtl = { knob (laneParam (l, lpLevel), "LEVEL", ui::magenta), knob (laneParam (l, lpPitch), "PITCH", ui::magenta), knob (laneParam (l, lpPan), "PAN", ui::magenta), knob (laneParam (l, lpProb), "PROB", ui::magenta),
                    knob (laneParam (l, lpGate), "GATE", ui::magenta), knob (laneParam (l, lpLen), "STEPS", ui::magenta), combo (laneParam (l, lpRes), "RES") };
    }
};

// ----------------------------------------------------------------------------- BEAT REPEAT
class BeatRepeatPage : public PageBase
{
public:
    explicit BeatRepeatPage (PsyProcessor& p) : PageBase (p)
    {
        on = toggle (pBrOn, "BEAT REPEAT ON", ui::amber); bm = toggle (pBrMain, "ON GENERATOR", ui::amber); bp = toggle (pBrPerc, "ON PERC", ui::amber);
        size = combo (pBrSize, "FRAGMENT SIZE"); interval = combo (pBrInterval, "CHECK EVERY"); duration = combo (pBrDuration, "REPEAT FOR");
        k = { knob (pBrChance, "FREQUENCY", ui::amber), knob (pBrStutter, "STUTTER", ui::amber), knob (pBrGate, "GATE", ui::amber), knob (pBrVar, "VARIATION", ui::amber),
              knob (pBrIntensity, "INTENSITY", ui::amber), knob (pBrRandom, "RANDOM SIZE", ui::amber), knob (pBrMix, "LIVE MUTE", ui::amber) };
        freeze = make<HoldButton> (p, pBrFreeze, "HOLD  -  FREEZE / STUTTER"); freeze->setColour (juce::TextButton::buttonColourId, ui::amber.darker (0.4f));
    }
    void paint (juce::Graphics& g) override
    {
        drawPanel (g, r, "BEAT REPEAT  (extra module – not part of the original plugin)", ui::amber);
        g.setColour (ui::dim); g.setFont (juce::FontOptions (12.f));
        g.drawFittedText ("Captures the last notes the generator/PERC played and repeats that fragment in sync with the host tempo. "
                          "FREQUENCY = chance of triggering at every CHECK interval. STUTTER < 0 fades repeats out, > 0 builds them up. "
                          "LIVE MUTE = how much the live notes are replaced during a repeat. Hold the big button to freeze the last fragment on the next grid step; release to return.",
                          r.getX() + 18, r.getBottom() - 90, r.getWidth() - 36, 80, juce::Justification::topLeft, 3);
    }
    void resized() override
    {
        r = getLocalBounds().reduced (14, 8); auto c = inner (r);
        auto t = c.removeFromTop (30); placeRow (t, { on, bm, bp });
        c.removeFromTop (14); auto cr = c.removeFromTop (46); placeRow (cr.removeFromLeft (560), { size, interval, duration });
        c.removeFromTop (14); placeGrid (c.removeFromTop (110), { k[0], k[1], k[2], k[3], k[4], k[5], k[6] }, 7, 110);
        c.removeFromTop (14); freeze->setBounds (c.removeFromTop (70).reduced (120, 0));
    }
private:
    ParamToggle *on, *bm, *bp; ParamCombo *size, *interval, *duration; std::vector<ParamKnob*> k; HoldButton* freeze; Rectangle<int> r;
};

// ============================================================================ Splash
class Splash : public juce::Component, private juce::Timer
{
public:
    explicit Splash (juce::Image l) : logo (std::move (l)) { startTimerHz (30); }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (ui::bg);
        if (logo.isValid())
        {
            auto area = getLocalBounds().withSizeKeepingCentre (getWidth() / 2, getHeight() / 2).toFloat();
            g.drawImage (logo, area, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        }
        else
        {
            g.setColour (ui::dim); g.setFont (juce::FontOptions (14.f));
            g.drawText ("[ place your official logo at assets/logo.png and rebuild ]", getLocalBounds().withY (getHeight() / 2 + 40).withHeight (24), juce::Justification::centred);
        }
        g.setColour (ui::text); g.setFont (juce::FontOptions (22.f, juce::Font::bold));
        g.drawText (JucePlugin_Name, getLocalBounds().withY (getHeight() / 2 - 100).withHeight (40), juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent&) override { ticks = 200; }
private:
    juce::Image logo; int ticks = 0;
    void timerCallback() override
    {
        ++ticks; if (ticks > 60) setAlpha (juce::jmax (0.f, 1.f - (float) (ticks - 60) / 20.f));
        if (ticks > 80) { stopTimer(); setVisible (false); }
    }
};

// ============================================================================ Editor
static constexpr int kW = 1120, kH = 700, kHeaderH = 64, kTabH = 34;

PsyEditor::PsyEditor (PsyProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    lnf = std::make_unique<PsyLnF>(); setLookAndFeel (lnf.get());
#if PSY_HAS_LOGO
    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);
#endif
    pages[0] = std::make_unique<GeneratorPage> (p); pages[1] = std::make_unique<RhythmPage> (p);
    pages[2] = std::make_unique<PercPage> (p);      pages[3] = std::make_unique<BeatRepeatPage> (p);
    const char* names[4] = { "GENERATOR", "RHYTHM & SEQUENCER", "PERC", "BEAT REPEAT" };
    const Colour cols[4] = { ui::teal, ui::amber, ui::magenta, ui::amber };
    for (int i = 0; i < 4; ++i)
    {
        tabs[i].setButtonText (names[i]); tabs[i].setClickingTogglesState (true); tabs[i].setRadioGroupId (99);
        tabs[i].setColour (juce::TextButton::buttonOnColourId, cols[i]); tabs[i].onClick = [this, i] { showPage (i); };
        addAndMakeVisible (tabs[i]); addChildComponent (*pages[i]);
    }
    // controles globais do cabeçalho
    struct H { int idx; const char* label; Colour c; };
    for (auto h : { H { pMainOn, "GEN ON", ui::teal }, H { pLatch, "LATCH", ui::teal }, H { pFreeRun, "FREE RUN", ui::violet } })
    { auto* t = new ParamToggle (p, h.idx, h.label, h.c); headerControls.add (t); addAndMakeVisible (t); }
    presetButton.onClick = [this] { showPresetMenu(); }; prevButton.onClick = [this] { stepPreset (-1); }; nextButton.onClick = [this] { stepPreset (1); }; saveButton.onClick = [this] { promptSave(); };
    for (auto* b : { &presetButton, &prevButton, &nextButton, &saveButton }) addAndMakeVisible (b);
    refreshPresetLabel();
    tabs[0].setToggleState (true, juce::dontSendNotification); showPage (0);
    setSize (kW, kH); setResizable (false, false);
    splash = std::make_unique<Splash> (logo); addAndMakeVisible (*splash); splash->setBounds (getLocalBounds());
    startTimerHz (10);
}
PsyEditor::~PsyEditor() { setLookAndFeel (nullptr); }

void PsyEditor::showPage (int i) { currentPage = i; for (int k = 0; k < 4; ++k) pages[k]->setVisible (k == i); }

void PsyEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    g.setColour (ui::panel); g.fillRect (0, 0, getWidth(), kHeaderH);
    g.setColour (ui::line); g.drawHorizontalLine (kHeaderH, 0.f, (float) getWidth());
    if (logo.isValid()) g.drawImage (logo, logoArea.toFloat(), juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    else
    {
        g.setColour (ui::dim); g.drawRoundedRectangle (logoArea.toFloat().reduced (1.f), 6.f, 1.f);
        g.setFont (juce::FontOptions (10.f)); g.drawFittedText ("YOUR LOGO\nassets/logo.png", logoArea, juce::Justification::centred, 2);
    }
    g.setColour (ui::text); g.setFont (juce::FontOptions (20.f, juce::Font::bold));
    g.drawText (JucePlugin_Name, logoArea.getRight() + 12, 8, 220, 28, juce::Justification::centredLeft);
    g.setColour (ui::dim); g.setFont (juce::FontOptions (10.5f));
    g.drawText ("MIDI RHYTHM GENERATOR + PERC + BEAT REPEAT", logoArea.getRight() + 12, 34, 300, 16, juce::Justification::centredLeft);
    // status
    const bool run = proc.uiRunning.load();
    g.setColour (run ? ui::teal : ui::dim); g.fillEllipse ((float) statusArea.getX(), (float) statusArea.getCentreY() - 4.f, 8.f, 8.f);
    g.setColour (ui::text); g.setFont (juce::FontOptions (12.f));
    g.drawText (String (run ? "RUNNING  " : "STOPPED  ") + String (proc.uiBpm.load(), 1) + " BPM", statusArea.withTrimmedLeft (14), juce::Justification::centredLeft);
}

void PsyEditor::resized()
{
    logoArea = { 14, 8, 150, 48 };
    auto r = Rectangle<int> (330, 14, 560, 36);
    prevButton.setBounds (r.removeFromLeft (30)); presetButton.setBounds (r.removeFromLeft (300).reduced (3, 0)); nextButton.setBounds (r.removeFromLeft (30)); r.removeFromLeft (6); saveButton.setBounds (r.removeFromLeft (70));
    for (int i = 0; i < headerControls.size(); ++i) headerControls[i]->setBounds (740 + i * 100, 8, 94, 24);
    statusArea = { 740, 38, 300, 20 };
    auto t = Rectangle<int> (14, kHeaderH + 6, getWidth() - 28, kTabH - 4); const int w = 190;
    for (int i = 0; i < 4; ++i) tabs[i].setBounds (t.getX() + i * (w + 6), t.getY(), w, t.getHeight());
    for (auto& pg : pages) pg->setBounds (0, kHeaderH + kTabH + 8, getWidth(), getHeight() - kHeaderH - kTabH - 8);
    if (splash) splash->setBounds (getLocalBounds());
}

void PsyEditor::timerCallback() { repaint (statusArea.expanded (4)); }

void PsyEditor::refreshPresetLabel() { presetButton.setButtonText (proc.currentPresetName); }

void PsyEditor::showPresetMenu()
{
    juce::PopupMenu m; presetOrder.clear(); int id = 1;
    m.addSectionHeader ("Factory");
    const auto fac = PsyProcessor::factoryPresetNames();
    for (int i = 0; i < fac.size(); ++i) { m.addItem (id++, fac[i]); presetOrder.add ("F:" + String (i)); }
    const auto usr = proc.listUserPresets();
    if (usr.size() > 0) m.addSectionHeader ("User");
    for (auto& n : usr) { m.addItem (id++, n); presetOrder.add ("U:" + n); }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton), [this] (int res)
    {
        if (res <= 0 || res > presetOrder.size()) return;
        const auto key = presetOrder[res - 1];
        if (key.startsWith ("F:")) proc.loadFactoryPreset (key.substring (2).getIntValue());
        else proc.loadPresetFile (PsyProcessor::presetDirectory().getChildFile (key.substring (2) + ".psypreset"));
        refreshPresetLabel(); repaint();
    });
}
void PsyEditor::stepPreset (int dir)
{
    juce::StringArray all; const auto fac = PsyProcessor::factoryPresetNames(); for (auto& f : fac) all.add ("F:" + f); for (auto& u : proc.listUserPresets()) all.add ("U:" + u);
    int cur = 0; for (int i = 0; i < all.size(); ++i) if (all[i].substring (2) == proc.currentPresetName) cur = i;
    const int nx = (cur + dir + all.size()) % all.size(); const auto k = all[nx];
    if (k.startsWith ("F:")) proc.loadFactoryPreset (fac.indexOf (k.substring (2)));
    else proc.loadPresetFile (PsyProcessor::presetDirectory().getChildFile (k.substring (2) + ".psypreset"));
    refreshPresetLabel();
}
void PsyEditor::promptSave()
{
    auto* w = new juce::AlertWindow ("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", proc.currentPresetName); w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
    {
        if (r == 1) { const auto nm = w->getTextEditorContents ("name"); if (! proc.savePreset (nm)) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset", "Could not save the preset."); refreshPresetLabel(); }
    }), true);
}
