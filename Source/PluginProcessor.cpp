#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Aplica compresion y ancho estereo a una banda.
    inline void processBand (float& a, float& b, BandComp& comp, float thresholdDb, float ratio, float width)
    {
        const float level = std::max (std::abs (a), std::abs (b));
        const float g = comp.process (level, thresholdDb, ratio);
        a *= g;
        b *= g;

        const float mid = (a + b) * 0.5f;
        const float side = (a - b) * 0.5f * width;
        a = mid + side;
        b = mid - side;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout EdyMastericerAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto add = [&params] (const char* id, const char* name, float lo, float hi, float def, float step)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 },
                                                                        name,
                                                                        juce::NormalisableRange<float> (lo, hi, step),
                                                                        def));
    };

    add ("inGain",     "Entrada",            -12.0f,  12.0f,   0.0f, 0.1f);
    add ("eqLow",      "EQ Graves",          -12.0f,  12.0f,   0.0f, 0.1f);
    add ("eqMid",      "EQ Medios",          -12.0f,  12.0f,   0.0f, 0.1f);
    add ("eqHigh",     "EQ Agudos",          -12.0f,  12.0f,   0.0f, 0.1f);
    add ("compThresh", "Umbral compresor",   -40.0f,   0.0f, -12.0f, 0.1f);
    add ("compRatio",  "Ratio compresor",      1.0f,  10.0f,   2.0f, 0.1f);
    add ("widthLow",   "Estereo Graves",       0.0f, 200.0f, 100.0f, 1.0f);
    add ("widthMid",   "Estereo Medios",       0.0f, 200.0f, 100.0f, 1.0f);
    add ("widthHigh",  "Estereo Agudos",       0.0f, 200.0f, 100.0f, 1.0f);
    add ("exciter",    "Exciter",              0.0f, 100.0f,   0.0f, 1.0f);
    add ("drive",      "Drive",                0.0f,  12.0f,   0.0f, 0.1f);
    add ("ceiling",    "Techo",              -12.0f,   0.0f,  -0.3f, 0.1f);
    add ("release",    "Release",             10.0f, 500.0f, 100.0f, 1.0f);
    add ("outGain",    "Salida",             -12.0f,  12.0f,   0.0f, 0.1f);
    add ("reverbSize", "Reverb Tamano",        0.0f, 100.0f,  40.0f, 1.0f);
    add ("reverbDamp", "Reverb Amortiguacion", 0.0f, 100.0f,  50.0f, 1.0f);
    add ("reverbMix",  "Reverb Mezcla",        0.0f, 100.0f,   0.0f, 1.0f);

    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "lufsTarget", 1 },
                                                                     "Objetivo LUFS",
                                                                     juce::StringArray { "Spotify / YouTube (-14)",
                                                                                         "Apple Music (-16)",
                                                                                         "Fuerte / club (-9)",
                                                                                         "TV / radio (-23)" },
                                                                     0));

    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

EdyMastericerAudioProcessor::EdyMastericerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pInGain     = apvts.getRawParameterValue ("inGain");
    pOutGain    = apvts.getRawParameterValue ("outGain");
    pEqLow      = apvts.getRawParameterValue ("eqLow");
    pEqMid      = apvts.getRawParameterValue ("eqMid");
    pEqHigh     = apvts.getRawParameterValue ("eqHigh");
    pCompThresh = apvts.getRawParameterValue ("compThresh");
    pCompRatio  = apvts.getRawParameterValue ("compRatio");
    pWidthLow   = apvts.getRawParameterValue ("widthLow");
    pWidthMid   = apvts.getRawParameterValue ("widthMid");
    pWidthHigh  = apvts.getRawParameterValue ("widthHigh");
    pExciter    = apvts.getRawParameterValue ("exciter");
    pDrive      = apvts.getRawParameterValue ("drive");
    pCeiling    = apvts.getRawParameterValue ("ceiling");
    pRelease    = apvts.getRawParameterValue ("release");
    pBypass     = apvts.getRawParameterValue ("bypass");
    pReverbSize = apvts.getRawParameterValue ("reverbSize");
    pReverbDamp = apvts.getRawParameterValue ("reverbDamp");
    pReverbMix  = apvts.getRawParameterValue ("reverbMix");
}

