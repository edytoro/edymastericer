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

    float levelToFraction (float level)
    {
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        return juce::jmap (juce::jlimit (-60.0f, 0.0f, db), -60.0f, 0.0f, 0.0f, 1.0f);
    }
}

EdyMastericerAudioProcessorEditor::EdyMastericerAudioProcessorEditor (EdyMastericerAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (980, 440);

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

    startTimerHz (30);
}

void EdyMastericerAudioProcessorEditor::resized()
{
    bypassButton.setBounds (getWidth() - 130, 18, 110, 28);

    meterArea = juce::Rectangle<int> (getWidth() - 100, 70, 88, getHeight() - 86);

    auto content = getLocalBounds().withTrimmedTop (62).withTrimmedRight (100).reduced (12);
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

    repaint (meterArea);
}
