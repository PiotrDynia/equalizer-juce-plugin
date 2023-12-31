#pragma once

#include "SimpleEQAudioProcessor.h"

struct LookAndFeel : juce::LookAndFeel_V4 {
    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;
};

struct RotarySliderWithLabels : juce::Slider {
    RotarySliderWithLabels(juce::RangedAudioParameter& rap, const juce::String& unitSuffix) : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
                                            juce::Slider::TextEntryBoxPosition::NoTextBox),
                                            param(&rap), suffix(unitSuffix){
        setLookAndFeel(&lnf);
    }

    ~RotarySliderWithLabels() {
        setLookAndFeel(nullptr);
    }

    struct LabelPos {
        float pos;
        juce::String label;
    };

    void paint(juce::Graphics& g) override;
    juce::Rectangle<int> getSliderBounds() const;
    int getTextHeight() const { return 14; }
    juce::String getDisplayString() const;

    juce::Array<LabelPos> labels;
private:
    LookAndFeel lnf;

    juce::RangedAudioParameter* param;
    juce::String suffix;
};

struct ResponseCurveComponent : juce::Component, juce::AudioProcessorParameter::Listener,
                                juce::Timer {
    ResponseCurveComponent(SimpleEQAudioProcessor&);
    ~ResponseCurveComponent();

    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override {}
    void timerCallback() override;
    void paint (juce::Graphics&) override;

private:
    void updateChain();

    SimpleEQAudioProcessor& processorRef;
    juce::Atomic<bool> parametersChanged {false};
    MonoChain monoChain;
};

//==============================================================================
class SimpleEQAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SimpleEQAudioProcessorEditor (SimpleEQAudioProcessor&);
    ~SimpleEQAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;

    std::vector<juce::Component*> getComps();
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SimpleEQAudioProcessor& processorRef;

    RotarySliderWithLabels peakFreqSlider,
    peakGainSlider,
    peakQualitySlider,
    lowCutFreqSlider,
    highCutFreqSlider,
    lowCutSlopeSlider,
    highCutSlopeSlider;

    ResponseCurveComponent responseCurveComponent;

    Attachment peakFreqSliderAttachment,
    peakGainSliderAttachment,
    peakQualitySliderAttachment,
    lowCutFreqSliderAttachment,
    highCutFreqSliderAttachment,
    lowCutSlopeSliderAttachment,
    highCutSlopeSliderAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEQAudioProcessorEditor)
};