bool EdyMastericerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EdyMastericerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };

    eqLow.prepare (spec);
    eqMid.prepare (spec);
    eqHigh.prepare (spec);
    exHP.prepare (spec);
    limiter.prepare (spec);
    reverb.prepare (spec);
    reverb.reset();

    // Medidor LUFS: filtro K (shelf de agudos + pasa-altos) y bloques de 100 ms
    kShelf.prepare (spec);
    kHP.prepare (spec);
    kShelf.reset();
    kHP.reset();
    *kShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 1681.97f, 0.7071f, 1.5849f);
    *kHP.state    = *juce::dsp::IIR::Coefficients<float>::makeHighPass  (sampleRate, 38.13f, 0.5f);
    kBuf.setSize (2, (int) spec.maximumBlockSize);
    subBlockLen = juce::jmax (1, (int) (sampleRate * 0.1));
    subCount = 0;
    subSum = 0.0;
    subWritePos = 0;
    subFilled = 0;
    if (sampleRate != lastSampleRate)
    {
        histEnergy.fill (0.0);
        histCount.fill (0.0);
        lastSampleRate = sampleRate;
    }

    xo1.prepare (spec);
    xo2.prepare (spec);
    ap2.prepare (spec);
    xo1.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    xo2.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    ap2.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
    xo1.setCutoffFrequency (200.0f);
    xo2.setCutoffFrequency (3000.0f);
    ap2.setCutoffFrequency (3000.0f);

    eqLow.reset(); eqMid.reset(); eqHigh.reset(); exHP.reset();
    xo1.reset(); xo2.reset(); ap2.reset();
    limiter.reset();

    *exHP.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 3000.0f);

    exBuf.setSize (2, (int) spec.maximumBlockSize);

    compLow.prepare (sampleRate);
    compMid.prepare (sampleRate);
    compHigh.prepare (sampleRate);

    cLo = cMid = cHi = -999.0f; // fuerza la actualizacion del EQ

    wLow.reset (sampleRate, 0.05);   wLow.setCurrentAndTargetValue (pWidthLow->load() / 100.0f);
    wMid.reset (sampleRate, 0.05);   wMid.setCurrentAndTargetValue (pWidthMid->load() / 100.0f);
    wHigh.reset (sampleRate, 0.05);  wHigh.setCurrentAndTargetValue (pWidthHigh->load() / 100.0f);
    exAmt.reset (sampleRate, 0.05);  exAmt.setCurrentAndTargetValue (pExciter->load() / 100.0f);

    lastIn    = juce::Decibels::decibelsToGain (pInGain->load());
    lastDrive = juce::Decibels::decibelsToGain (pDrive->load());
    lastOut   = juce::Decibels::decibelsToGain (pOutGain->load());
}

void EdyMastericerAudioProcessor::updateEq()
{
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    const float lo  = pEqLow->load();
    const float mid = pEqMid->load();
    const float hi  = pEqHigh->load();

    if (lo != cLo)
    {
        *eqLow.state = *Coeffs::makeLowShelf (sr, 120.0f, 0.7f, juce::Decibels::decibelsToGain (lo));
        cLo = lo;
    }
    if (mid != cMid)
    {
        *eqMid.state = *Coeffs::makePeakFilter (sr, 1000.0f, 0.7f, juce::Decibels::decibelsToGain (mid));
        cMid = mid;
    }
    if (hi != cHi)
    {
        *eqHigh.state = *Coeffs::makeHighShelf (sr, 6000.0f, 0.7f, juce::Decibels::decibelsToGain (hi));
        cHi = hi;
    }
}

void EdyMastericerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2)
        return;

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // Medidor de entrada
    const float inL = buffer.getMagnitude (0, 0, n);
    const float inR = buffer.getMagnitude (1, 0, n);
    inPeakL.store (inL);
    inPeakR.store (inR);

    if (pBypass->load() > 0.5f)
    {
        outPeakL.store (inL);
        outPeakR.store (inR);
        measureLoudness (buffer, n);
        return;
    }

    auto ramp = [&] (float& last, float target)
    {
        if (last != 1.0f || target != 1.0f)
            buffer.applyGainRamp (0, n, last, target);
        last = target;
    };

    // 1) Ganancia de entrada
    ramp (lastIn, juce::Decibels::decibelsToGain (pInGain->load()));

    // 2) Ecualizador
    updateEq();
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        eqLow.process (ctx);
        eqMid.process (ctx);
        eqHigh.process (ctx);
    }

    // 3) Compresor multibanda + ancho estereo por banda
    {
        const float thr   = pCompThresh->load();
        const float ratio = juce::jmax (1.0f, pCompRatio->load());
        wLow.setTargetValue  (pWidthLow->load()  / 100.0f);
        wMid.setTargetValue  (pWidthMid->load()  / 100.0f);
        wHigh.setTargetValue (pWidthHigh->load() / 100.0f);

        float* l = buffer.getWritePointer (0);
        float* r = buffer.getWritePointer (1);

        for (int i = 0; i < n; ++i)
        {
            float lowL, restL, lowR, restR;
            xo1.processSample (0, l[i], lowL, restL);
            xo1.processSample (1, r[i], lowR, restR);

            float midL, highL, midR, highR;
            xo2.processSample (0, restL, midL, highL);
            xo2.processSample (1, restR, midR, highR);

            // La banda de graves necesita pasar por un filtro "all-pass" para mantener la fase alineada
            lowL = ap2.processSample (0, lowL);
            lowR = ap2.processSample (1, lowR);

            processBand (lowL,  lowR,  compLow,  thr, ratio, wLow.getNextValue());
            processBand (midL,  midR,  compMid,  thr, ratio, wMid.getNextValue());
            processBand (highL, highR, compHigh, thr, ratio, wHigh.getNextValue());

            l[i] = lowL + midL + highL;
            r[i] = lowR + midR + highR;
        }
    }

    // 4) Exciter armonico (satura suavemente los agudos y los suma)
    exAmt.setTargetValue (pExciter->load() / 100.0f);
    if (exAmt.isSmoothing() || exAmt.getTargetValue() > 0.0f)
    {
        if (exBuf.getNumSamples() < n)
            exBuf.setSize (2, n, false, false, true);

        exBuf.copyFrom (0, 0, buffer, 0, 0, n);
        exBuf.copyFrom (1, 0, buffer, 1, 0, n);

        juce::dsp::AudioBlock<float> exBlock (exBuf);
        auto sub = exBlock.getSubBlock (0, (size_t) n);
        exHP.process (juce::dsp::ProcessContextReplacing<float> (sub));

        float* l = buffer.getWritePointer (0);
        float* r = buffer.getWritePointer (1);
        const float* hl = exBuf.getReadPointer (0);
        const float* hr = exBuf.getReadPointer (1);

        for (int i = 0; i < n; ++i)
        {
            const float amt = exAmt.getNextValue() * 0.6f;
            l[i] += amt * std::tanh (3.0f * hl[i]) / 3.0f;
            r[i] += amt * std::tanh (3.0f * hr[i]) / 3.0f;
        }
    }

    // 4b) Reverb de masterizacion (mezcla 0 % = sin efecto)
    {
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize   = pReverbSize->load() / 100.0f;
        rp.damping    = pReverbDamp->load() / 100.0f;
        rp.wetLevel   = (pReverbMix->load() / 100.0f) * 0.4f;
        rp.dryLevel   = 1.0f;
        rp.width      = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);

        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) n);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    // 5) Drive (empuja la senal hacia el limitador para ganar volumen)
    ramp (lastDrive, juce::Decibels::decibelsToGain (pDrive->load()));

    // 6) Limitador final
    const float ceilingDb = pCeiling->load();
    limiter.setThreshold (ceilingDb);
    limiter.setRelease (pRelease->load());
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) n);
        limiter.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    // Seguro: nunca superar el techo
    const float c = juce::Decibels::decibelsToGain (ceilingDb);
    for (int ch = 0; ch < 2; ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            d[i] = juce::jlimit (-c, c, d[i]);
    }

    // 7) Ajuste final de volumen de salida
    ramp (lastOut, juce::Decibels::decibelsToGain (pOutGain->load()));

    outPeakL.store (buffer.getMagnitude (0, 0, n));
    outPeakR.store (buffer.getMagnitude (1, 0, n));

    measureLoudness (buffer, n);
}

