#include "PluginEditor.h"

namespace
{
    const juce::Colour kBackground (0xff15171c);
    const juce::Colour kPanel      (0xff1f2229);
    const juce::Colour kBorder     (0xff343945);
    const juce::Colour kAccent     (0xffff9f1c);
    const juce::Colour kText       (0xffd8dce3);
    const juce::Colour kDim        (0xff8a909c);

    struct KnobDef { const char* id; const char* name; const char* suffix; };

    const KnobDef kKnobs[] =
    {
        { "eqLow",      "GRAVES",   " dB" },
        { "eqMid",      "MEDIOS",   " dB" },
        { "eqHigh",     "AGUDOS",   " dB" },
        { "compThresh", "UMBRAL",   " dB" },
        { "compRatio",  "RATIO",    " :1" },
        { "widthLow",   "GRAVES",   " %"  },
        { "widthMid",   "MEDIOS",   " %"  },
        { "widthHigh",  "AGUDOS",   " %"  },
        { "exciter",    "CANTIDAD", " %"  },
        { "drive",      "DRIVE",    " dB" },
        { "ceiling",    "TECHO",    " dB" },
        { "release",    "RELEASE",  " ms" },
        { "inGain",     "ENTRADA",  " dB" },
        { "outGain",    "SALIDA",   " dB" },
        { "reverbSize", "TAMANO",   " %"  },
        { "reverbDamp", "AMORTIG.", " %"  },
        { "reverbMix",  "MEZCLA",   " %"  }
    };

    struct SectionDef { const char* title; int first; int count; int row; };

    const SectionDef kSections[] =
    {
        { "ECUALIZADOR",          0, 3, 0 },
        { "COMPRESOR",            3, 2, 0 },
        { "AMPLIADOR ESTEREO",    5, 3, 0 },
        { "EXCITER",              8, 1, 1 },
        { "REVERB",              14, 3, 1 },
        { "MAXIMIZER / LIMITADOR", 9, 3, 1 },
        { "ENTRADA / SALIDA",    12, 2, 1 }
    };

    const juce::Colour kGreen (0xff3ddc84);
    const juce::Colour kRed   (0xffff4d4d);
    constexpr float kLufsMin = -32.0f;
    constexpr float kLufsMax = 0.0f;

    float levelToFraction (float level)
    {
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        return juce::jmap (juce::jlimit (-60.0f, 0.0f, db), -60.0f, 0.0f, 0.0f, 1.0f);
    }
}

EdyMastericerAudioProcessorEditor::EdyMastericerAudioProcessorEditor (EdyMastericerAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (980, 570);

    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto& k = knobs[(size_t) i];

        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        k.slider.setTextValueSuffix (kKnobs[i].suffix);
        k.slider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
        k.slider.setColour (juce::Slider::rotarySliderOutlineColourId, kBorder);
        k.slider.setColour (juce::Slider::thumbColourId, kText);
        k.slider.setColour (juce::Slider::textBoxTextColourId, kText);
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (k.slider);

        k.label.setText (kKnobs[i].name, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setColour (juce::Label::textColourId, kDim);
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, kKnobs[i].id, k.slider);
    }

    bypassButton.setColour (juce::ToggleButton::textColourId, kText);
    bypassButton.setColour (juce::ToggleButton::tickColourId, kAccent);
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bypassButton);

    targetBox.addItemList (proc.apvts.getParameter ("lufsTarget")->getAllValueStrings(), 1);
    targetBox.setColour (juce::ComboBox::backgroundColourId, kPanel);
    targetBox.setColour (juce::ComboBox::textColourId, kText);
    targetBox.setColour (juce::ComboBox::outlineColourId, kBorder);
    targetBox.setColour (juce::ComboBox::arrowColourId, kAccent);
    addAndMakeVisible (targetBox);
    targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "lufsTarget", targetBox);

    resetButton.setColour (juce::TextButton::buttonColourId, kPanel);
    resetButton.setColour (juce::TextButton::textColourOffId, kText);
    resetButton.onClick = [this] { proc.resetLoudness(); };
    addAndMakeVisible (resetButton);

    startTimerHz (30);
}

