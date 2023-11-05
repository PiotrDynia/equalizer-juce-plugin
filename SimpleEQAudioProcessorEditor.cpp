#include "SimpleEQAudioProcessor.h"
#include "SimpleEQAudioProcessorEditor.h"

void LookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height, float sliderPosProportional,
                                   float rotaryStartAngle, float rotaryEndAngle, juce::Slider &slider) {
    auto bounds = juce::Rectangle<float>(x, y, width, height);

    g.setColour(juce::Colour(97u, 18u, 167u));
    g.fillEllipse(bounds);

    g.setColour(juce::Colour(juce::Colour(255u, 154u, 1u)));
    g.drawEllipse(bounds, 1.f);

    auto center = bounds.getCentre();

    juce::Path p;
    juce::Rectangle<float> r;
    r.setLeft(center.getX() - 2);
    r.setRight(center.getX() + 2);
    r.setTop(bounds.getY());
    r.setBottom(center.getY());
    p.addRectangle(r);

    jassert(rotaryStartAngle < rotaryEndAngle);

    auto sliderAngRad = juce::jmap(sliderPosProportional, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);
    p.applyTransform(juce::AffineTransform().rotated(sliderAngRad, center.getX(), center.getY()));

    g.fillPath(p);
}

//==============================================================================

void RotarySliderWithLabels::paint(juce::Graphics& g) {
    auto startAng = juce::degreesToRadians(180.f + 45.f);
    auto endAng = juce::degreesToRadians(180.f - 45.f) + juce::MathConstants<float>::twoPi;

    auto range = getRange();
    auto sliderBounds = getSliderBounds();

    getLookAndFeel().drawRotarySlider(g, sliderBounds.getX(), sliderBounds.getY(),
                                      sliderBounds.getWidth(), sliderBounds.getHeight(),
                                      static_cast<float>(juce::jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0)),
                                      startAng, endAng, *this);
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const {
    return getLocalBounds();
}

//==============================================================================

ResponseCurveComponent::ResponseCurveComponent(SimpleEQAudioProcessor& p) : processorRef(p) {
    const auto& params = processorRef.getParameters();

    for (auto* param : params) {
        param->addListener(this);
    }

    startTimerHz(60);
}

ResponseCurveComponent::~ResponseCurveComponent() {
    const auto& params = processorRef.getParameters();

    for (auto* param : params) {
        param->removeListener(this);
    }
}


void ResponseCurveComponent::parameterValueChanged(int parameterIndex, float newValue) {
    parametersChanged.set(true);
}

void ResponseCurveComponent::timerCallback() {
    if (parametersChanged.compareAndSetBool(false, true)) {
        // update monochain
        auto chainSettings = getChainSettings(processorRef.apvts);
        auto peakCoefficients = makePeakFilter(chainSettings, processorRef.getSampleRate());
        updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);

        auto lowCutCoefficients = makeLowCutFilter(chainSettings, processorRef.getSampleRate());
        auto highCutCoefficients = makeHighCutFilter(chainSettings, processorRef.getSampleRate());
        updateCutFilter(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
        updateCutFilter(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients, chainSettings.highCutSlope);

        // signal a repaint
        repaint();
    }
}

