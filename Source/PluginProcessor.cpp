/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
TapSynthAudioProcessor::TapSynthAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts (*this, nullptr, "Parameters", createParams())
#endif
{
    synth.addSound (new SynthSound());
    
    for (int i = 0; i < numVoices; i++)
    {
        synth.addVoice (new SynthVoice());
    }
    
    for (int ch = 0; ch < numChannelsToProcess; ++ch)
    {
        filter[ch].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    }
}

TapSynthAudioProcessor::~TapSynthAudioProcessor()
{
    
}

//==============================================================================
const juce::String TapSynthAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TapSynthAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool TapSynthAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool TapSynthAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double TapSynthAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TapSynthAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int TapSynthAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TapSynthAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String TapSynthAudioProcessor::getProgramName (int index)
{
    return {};
}

void TapSynthAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void TapSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    
    for (int i = 0; i < synth.getNumVoices(); i++)
    {
        if (auto voice = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
        {
            voice->prepareToPlay (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
        }
    }
    
    for (int ch = 0; ch < numChannelsToProcess; ++ch)
    {
        filter[ch].prepareToPlay (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    }
}

void TapSynthAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TapSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void TapSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    setParams();
    
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
    
    for (int ch = 0; ch < numChannelsToProcess; ++ch)
    {
        auto* output = buffer.getWritePointer (ch);
        
        for (int s = 0; s < buffer.getNumSamples(); ++s)
        {
            output[s] = filter[ch].processNextSample (ch, buffer.getSample (ch, s));
        }
    }
}

//==============================================================================
bool TapSynthAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* TapSynthAudioProcessor::createEditor()
{
    return new TapSynthAudioProcessorEditor (*this);
}

//==============================================================================
void TapSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void TapSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapSynthAudioProcessor();
}

