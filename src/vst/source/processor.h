//------------------------------------------------------------------------
// Copyright(c) 2025 My Plug-in Company.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "../../lib/oscillators/oscillator.hpp" // For WaveType enum

#include <memory>

// Forward declarations to avoid include conflicts
namespace synth
{
	class PolyphonicOscillator;
}

namespace ModularSistemas
{

	//------------------------------------------------------------------------
	//  ObsSynthProcessor
	//------------------------------------------------------------------------
	class ObsSynthProcessor : public Steinberg::Vst::AudioEffect
	{
	public:
		ObsSynthProcessor();
		~ObsSynthProcessor() SMTG_OVERRIDE;

		// Create function
		static Steinberg::FUnknown *createInstance(void * /*context*/)
		{
			return (Steinberg::Vst::IAudioProcessor *)new ObsSynthProcessor;
		}

		//--- ---------------------------------------------------------------------
		// AudioEffect overrides:
		//--- ---------------------------------------------------------------------
		/** Called at first after constructor */
		Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown *context) SMTG_OVERRIDE;

		/** Called at the end before destructor */
		Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;

		/** Switch the Plug-in on/off */
		Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) SMTG_OVERRIDE;

		/** Will be called before any process call */
		Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup &newSetup) SMTG_OVERRIDE;

		/** Asks if a given sample size is supported see SymbolicSampleSizes. */
		Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;

		/** Here we go...the process call */
		Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData &data) SMTG_OVERRIDE;

		/** For persistence */
		Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream *state) SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream *state) SMTG_OVERRIDE;

		/** For receiving messages from controller */
		Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage *message) SMTG_OVERRIDE;

		//------------------------------------------------------------------------
	protected:
		std::unique_ptr<synth::PolyphonicOscillator> polyphonicOscillator_;
		float sampleRate_;

		// Parameter values - all normalized [0.0, 1.0] from VST3
		float masterVolume_ = 0.7f;
		float masterPan_ = 0.5f;		// 0=Left, 0.5=Center, 1.0=Right
		float waveform_ = 0.0f;			// 0-4 index for waveform type
		float oscillatorVolume_ = 0.8f; // 0.0-1.0 oscillator volume
		float attackTime_ = 0.03f;		// 0.0-1.0 maps to 0.0-5.0 seconds
		float decayTime_ = 0.07f;		// 0.0-1.0 maps to 0.0-5.0 seconds
		float sustainLevel_ = 0.5f;		// 0.0-1.0 sustain level
		float releaseTime_ = 0.07f;		// 0.0-1.0 maps to 0.0-10.0 seconds
		float dryWetMix_ = 1.0f;		// 0.0=Dry, 1.0=Wet (envelope processed)

		// FM Synthesis Parameters
		float fmModulatorRatio_ = 0.1f;	 // 0.0-1.0 maps to 0.1-10.0 ratio
		float fmModulationIndex_ = 0.1f; // 0.0-1.0 maps to 0.0-20.0 index
		float fmAlgorithm_ = 0.0f;		 // 0.0-1.0 maps to 0-4 algorithm index
		float fmOperator2Ratio_ = 0.1f;	 // 0.0-1.0 maps to 0.1-10.0 ratio
		float fmOperator2Level_ = 0.5f;	 // 0.0-1.0 operator level
		float fmOperator3Ratio_ = 0.1f;	 // 0.0-1.0 maps to 0.1-10.0 ratio
		float fmOperator3Level_ = 0.5f;	 // 0.0-1.0 operator level
		float fmOperator4Ratio_ = 0.1f;	 // 0.0-1.0 maps to 0.1-10.0 ratio
		float fmOperator4Level_ = 0.5f;	 // 0.0-1.0 operator level
		float fmFeedback_ = 0.0f;		 // 0.0-1.0 feedback amount

		// Wave Table Parameters
		float waveTablePosition_ = 0.0f; // 0.0-1.0 position in wave table bank
		float waveTableMorph_ = 0.5f;	 // 0.0-1.0 morphing speed
		float pcmMix_ = 0.0f;			 // 0.0-1.0 PCM/basic waveform mix
		float fmMix_ = 0.0f;			 // 0.0-1.0 FM/basic oscillator mix

		// LFO and Modulation Parameters
		float lfoRate_ = 0.2f;	// 0.0-1.0 maps to 0.1-20.0 Hz
		float lfoDepth_ = 0.0f; // 0.0-1.0 LFO modulation depth

		// Filter Parameters
		float filterCutoff_ = 1.0f;	   // 0.0-1.0 maps to 20-20000 Hz
		float filterResonance_ = 0.0f; // 0.0-1.0 filter resonance

		// Effects Parameters
		float chorusRate_ = 0.3f;  // 0.0-1.0 chorus LFO rate
		float chorusDepth_ = 0.0f; // 0.0-1.0 chorus modulation depth
		float reverbRoom_ = 0.5f;  // 0.0-1.0 reverb room size
		float reverbMix_ = 0.0f;   // 0.0-1.0 reverb dry/wet mix
		float overdrive_ = 0.0f;   // 0.0-1.0 overdrive amount
		float globalMix_ = 1.0f;   // 0.0-1.0 global output level

		// Helper function to convert waveform parameter index to WaveType
		synth::WaveType convertWaveformIndex(int index);

		// Helper functions to convert normalized parameters to actual values
		float convertAttackTime(float normalized) const;
		float convertDecayTime(float normalized) const;
		float convertReleaseTime(float normalized) const;
		int convertWaveformValue(float normalized) const;

		// FM parameter conversion helpers
		float convertFMRatio(float normalized) const;	 // 0.0-1.0 to 0.1-10.0
		float convertFMIndex(float normalized) const;	 // 0.0-1.0 to 0.0-20.0
		int convertFMAlgorithm(float normalized) const;	 // 0.0-1.0 to 0-4
		float convertFMLevel(float normalized) const;	 // 0.0-1.0 to 0.0-1.0 (direct mapping)
		float convertFMFeedback(float normalized) const; // 0.0-1.0 to 0.0-1.0 (direct mapping)

		// Additional parameter conversion helpers
		float convertLFORate(float normalized) const;	   // 0.0-1.0 to 0.1-20.0 Hz
		float convertFilterCutoff(float normalized) const; // 0.0-1.0 to 20-20000 Hz
		float convertChorusRate(float normalized) const;   // 0.0-1.0 to 0.1-10.0 Hz
	};

	//------------------------------------------------------------------------
} // namespace ModularSistemas