void ResponseCurveComponent::paint(juce::Graphics& g) {
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (juce::Colours::black);

    auto responseArea = getLocalBounds();
    auto width = responseArea.getWidth();

    auto& lowCut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highCut = monoChain.get<ChainPositions::HighCut>();

    auto sampleRate = processorRef.getSampleRate();

    std::vector<double> mags;
    mags.resize(static_cast<unsigned long>(width));

    for (int i = 0; i < width; ++i) {
        double mag = 1.f;
        auto freq = juce::mapToLog10(static_cast<double>(i) / static_cast<double>(width), 20.0, 20000.0);

        if (!monoChain.isBypassed<ChainPositions::Peak>()) {
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }

        if (!lowCut.isBypassed<0>()) {
            mag *= lowCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        if (!lowCut.isBypassed<1>()) {
            mag *= lowCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        if (!lowCut.isBypassed<2>()) {
            mag *= lowCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        if (!lowCut.isBypassed<3>()) {
            mag *= lowCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }

        if (!highCut.isBypassed<0>()) {
            mag *= highCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        if (!highCut.isBypassed<1>()) {
            mag *= highCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        if (!highCut.isBypassed<2>()) {
            mag *= highCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        if (!highCut.isBypassed<3>()) {
            mag *= highCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }

        mags[static_cast<unsigned long>(i)] = juce::Decibels::gainToDecibels(mag);
    }

    juce::Path responseCurve;

    const double outputMin = responseArea.getBottom();
    const double outputMax = responseArea.getY();
    auto map = [outputMin, outputMax](double input) {
        return juce::jmap(input, -24.0, 24.0, outputMin, outputMax);
    };

    responseCurve.startNewSubPath(static_cast<float>(responseArea.toFloat().getX()), static_cast<float>(map(mags.front())));

    for (size_t i = 1; i < mags.size(); ++i) {
        responseCurve.lineTo(static_cast<float>(responseArea.getX() + i), static_cast<float>(map(mags[i])));
    }

    g.setColour(juce::Colours::orange);
    g.drawRoundedRectangle(responseArea.toFloat(), 4.f, 1.f);
    g.setColour(juce::Colours::white);
    g.strokePath(responseCurve, juce::PathStrokeType(2.f));
}

//==============================================================================
SimpleEQAudioProcessorEditor::SimpleEQAudioProcessorEditor (SimpleEQAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p),
      peakFreqSlider(*processorRef.apvts.getParameter("Peak Freq"), "Hz"),
      peakGainSlider(*processorRef.apvts.getParameter("Peak Gain"), "dB"),
      peakQualitySlider(*processorRef.apvts.getParameter("Peak Quality"), ""),
      lowCutFreqSlider(*processorRef.apvts.getParameter("LowCut Freq"), "Hz"),
      highCutFreqSlider(*processorRef.apvts.getParameter("HighCut Freq"), "Hz"),
      lowCutSlopeSlider(*processorRef.apvts.getParameter("LowCut Slope"), "dB/Oct"),
      highCutSlopeSlider(*processorRef.apvts.getParameter("HighCut Slope"), "dB/Oct"),
      responseCurveComponent(processorRef),
      peakFreqSliderAttachment(processorRef.apvts, "Peak Freq", peakFreqSlider),
      peakGainSliderAttachment(processorRef.apvts, "Peak Gain", peakGainSlider),
      peakQualitySliderAttachment(processorRef.apvts, "Peak Quality", peakQualitySlider),
      lowCutFreqSliderAttachment(processorRef.apvts, "LowCut Freq", lowCutFreqSlider),
      highCutFreqSliderAttachment(processorRef.apvts, "HighCut Freq", highCutFreqSlider),
      lowCutSlopeSliderAttachment(processorRef.apvts, "LowCut Slope", lowCutSlopeSlider),
      highCutSlopeSliderAttachment(processorRef.apvts, "HighCut Slope", highCutSlopeSlider)
{
    juce::ignoreUnused (processorRef);

    for (auto* comp : getComps()) {
        addAndMakeVisible(comp);
    }

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (600, 400);
}

SimpleEQAudioProcessorEditor::~SimpleEQAudioProcessorEditor()
{

}

//==============================================================================
void SimpleEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (juce::Colours::black);
}

void SimpleEQAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto bounds = getLocalBounds();
    auto responseArea = bounds.removeFromTop(static_cast<int>(bounds.getHeight() * 0.33));

    responseCurveComponent.setBounds(responseArea);

    auto lowCutArea = bounds.removeFromLeft(static_cast<int>(bounds.getWidth() * 0.33));
    auto highCutArea = bounds.removeFromRight(static_cast<int>(bounds.getWidth() * 0.5));
    auto peakFreqArea = bounds.removeFromTop(static_cast<int>(bounds.getHeight() * 0.33));
    auto peakGainArea = bounds.removeFromTop(static_cast<int>(bounds.getHeight() * 0.5));

    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(static_cast<int>(lowCutArea.getHeight() * 0.5)));
    lowCutSlopeSlider.setBounds(lowCutArea);
    highCutFreqSlider.setBounds(highCutArea.removeFromTop(static_cast<int>(highCutArea.getHeight() * 0.5)));
    highCutSlopeSlider.setBounds(highCutArea);
    peakFreqSlider.setBounds(peakFreqArea);
    peakGainSlider.setBounds(peakGainArea);
    peakQualitySlider.setBounds(bounds);
}

std::vector<juce::Component*> SimpleEQAudioProcessorEditor::getComps() {
    return {
        &peakFreqSlider,
        &peakGainSlider,
        &peakQualitySlider,
        &lowCutFreqSlider,
        &highCutFreqSlider,
        &lowCutSlopeSlider,
        &highCutSlopeSlider,
        &responseCurveComponent
    };
}