void EdyMastericerAudioProcessor::measureLoudness (const juce::AudioBuffer<float>& buf, int n)
{
    if (resetRequested.exchange (false))
    {
        histEnergy.fill (0.0);
        histCount.fill (0.0);
        subPow.fill (0.0);
        subFilled = 0;
        subWritePos = 0;
        subCount = 0;
        subSum = 0.0;
        lufsMomentary.store (-100.0f);
        lufsShort.store (-100.0f);
        lufsIntegrated.store (-100.0f);
    }

    if (kBuf.getNumSamples() < n)
        kBuf.setSize (2, n, false, false, true);

    kBuf.copyFrom (0, 0, buf, 0, 0, n);
    kBuf.copyFrom (1, 0, buf, 1, 0, n);

    juce::dsp::AudioBlock<float> kBlock (kBuf);
    auto sub = kBlock.getSubBlock (0, (size_t) n);
    kShelf.process (juce::dsp::ProcessContextReplacing<float> (sub));
    kHP.process (juce::dsp::ProcessContextReplacing<float> (sub));

    const float* l = kBuf.getReadPointer (0);
    const float* r = kBuf.getReadPointer (1);

    for (int i = 0; i < n; ++i)
    {
        subSum += (double) l[i] * l[i] + (double) r[i] * r[i];

        if (++subCount >= subBlockLen)
        {
            pushSubBlock (subSum / (double) subBlockLen);
            subSum = 0.0;
            subCount = 0;
        }
    }
}

void EdyMastericerAudioProcessor::pushSubBlock (double power)
{
    constexpr int kHistory = 30; // 30 x 100 ms = 3 s

    subPow[(size_t) subWritePos] = power;
    subWritePos = (subWritePos + 1) % kHistory;
    subFilled = juce::jmin (subFilled + 1, kHistory);

    auto meanLast = [this] (int count)
    {
        double s = 0.0;
        for (int k = 0; k < count; ++k)
            s += subPow[(size_t) ((subWritePos - 1 - k + 2 * kHistory) % kHistory)];
        return s / (double) count;
    };

    auto toLufs = [] (double p)
    {
        return p > 1.0e-10 ? (float) (-0.691 + 10.0 * std::log10 (p)) : -100.0f;
    };

    if (subFilled >= 4)
    {
        const double pm = meanLast (4);
        const float lm = toLufs (pm);
        lufsMomentary.store (lm);

        // Integrado: solo bloques por encima de -70 LUFS, y luego por encima de (promedio - 10 LU)
        if (lm > -70.0f)
        {
            const int bin = juce::jlimit (0, kBins - 1, (int) std::floor ((lm + 70.0f) * 10.0f));
            histCount[(size_t) bin] += 1.0;
            histEnergy[(size_t) bin] += pm;
        }

        double totalCount = 0.0, totalEnergy = 0.0;
        for (int b = 0; b < kBins; ++b)
        {
            totalCount  += histCount[(size_t) b];
            totalEnergy += histEnergy[(size_t) b];
        }

        if (totalCount > 0.0)
        {
            const float relThreshold = toLufs (totalEnergy / totalCount) - 10.0f;
            double gCount = 0.0, gEnergy = 0.0;

            for (int b = 0; b < kBins; ++b)
            {
                if ((float) b * 0.1f - 70.0f >= relThreshold)
                {
                    gCount  += histCount[(size_t) b];
                    gEnergy += histEnergy[(size_t) b];
                }
            }

            if (gCount > 0.0)
                lufsIntegrated.store (toLufs (gEnergy / gCount));
        }
    }

    if (subFilled >= 4)
        lufsShort.store (toLufs (meanLast (subFilled)));
}

juce::AudioProcessorEditor* EdyMastericerAudioProcessor::createEditor()
{
    return new EdyMastericerAudioProcessorEditor (*this);
}

void EdyMastericerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void EdyMastericerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EdyMastericerAudioProcessor();
}
