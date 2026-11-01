#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Compresor sencillo (un solo canal de deteccion enlazado para ambos lados).
struct BandComp
{
    float env = 0.0f, atk = 0.0f, rel = 0.0f;

    void prepare (double sampleRate)
    {
        atk = (float) std::exp (-1.0 / (0.010 * sampleRate)); // ataque 10 ms
        rel = (float) std::exp (-1.0 / (0.120 * sampleRate)); // relajacion 120 ms
        env = 0.0f;
    }

    // Devuelve la ganancia (lineal) que hay que aplicar a la banda.
    float process (float level, float thresholdDb, float ratio)
    {
        const float coef = level > env ? atk : rel;
        env = level + coef * (env - level);

        const float db = juce::Decibels::gainToDecibels (env, -100.0f);
        const float over = db - thresholdDb;
        if (over <= 0.0f)
            return 1.0f;

        const float reductionDb = over * (1.0f - 1.0f / ratio);
        return juce::Decibels::decibelsToGain (-reductionDb);
    }
};

class EdyMastericerAudioProcessor : public juce::AudioProcessor
{
public:
    EdyMastericerAudioProcessor();
    ~EdyMastericerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EdyMastericer"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Medidores (los lee la interfaz)
    std::atomic<float> inPeakL { 0.0f }, inPeakR { 0.0f };
    std::atomic<float> outPeakL { 0.0f }, outPeakR { 0.0f };

private:
    using Duplicator = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                      juce::dsp::IIR::Coefficients<float>>;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateEq();

    // Punteros a los parametros
    std::atomic<float>* pInGain = nullptr;
    std::atomic<float>* pOutGain = nullptr;
    std::atomic<float>* pEqLow = nullptr;
    std::atomic<float>* pEqMid = nullptr;
    std::atomic<float>* pEqHigh = nullptr;
    std::atomic<float>* pCompThresh = nullptr;
    std::atomic<float>* pCompRatio = nullptr;
    std::atomic<float>* pWidthLow = nullptr;
    std::atomic<float>* pWidthMid = nullptr;
    std::atomic<float>* pWidthHigh = nullptr;
    std::atomic<float>* pExciter = nullptr;
    std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pCeiling = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pBypass = nullptr;

    double sr = 44100.0;

    // Ecualizador
    Duplicator eqLow, eqMid, eqHigh;
    float cLo = -999.0f, cMid = -999.0f, cHi = -999.0f;

    // Division en 3 bandas (Linkwitz-Riley): graves < 200 Hz < medios < 3 kHz < agudos
    juce::dsp::LinkwitzRileyFilter<float> xo1, xo2, ap2;
    BandComp compLow, compMid, compHigh;
    juce::SmoothedValue<float> wLow, wMid, wHigh;

    // Exciter
    Duplicator exHP;
    juce::AudioBuffer<float> exBuf;
    juce::SmoothedValue<float> exAmt;

    // Limitador final
    juce::dsp::Limiter<float> limiter;

    float lastIn = 1.0f, lastDrive = 1.0f, lastOut = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EdyMastericerAudioProcessor)
};
