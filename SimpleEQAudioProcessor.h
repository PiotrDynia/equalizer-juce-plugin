#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

enum Slope {
    Slope_12,
    Slope_24,
    Slope_36,
    Slope_48
};

struct ChainSettings {
    float peakFreq {0}, peakGainInDecibels {0}, peakQuality {1.f};
    float lowCutFreq {0}, highCutFreq{0};
    Slope lowCutSlope {Slope::Slope_12}, highCutSlope {Slope::Slope_12};
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

//==============================================================================
class SimpleEQAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    SimpleEQAudioProcessor();
    ~SimpleEQAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts {*this, nullptr, "Parameters", createParameterLayout()};

private:
    using Filter = juce::dsp::IIR::Filter<float>;

    // LowPass/HiPass slope - 12/24/36/48
    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;

    // Whole chain - HiPass, BandPass, LowPass
    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;

    using Coefficients = Filter::CoefficientsPtr;

    MonoChain leftChain, rightChain;

    enum ChainPositions {
        LowCut,
        Peak,
        HighCut
    };

    static void updateCoefficients(Coefficients& old, const Coefficients& replacements);
    void updatePeakFilter(const ChainSettings& chainSettings);

    template<int Index, typename ChainType, typename CoefficientType>
    void update(ChainType& chainType, const CoefficientType& coefficients);

    template<typename ChainType, typename CoefficientType>
    void updateCutFilter(ChainType& chainType, const CoefficientType& coefficients, const Slope& slope);

    void updateLowCutFilters(const ChainSettings& chainSettings);
    void updateHighCutFilters(const ChainSettings& chainSettings);
    void updateFilters();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEQAudioProcessor)
};