void EdyMastericerAudioProcessorEditor::resized()
{
    bypassButton.setBounds (getWidth() - 130, 18, 110, 28);

    meterArea = juce::Rectangle<int> (getWidth() - 100, 70, 88, getHeight() - 86);

    auto content = getLocalBounds().withTrimmedTop (62).withTrimmedRight (100).reduced (12);

    lufsArea = content.removeFromBottom (124).reduced (4);
    lufsBar = juce::Rectangle<int> (lufsArea.getX() + 14, lufsArea.getY() + 36, lufsArea.getWidth() - 14 - 220, 22);
    targetBox.setBounds (lufsArea.getRight() - 196, lufsArea.getY() + 40, 182, 26);
    resetButton.setBounds (lufsArea.getRight() - 196, lufsArea.getY() + 76, 182, 26);

    const int rowH = content.getHeight() / 2;

    sectionBounds.clear();

    for (int row = 0; row < 2; ++row)
    {
        int totalKnobs = 0;
        for (auto& s : kSections)
            if (s.row == row)
                totalKnobs += s.count;

        auto rowArea = content.withY (content.getY() + row * rowH).withHeight (rowH);
        int x = rowArea.getX();

        for (auto& s : kSections)
        {
            if (s.row != row)
                continue;

            const int w = rowArea.getWidth() * s.count / totalKnobs;
            auto sec = juce::Rectangle<int> (x, rowArea.getY(), w, rowH).reduced (4);
            sectionBounds.push_back (sec);
            x += w;

            auto inner = sec.reduced (6).withTrimmedTop (22);
            const int cellW = inner.getWidth() / s.count;

            for (int j = 0; j < s.count; ++j)
            {
                auto cell = juce::Rectangle<int> (inner.getX() + j * cellW, inner.getY(), cellW, inner.getHeight());
                auto& k = knobs[(size_t) (s.first + j)];
                k.label.setBounds (cell.removeFromTop (16));
                k.slider.setBounds (cell);
            }
        }
    }
}

void EdyMastericerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);

    g.setColour (kAccent);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("EdyMastericer", 16, 10, 320, 32, juce::Justification::centredLeft);

    g.setColour (kDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Suite de masterizacion estereo", 18, 38, 320, 16, juce::Justification::centredLeft);

    // Paneles de seccion (el orden coincide con el de kSections: fila 0 y despues fila 1)
    size_t idx = 0;
    for (int row = 0; row < 2; ++row)
    {
        for (auto& s : kSections)
        {
            if (s.row != row || idx >= sectionBounds.size())
                continue;

            auto r = sectionBounds[idx++];
            g.setColour (kPanel);
            g.fillRoundedRectangle (r.toFloat(), 8.0f);
            g.setColour (kBorder);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);

            g.setColour (kAccent);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (s.title, r.getX() + 12, r.getY() + 6, r.getWidth() - 24, 16, juce::Justification::centredLeft);
        }
    }

    // Medidor LUFS con zona objetivo
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (lufsArea.toFloat(), 8.0f);
        g.setColour (kBorder);
        g.drawRoundedRectangle (lufsArea.toFloat().reduced (0.5f), 8.0f, 1.0f);

        g.setColour (kAccent);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("LUFS - VOLUMEN PERCIBIDO (SALIDA)", lufsArea.getX() + 12, lufsArea.getY() + 6, 400, 16, juce::Justification::centredLeft);

        g.setColour (kDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("OBJETIVO", targetBox.getX(), targetBox.getY() - 16, 120, 14, juce::Justification::centredLeft);

        const int idx = juce::jlimit (0, 3, (int) proc.apvts.getRawParameterValue ("lufsTarget")->load());
        const float target = EdyMastericerAudioProcessor::lufsTargets[idx];

        auto xFor = [this] (float lufs)
        {
            const float f = juce::jlimit (0.0f, 1.0f, (lufs - kLufsMin) / (kLufsMax - kLufsMin));
            return (float) lufsBar.getX() + f * (float) lufsBar.getWidth();
        };

        g.setColour (kBackground);
        g.fillRect (lufsBar);

        // Zona amarilla (+-3 LU) y zona verde (+-1 LU) alrededor del objetivo
        g.setColour (kAccent.withAlpha (0.30f));
        g.fillRect (juce::Rectangle<float> (xFor (target - 3.0f), (float) lufsBar.getY(), xFor (target + 3.0f) - xFor (target - 3.0f), (float) lufsBar.getHeight()));
        g.setColour (kGreen.withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float> (xFor (target - 1.0f), (float) lufsBar.getY(), xFor (target + 1.0f) - xFor (target - 1.0f), (float) lufsBar.getHeight()));

        // Barra de nivel (corto plazo)
        const float shown = lufsS > -70.0f ? lufsS : (lufsM > -70.0f ? lufsM : -100.0f);
        if (shown > kLufsMin)
        {
            const float diff = std::abs (shown - target);
            g.setColour ((diff <= 1.0f ? kGreen : diff <= 3.0f ? kAccent : kRed).withAlpha (0.95f));
            g.fillRect (juce::Rectangle<float> ((float) lufsBar.getX(), (float) lufsBar.getY() + 6.0f,
                                                xFor (shown) - (float) lufsBar.getX(), (float) lufsBar.getHeight() - 12.0f));
        }

        // Linea del objetivo y marca del valor integrado
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillRect (xFor (target) - 1.0f, (float) lufsBar.getY() - 3.0f, 2.0f, (float) lufsBar.getHeight() + 6.0f);
        if (lufsI > -70.0f)
        {
            g.setColour (juce::Colour (0xff4dc3ff));
            g.fillRect (xFor (lufsI) - 1.0f, (float) lufsBar.getY(), 2.0f, (float) lufsBar.getHeight());
        }

        g.setColour (kBorder);
        g.drawRect (lufsBar, 1);

        // Escala
        g.setColour (kDim);
        g.setFont (juce::FontOptions (10.0f));
        for (int v = -30; v <= 0; v += 6)
            g.drawText (juce::String (v), (int) xFor ((float) v) - 14, lufsBar.getBottom() + 3, 28, 12, juce::Justification::centred);

        // Lecturas
        auto valueText = [] (float v) { return v > -99.0f ? juce::String (v, 1) + " LUFS" : juce::String ("---"); };
        const char* titles[3] = { "MOMENTANEO", "CORTO PLAZO (3 s)", "INTEGRADO" };
        const float values[3] = { lufsM, lufsS, lufsI };

        for (int i = 0; i < 3; ++i)
        {
            const int x = lufsBar.getX() + i * 115;
            g.setColour (kDim);
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawText (titles[i], x, lufsBar.getBottom() + 18, 112, 12, juce::Justification::centredLeft);
            g.setColour (i == 2 ? juce::Colour (0xff4dc3ff) : kText);
            g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
            g.drawText (valueText (values[i]), x, lufsBar.getBottom() + 31, 112, 22, juce::Justification::centredLeft);
        }

        // Guia en palabras
        const float ref = lufsI > -70.0f ? lufsI : (lufsS > -70.0f ? lufsS : -100.0f);
        juce::String msg = "Reproduce la cancion para medir";
        juce::Colour msgColour = kDim;

        if (ref > -70.0f)
        {
            const float diff = ref - target;
            msgColour = std::abs (diff) <= 1.0f ? kGreen : std::abs (diff) <= 3.0f ? kAccent : kRed;

            if (std::abs (diff) <= 1.0f)
                msg = "EN EL OBJETIVO (" + juce::String (target, 0) + " LUFS)";
            else if (diff < 0.0f)
                msg = "MUY BAJO: sube unos " + juce::String (-diff, 1) + " LU";
            else
                msg = "MUY ALTO: baja unos " + juce::String (diff, 1) + " LU";
        }

        g.setColour (msgColour);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawFittedText (msg, lufsBar.getX() + 350, lufsBar.getBottom() + 22, lufsBar.getWidth() - 350, 32, juce::Justification::centredLeft, 2);
    }

    // Medidores
    g.setColour (kPanel);
    g.fillRoundedRectangle (meterArea.toFloat(), 8.0f);
    g.setColour (kBorder);
    g.drawRoundedRectangle (meterArea.toFloat().reduced (0.5f), 8.0f, 1.0f);

    const int barW = 12, gap = 6, midGap = 10;
    const int totalW = 4 * barW + 2 * gap + midGap;
    int x = meterArea.getX() + (meterArea.getWidth() - totalW) / 2;
    const int top = meterArea.getY() + 28;
    const int bottom = meterArea.getBottom() - 12;
    const int h = bottom - top;

    g.setColour (kDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("IN",  x, meterArea.getY() + 8, 2 * barW + gap, 14, juce::Justification::centred);
    g.drawText ("OUT", x + 2 * barW + gap + midGap, meterArea.getY() + 8, 2 * barW + gap, 14, juce::Justification::centred);

    for (int i = 0; i < 4; ++i)
    {
        juce::Rectangle<int> bar (x, top, barW, h);
        g.setColour (kBackground);
        g.fillRect (bar);

        const float frac = levelToFraction (display[i]);
        const float db = juce::Decibels::gainToDecibels (display[i], -60.0f);
        g.setColour (db > -1.0f ? juce::Colour (0xffff4d4d)
                   : db > -6.0f ? kAccent
                                : juce::Colour (0xff3ddc84));
        const int fh = (int) (frac * (float) h);
        g.fillRect (x, bottom - fh, barW, fh);

        x += barW + (i == 1 ? midGap : gap);
    }
}

void EdyMastericerAudioProcessorEditor::timerCallback()
{
    const float v[4] = { proc.inPeakL.load(), proc.inPeakR.load(), proc.outPeakL.load(), proc.outPeakR.load() };

    for (int i = 0; i < 4; ++i)
        display[i] = std::max (v[i], display[i] * 0.88f);

    lufsM = proc.lufsMomentary.load();
    lufsS = proc.lufsShort.load();
    lufsI = proc.lufsIntegrated.load();

    repaint (meterArea);
    repaint (lufsArea);
}
