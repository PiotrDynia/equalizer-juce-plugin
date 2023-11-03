#include "SimpleEQAudioProcessor.h"
#include "SimpleEQAudioProcessorEditor.h"

//==============================================================================
SimpleEQAudioProcessor::SimpleEQAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
}

SimpleEQAudioProcessor::~SimpleEQAudioProcessor() {
}

//==============================================================================
const juce::String SimpleEQAudioProcessor::getName() const {
    return JucePlugin_Name;
}

bool SimpleEQAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool SimpleEQAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool SimpleEQAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double SimpleEQAudioProcessor::getTailLengthSeconds() const {
    return 0.0;
}

int SimpleEQAudioProcessor::getNumPrograms() {
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleEQAudioProcessor::getCurrentProgram() {
    return 0;
}

void SimpleEQAudioProcessor::setCurrentProgram(int index) {
    juce::ignoreUnused(index);
}

const juce::String SimpleEQAudioProcessor::getProgramName(int index) {
    juce::ignoreUnused(index);
    return {};
}

void SimpleEQAudioProcessor::changeProgramName(int index, const juce::String& newName) {
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void SimpleEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused(sampleRate, samplesPerBlock);

    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = static_cast<unsigned int>(samplesPerBlock);
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;
    leftChain.prepare(spec);
    rightChain.prepare(spec);

    updateFilters();
}

void SimpleEQAudioProcessor::releaseResources() {
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool SimpleEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}

void SimpleEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages) {
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
}

    updateFilters();

    juce::dsp::AudioBlock<float> block(buffer);

    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftContext (leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext (rightBlock);

    leftChain.process(leftContext);
    rightChain.process(rightContext);
}

//==============================================================================
bool SimpleEQAudioProcessor::hasEditor() const {
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SimpleEQAudioProcessor::createEditor()
{
    return new SimpleEQAudioProcessorEditor (*this);
//    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void SimpleEQAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused(destData);

    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void SimpleEQAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused(data, sizeInBytes);

    auto tree = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));

    if (tree.isValid()) {
        apvts.replaceState(tree);
        updateFilters();
    }
}

void SimpleEQAudioProcessor::updateFilters() {
    auto chainSettings = getChainSettings(apvts);
    updateLowCutFilters(chainSettings);
    updatePeakFilter(chainSettings);
    updateHighCutFilters(chainSettings);
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
    ChainSettings chainSettings;

    chainSettings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();
    chainSettings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load();
    chainSettings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
    chainSettings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    chainSettings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();
    chainSettings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    chainSettings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());

    return chainSettings;
}

void SimpleEQAudioProcessor::updateLowCutFilters(const ChainSettings &chainSettings) {
    // order parameter - 2 for 12dB, 4 for 24 dB, etc...
    auto lowCutCoefficients =
            juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
                                                                                        getSampleRate(),
                                                                                        2 * (chainSettings.lowCutSlope + 1));
    auto& lowLeftCut = leftChain.get<ChainPositions::LowCut>();
    auto& lowRightCut = rightChain.get<ChainPositions::LowCut>();

    updateCutFilter(lowLeftCut, lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(lowRightCut, lowCutCoefficients, chainSettings.lowCutSlope);
}

void SimpleEQAudioProcessor::updatePeakFilter(const ChainSettings &chainSettings) {
    auto peakCoefficients =
            juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(),
                                                                chainSettings.peakFreq,
                                                                chainSettings.peakQuality,
                                                                juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));

    updateCoefficients(leftChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
}

void SimpleEQAudioProcessor::updateHighCutFilters(const ChainSettings &chainSettings) {
    auto highCutCoefficients =
            juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq,
                                                                                       getSampleRate(),
                                                                                       2 * (chainSettings.highCutSlope + 1));

    auto& highLeftCut = leftChain.get<ChainPositions::HighCut>();
    auto& highRightCut = rightChain.get<ChainPositions::HighCut>();

    updateCutFilter(highLeftCut, highCutCoefficients, chainSettings.highCutSlope);
    updateCutFilter(highRightCut, highCutCoefficients, chainSettings.highCutSlope);
}

template<typename ChainType, typename CoefficientType>
void SimpleEQAudioProcessor::updateCutFilter(ChainType& chainType, const CoefficientType& coefficients, const Slope& slope) {
    chainType.template setBypassed<0>(true);
    chainType.template setBypassed<1>(true);
    chainType.template setBypassed<2>(true);
    chainType.template setBypassed<3>(true);

    switch (slope) {
        case Slope_48: {
            update<3>(chainType, coefficients);
        }
        case Slope_36: {
            update<2>(chainType, coefficients);
        }
        case Slope_24: {
            update<1>(chainType, coefficients);
        }
        case Slope_12: {
            update<0>(chainType, coefficients);
        }
    }
}

template<int Index, typename ChainType, typename CoefficientType>
void SimpleEQAudioProcessor::update(ChainType& chainType, const CoefficientType& coefficients) {
    updateCoefficients(chainType.template get<Index>().coefficients, coefficients[Index]);
    chainType.template setBypassed<Index>(false);
}

void SimpleEQAudioProcessor::updateCoefficients(SimpleEQAudioProcessor::Coefficients &old,
                                                const SimpleEQAudioProcessor::Coefficients &replacements) {
    *old = *replacements;
}

juce::AudioProcessorValueTreeState::ParameterLayout SimpleEQAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq", "LowCut Freq", juce::NormalisableRange<float>(
            20.f, 20000.f, 1.f, 0.25f), 20.f));

    layout.add(
            std::make_unique<juce::AudioParameterFloat>("HighCut Freq", "HighCut Freq", juce::NormalisableRange<float>(
                    20.f, 20000.f, 1.f, 0.25f), 20000.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Freq", "Peak Freq", juce::NormalisableRange<float>(
            20.f, 20000.f, 1.f, 0.25f), 750.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Gain", "Peak Gain", juce::NormalisableRange<float>(
            -24.f, 24.f, 0.5f, 1.f), 0.0f));

    layout.add(
            std::make_unique<juce::AudioParameterFloat>("Peak Quality", "Peak Quality", juce::NormalisableRange<float>(
                    0.1f, 10.f, 0.05f, 1.f), 1.f));

    juce::StringArray stringArray;

    for (int i = 0; i < 4; ++i) {
        juce::String str;
        str << (12 + i * 12);
        str << " db/Oct";
        stringArray.add(str);
    }

    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "LowCut Slope", stringArray, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "HighCut Slope", stringArray, 0));

    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
    return new SimpleEQAudioProcessor();
}
