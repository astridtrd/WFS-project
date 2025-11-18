/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <fstream>  
#include <vector>
#include <sstream>
#include <string>

//==============================================================================
PFE_WFS_simpleAudioProcessor::PFE_WFS_simpleAudioProcessor()
     : AudioProcessor (BusesProperties() 
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)                   
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)                    
                       ),
    params(apvts, _speakerMask, NUM_OUTPUT)
{    
    for (int i = 0; i < NUM_OUTPUT; i++) _speakerMask[i] = 0.0f;
    
    params._loudspeakerLayout = &_loudspeakerLayout_config48L;

}

//==============================================================================
const juce::String PFE_WFS_simpleAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PFE_WFS_simpleAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PFE_WFS_simpleAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PFE_WFS_simpleAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PFE_WFS_simpleAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PFE_WFS_simpleAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PFE_WFS_simpleAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PFE_WFS_simpleAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String PFE_WFS_simpleAudioProcessor::getProgramName (int index)
{
    return {};
}

void PFE_WFS_simpleAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================


std::vector<std::vector<float>> PFE_WFS_simpleAudioProcessor::loadCoefficients(const std::string& filePath)
{

    std::vector<std::vector<float>> filtersCoefficients(16, std::vector<float>(960, 0.0f)); // vectors containing the coefficients

    std::string line;

    int filterIndex = 0; // filter index (0-15)
    int coefficientIndex = 0; // coefficient Index (0-959)


    std::ifstream file(filePath);

    // Verify if the file is corectly open
    if (!file.is_open()) {
        std::cerr << "Error opening file" << std::endl;
        return filtersCoefficients; 
    }



    while (std::getline(file, line) && filterIndex < 16)
    {
        std::stringstream ss(line);
        float coefficient;
        if (ss >> coefficient) {
            filtersCoefficients[filterIndex][coefficientIndex] = coefficient;
            coefficientIndex++;
        }

        // if we reach 960 coeffs, next filter
        if (coefficientIndex == 960) {
            coefficientIndex = 0; // Reinitialize index for next filter
            filterIndex++; // next filter
        }
    }

    file.close();
    return filtersCoefficients;
}

std::vector<float> PFE_WFS_simpleAudioProcessor::loadFIRCoefficients(const std::string& filePath)
{
    std::vector<float> Coefficients(257, 0.0f);
    int coefficientIndex = 0;
    std::string line;
    std::ifstream file(filePath);

    // verify if file is correctly open
    if (!file.is_open()) {
        std::cerr << "Error opening file" << std::endl;
        return {}; 
    }

    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        float coefficient;
        if (ss >> coefficient) {
            Coefficients[coefficientIndex] = coefficient;
            coefficientIndex++;
        }

    }
    file.close();

    return Coefficients;
}


    








void PFE_WFS_simpleAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = juce::uint32(samplesPerBlock);
    spec.numChannels = 1;
    _wfsDelayLine.prepare(spec);

    double numSamples = (MAX_DISTANCE / 340.0f) * sampleRate;
    int maxDelayInSamples = int(std::ceil(numSamples));
    _wfsDelayLine.setMaximumDelayInSamples(maxDelayInSamples);
    _wfsDelayLine.reset();

    //=========== eq filter
    
    // get path to coefficients for eq filter
    juce::File coefficientsFile ("C:/Users/astri/OneDrive/Documents/M2/PC WFS/plugin/PFE_WFS_simple_starter/Source/output.txt");//load your own path

    // load the coefficients for eq filter
    std::vector<std::vector<float>> eqCoeffs = loadCoefficients(coefficientsFile.getFullPathName().toStdString());

    //Initialization of all of the 16 equalization filters
    for (int i = 0; i<16; i++)
    {
        eqFilters[i].coefficients = juce::dsp::FIR::Coefficients<float>::Ptr(
            new juce::dsp::FIR::Coefficients<float>(eqCoeffs[i].data(), eqCoeffs.size())
        );

        eqFilters[i].reset();
    
        eqFilters[i].prepare(spec);
    }

    //=========== fir filters (high pass and phase) 
    // 
    // Get the path of the coeff file for fir filter
    juce::File coefficientsFir("C:/Users/astri/OneDrive/Documents/M2/PC WFS/plugin/PFE_WFS_simple_starter/Source/FIR_b_coeffs.txt"); // load your own path
    //
    // Load the coefficients for fir filter
    std::vector<float> firCoeffs = loadFIRCoefficients(coefficientsFir.getFullPathName().toStdString());
    // Initialize the fir filters
    for (int i = 0; i < 16; i++)
    {
 
        firFilters[i].coefficients = juce::dsp::FIR::Coefficients<float>::Ptr(
            new juce::dsp::FIR::Coefficients<float>(firCoeffs.data(), firCoeffs.size())
        );
        firFilters[i].reset();

        firFilters[i].prepare(spec);
    }



}