juce::AudioProcessorValueTreeState::ParameterLayout TapSynthAudioProcessor::createParams()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // OSC select
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("OSC1", "Oscillator 1", juce::StringArray { "Sine", "Saw", "Square" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("OSC2", "Oscillator 2", juce::StringArray { "Sine", "Saw", "Square" }, 0));
    
    // OSC Gain
    params.push_back (std::make_unique<juce::AudioParameterFloat>("OSC1GAIN", "Oscillator 1 Gain", juce::NormalisableRange<float> { -40.0f, 0.2f, 0.1f }, 0.1f, "dB"));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("OSC2GAIN", "Oscillator 2 Gain", juce::NormalisableRange<float> { -40.0f, 0.2f, 0.1f }, 0.1f, "dB"));
    
    // OSC Pitch val
    params.push_back (std::make_unique<juce::AudioParameterInt>("OSC1PITCH", "Oscillator 1 Pitch", -48, 48, 0));
    params.push_back (std::make_unique<juce::AudioParameterInt>("OSC2PITCH", "Oscillator 2 Pitch", -48, 48, 0));
    
    // FM Osc Freq
    params.push_back (std::make_unique<juce::AudioParameterFloat>("OSC1FMFREQ", "Oscillator 1 FM Frequency", juce::NormalisableRange<float> { 0.0f, 1000.0f, 0.1f }, 0.0f, "Hz"));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("OSC2FMFREQ", "Oscillator 1 FM Frequency", juce::NormalisableRange<float> { 0.0f, 1000.0f, 0.1f }, 0.0f, "Hz"));
    
    // FM Osc Depth
    params.push_back (std::make_unique<juce::AudioParameterFloat>("OSC1FMDEPTH", "Oscillator 1 FM Depth", juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, ""));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("OSC2FMDEPTH", "Oscillator 2 FM Depth", juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, ""));
    
    // LFO
    params.push_back (std::make_unique<juce::AudioParameterFloat>("LFO1FREQ", "LFO1 Frequency", juce::NormalisableRange<float> { 0.0f, 20.0f, 0.1f }, 0.0f, "Hz"));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("LFO1DEPTH", "LFO1 Depth", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.1f }, 0.0f, ""));
    
    //Filter
    params.push_back (std::make_unique<juce::AudioParameterChoice>("FILTERTYPE", "Filter Type", juce::StringArray { "Low Pass", "Band Pass", "High Pass" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("FILTERCUTOFF", "Filter Cutoff", juce::NormalisableRange<float> { 20.0f, 20000.0f, 0.1f, 0.6f }, 20000.0f, "Hz"));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("FILTERRESONANCE", "Filter Resonance", juce::NormalisableRange<float> { 0.1f, 2.0f, 0.1f }, 0.1f, ""));
    
    // ADSR
    params.push_back (std::make_unique<juce::AudioParameterFloat>("ATTACK", "Attack", juce::NormalisableRange<float> { 0.1f, 1.0f, 0.1f }, 0.1f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("DECAY", "Decay", juce::NormalisableRange<float> { 0.1f, 1.0f, 0.1f }, 0.1f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("SUSTAIN", "Sustain", juce::NormalisableRange<float> { 0.1f, 1.0f, 0.1f }, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("RELEASE", "Release", juce::NormalisableRange<float> { 0.1f, 3.0f, 0.1f }, 0.4f));
    
    return { params.begin(), params.end() };
}

void TapSynthAudioProcessor::setParams()
{
    setVoiceParams();
    setFilterParams();
}

void TapSynthAudioProcessor::setVoiceParams()
{
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto voice = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
        {
            auto& attack = *apvts.getRawParameterValue ("ATTACK");
            auto& decay = *apvts.getRawParameterValue ("DECAY");
            auto& sustain = *apvts.getRawParameterValue ("SUSTAIN");
            auto& release = *apvts.getRawParameterValue ("RELEASE");
            
            auto& osc1Choice = *apvts.getRawParameterValue ("OSC1");
            auto& osc2Choice = *apvts.getRawParameterValue ("OSC2");
            auto& osc1Gain = *apvts.getRawParameterValue ("OSC1GAIN");
            auto& osc2Gain = *apvts.getRawParameterValue ("OSC2GAIN");
            auto& osc1Pitch = *apvts.getRawParameterValue ("OSC1PITCH");
            auto& osc2Pitch = *apvts.getRawParameterValue ("OSC2PITCH");
            auto& osc1FmFreq = *apvts.getRawParameterValue ("OSC1FMFREQ");
            auto& osc2FmFreq = *apvts.getRawParameterValue ("OSC2FMFREQ");
            auto& osc1FmDepth = *apvts.getRawParameterValue ("OSC1FMDEPTH");
            auto& osc2FmDepth = *apvts.getRawParameterValue ("OSC2FMDEPTH");

            auto& osc1 = voice->getOscillator1();
            auto& osc2 = voice->getOscillator2();
            
            auto& adsr = voice->getAdsr();
           
            for (int i = 0; i < getTotalNumOutputChannels(); i++)
            {
                osc1[i].setParams (osc1Choice, osc1Gain, osc1Pitch, osc1FmFreq, osc1FmDepth);
                osc2[i].setParams (osc2Choice, osc2Gain, osc2Pitch, osc2FmFreq, osc2FmDepth);
            }
            
            adsr.update (attack.load(), decay.load(), sustain.load(), release.load());
        }
    }
}

void TapSynthAudioProcessor::setFilterParams()
{
    auto& filterType = *apvts.getRawParameterValue ("FILTERTYPE");
    auto& filterCutoff = *apvts.getRawParameterValue ("FILTERCUTOFF");
    auto& filterResonance = *apvts.getRawParameterValue ("FILTERRESONANCE");
    
    auto& lfoFreq = *apvts.getRawParameterValue ("LFO1FREQ");
    auto& lfoDepth = *apvts.getRawParameterValue ("LFO1DEPTH");
    
    for (int ch = 0; ch < numChannelsToProcess; ++ch)
    {
        filter[ch].setParams (filterType, filterCutoff, filterResonance);
        filter[ch].setLfoParams (lfoFreq, lfoDepth);
    }
}