void PFE_WFS_simpleAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PFE_WFS_simpleAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return true;
}
#endif



void PFE_WFS_simpleAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    //Creates gain table
    juce::Array<float> gainArray;
    gainArray = params.calculateGains();
    float gain;

    //Creates attenuation table
    juce::Array<float> attArray;
    attArray = params.calculateAttenuations();
    float att;
   
    // Update the parameters value in the AudioProcessorValueTreeState instance
    params.update();
    float sampleRate = float(getSampleRate());

    // Get the channel 0 for mono input
    float* input = buffer.getWritePointer(0);
        
    // Save the read postion in the ring buffer (delay line)
    // because the read position is incremented at each popSAmple() call (3rd argument == true)
    // => For each channel, the initial value of the read position will be restored
    int intialReadpos = _wfsDelayLine.getReadPos(0);
    
    // Loop on each output channels
    for (int channel = 0; channel < totalNumOutputChannels; channel++) {

        //

        // Get the buffer for the output channel (after conversion to get the right channel in Reaper)
        float *output = buffer.getWritePointer(channelMappingReaper48[channel]);

        // we set the fractionnal delay in sample
        float delaySample = params._pWfsDelay[channel] * sampleRate;

        //gain for this channel
        gain = gainArray[channel];
        //attenuation for this channel
        att = attArray[channel];
        
        // loop on each samples for a givent channel
        for (int sample = 0; sample < buffer.getNumSamples(); sample++) {
            
            // update smoothing fo parameter and push sample of the input buffer in the delay line
            // Only for the channel 0 !
            if (channel == 0) {
                params.smoothen(); 
                _wfsDelayLine.pushSample(0, input[sample]);
            }
            
            if (params.wfsFilter == 1.0 && params.EQ == 0.0) { // if the gain button is activated
                float inSamplef = _wfsDelayLine.popSample(0, delaySample, true) * (float)params._speakerMask[channel] * gain * att;
                output[sample] = firFilters[channel].processSample(inSamplef);
            }
            

            else if (params.wfsFilter == 0.0 && params.EQ == 1.0) { //if the passe-haut button is activated (here it is for 
                float inSamplef = _wfsDelayLine.popSample(0, delaySample, true) * (float)params._speakerMask[channel] * gain * att;
                output[sample] = eqFilters[channel].processSample(inSamplef);
            }
            
            else if (params.wfsFilter == 1.0 && params.EQ == 1.0) { //if both buttons are activated
                float inSamplef = _wfsDelayLine.popSample(0, delaySample, true) * (float)params._speakerMask[channel] * gain * att;
                inSamplef = firFilters[channel].processSample(inSamplef);
                output[sample] = eqFilters[channel].processSample(inSamplef);
            }
            
            
            else {
                output[sample] = (_wfsDelayLine.popSample(0, delaySample, true) * (float)params._speakerMask[channel]);
            }

        }


        // Restore the initial value of the read position
        // Not for the last channel : the read position muste updated for the next processBlock() call (nex sample block)
        if (channel < totalNumOutputChannels - 1)
            _wfsDelayLine.setReadPos(0, intialReadpos);
    }
}

//==============================================================================
bool PFE_WFS_simpleAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PFE_WFS_simpleAudioProcessor::createEditor()
{
    return new PFE_WFS_simpleAudioProcessorEditor (*this, &params);
}

//==============================================================================
void PFE_WFS_simpleAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Serialize the value of all parameters 
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void PFE_WFS_simpleAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Deserialize the value of all parameters 
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PFE_WFS_simpleAudioProcessor();
}
