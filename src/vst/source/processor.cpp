//------------------------------------------------------------------------
// Copyright(c) 2025 My Plug-in Company.
//------------------------------------------------------------------------

#include "processor.h"
#include "cids.h"
#include "parameters.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstevents.h"

#include <memory>
#include <vector>
#include <array>

// Temporarily undefine any conflicting macros before including synth headers
#ifdef RELEASE
#undef RELEASE
#endif

// Include synth headers after VST headers to avoid conflicts
#include "../../lib/oscillators/polyphonic_oscillator.hpp"
#include "../../lib/oscillators/pcm_waveform.hpp"
#include <cstring>

using namespace Steinberg;

// UI-only control tags (must match controller.cpp)
enum UIControlTags : int32_t
{
	kPCMSelector = 108 // PCM file selector dropdown
};

namespace ModularSistemas
{
	//------------------------------------------------------------------------
	// ObsSynthProcessor
	//------------------------------------------------------------------------
	ObsSynthProcessor::ObsSynthProcessor()
		: sampleRate_(44100.0f) // Default sample rate
	{
		//--- set the wanted controller for our processor
		setControllerClass(kObsSynthControllerUID);
	}

	//------------------------------------------------------------------------
	ObsSynthProcessor::~ObsSynthProcessor()
	{
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthProcessor::initialize(FUnknown *context)
	{
		// Here the Plug-in will be instantiated

		//---always initialize the parent-------
		tresult result = AudioEffect::initialize(context);
		// if everything Ok, continue
		if (result != kResultOk)
		{
			return result;
		}

		//--- create Audio IO ------
		// No audio input needed for an instrument
		addAudioOutput(STR16("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);

		/* If you don't need an event bus, you can remove the next line */
		addEventInput(STR16("Event In"), 1);

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthProcessor::terminate()
	{
		// Here the Plug-in will be de-instantiated, last possibility to remove some memory!

		//---do not forget to call parent ------
		return AudioEffect::terminate();
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthProcessor::setActive(TBool state)
	{
		//--- called when the Plug-in is enable/disable (On/Off) -----
		if (state)
		{
			// Plugin is being activated - initialize the oscillator if not already done
			if (!polyphonicOscillator_)
			{
				polyphonicOscillator_ = std::make_unique<synth::PolyphonicOscillator>(synth::WaveType::SINE, sampleRate_);
				// TODO: Apply current parameter values
			}
		}
		else
		{
			// Plugin is being deactivated - stop all notes
			if (polyphonicOscillator_)
			{
				polyphonicOscillator_->allNotesOff();
			}
		}

		return AudioEffect::setActive(state);
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthProcessor::process(Vst::ProcessData &data)
	{
		//--- Process MIDI events first -----------
		if (data.inputEvents && polyphonicOscillator_)
		{
			int32 numEvents = data.inputEvents->getEventCount();
			for (int32 i = 0; i < numEvents; i++)
			{
				Vst::Event event;
				if (data.inputEvents->getEvent(i, event) == kResultOk)
				{
					switch (event.type)
					{
					case Vst::Event::kNoteOnEvent:
					{
						auto &noteOn = event.noteOn;
						uint8_t noteNumber = static_cast<uint8_t>(noteOn.pitch);
						float velocity = static_cast<float>(noteOn.velocity);
						polyphonicOscillator_->noteOn(noteNumber, velocity);
					}
					break;

					case Vst::Event::kNoteOffEvent:
					{
						auto &noteOff = event.noteOff;
						uint8_t noteNumber = static_cast<uint8_t>(noteOff.pitch);
						polyphonicOscillator_->noteOff(noteNumber);
					}
					break;
					}
				}
			}
		}

		//--- Process parameter changes -----------
		if (data.inputParameterChanges)
		{
			int32 numParamsChanged = data.inputParameterChanges->getParameterCount();
			for (int32 index = 0; index < numParamsChanged; index++)
			{
				if (auto *paramQueue = data.inputParameterChanges->getParameterData(index))
				{
					Vst::ParamValue value;
					int32 sampleOffset;
					int32 numPoints = paramQueue->getPointCount();

					// Get the last parameter value in the buffer
					if (paramQueue->getPoint(numPoints - 1, sampleOffset, value) == kResultOk)
					{
						switch (paramQueue->getParameterId())
						{
						case kMasterVolume:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								masterVolume_ = static_cast<float>(value);
							}
						}
						break;

						case kMasterPan:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								masterPan_ = static_cast<float>(value);
								// Pan is applied during audio output processing
							}
						}
						break;

						case kWaveform:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								waveform_ = static_cast<float>(value);
								int waveformIndex = convertWaveformValue(waveform_);
								std::cout << "[PROC] kWaveform: norm=" << value << " index=" << waveformIndex << "\n"
										  << std::flush;
								if (polyphonicOscillator_)
								{
									polyphonicOscillator_->setWaveType(convertWaveformIndex(waveformIndex));
									std::cout << "[PROC] setWaveType done idx=" << waveformIndex << "\n"
											  << std::flush;
								}
							}
						}
						break;

						case kOscillatorVolume:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								oscillatorVolume_ = static_cast<float>(value);
								// Oscillator volume is applied during audio generation in polyphonic oscillator
							}
						}
						break;

						case kAttackTime:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								attackTime_ = static_cast<float>(value);
								if (polyphonicOscillator_)
								{
									float attack = convertAttackTime(attackTime_);
									polyphonicOscillator_->setAttackTime(attack);
								}
							}
						}
						break;

						case kDecayTime:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								decayTime_ = static_cast<float>(value);
								if (polyphonicOscillator_)
								{
									float decay = convertDecayTime(decayTime_);
									polyphonicOscillator_->setDecayTime(decay);
								}
							}
						}
						break;

						case kSustainLevel:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								sustainLevel_ = static_cast<float>(value);
								if (polyphonicOscillator_)
								{
									polyphonicOscillator_->setSustainLevel(sustainLevel_);
								}
							}
						}
						break;

						case kReleaseTime:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								releaseTime_ = static_cast<float>(value);
								if (polyphonicOscillator_)
								{
									float release = convertReleaseTime(releaseTime_);
									polyphonicOscillator_->setReleaseTime(release);
								}
							}
						}
						break;

						case kDryWetMix:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								dryWetMix_ = static_cast<float>(value);
								if (polyphonicOscillator_)
								{
									polyphonicOscillator_->setDryWetMix(dryWetMix_);
								}
							}
						}
						break;

						case kFMModulatorRatio:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								fmModulatorRatio_ = static_cast<float>(value);
								if (polyphonicOscillator_)
								{
									float ratio = convertFMRatio(fmModulatorRatio_);
									polyphonicOscillator_->setFMModulatorRatio(ratio);
								}
							}
						}
						break;

						case kFMModulationIndex:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								fmModulationIndex_ = static_cast<float>(value);
								if (polyphonicOscillator_)
								{
									float index = convertFMIndex(fmModulationIndex_);
									polyphonicOscillator_->setFMModulationIndex(index);
								}
							}
						}
						break;

						case kFMAlgorithm:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								fmAlgorithm_ = static_cast<float>(value);
								if (polyphonicOscillator_)
								{
									int algorithmIndex = convertFMAlgorithm(fmAlgorithm_);
									synth::FMAlgorithm algorithm = static_cast<synth::FMAlgorithm>(algorithmIndex);
									polyphonicOscillator_->setFMAlgorithm(algorithm);
								}
							}
						}
						break;

						case kFMOperator2Ratio:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								fmOperator2Ratio_ = static_cast<float>(value);
								if (polyphonicOscillator_)
								{
									float ratio = convertFMRatio(fmOperator2Ratio_);
									polyphonicOscillator_->setFMOperatorRatio(2, ratio);
								}
							}
						}
						break;

						case kFMOperator2Level:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								fmOperator2Level_ = static_cast<float>(value);
								if (polyphonicOscillator_)
								{
									float level = convertFMLevel(fmOperator2Level_);
									polyphonicOscillator_->setFMOperatorLevel(2, level);
								}
							}
						}
						break;

						case kFMOperator3Ratio:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								fmOperator3Ratio_ = static_cast<float>(value);
								if (polyphonicOscillator_)
								{
									float ratio = convertFMRatio(fmOperator3Ratio_);
									polyphonicOscillator_->setFMOperatorRatio(3, ratio);
								}
							}
						}
						break;

						case kFMOperator3Level:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								fmOperator3Level_ = static_cast<float>(value);
								if (polyphonicOscillator_)
								{
									float level = convertFMLevel(fmOperator3Level_);
									polyphonicOscillator_->setFMOperatorLevel(3, level);
								}
							}
						}
						break;

						case kFMOperator4Ratio:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								fmOperator4Ratio_ = static_cast<float>(value);
								if (polyphonicOscillator_)
								{
									float ratio = convertFMRatio(fmOperator4Ratio_);
									polyphonicOscillator_->setFMOperatorRatio(4, ratio);
								}
							}
						}
						break;

						case kFMOperator4Level:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								fmOperator4Level_ = static_cast<float>(value);
								if (polyphonicOscillator_)
								{
									float level = convertFMLevel(fmOperator4Level_);
									polyphonicOscillator_->setFMOperatorLevel(4, level);
								}
							}
						}
						break;

						case kFMFeedback:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								fmFeedback_ = static_cast<float>(value);
								if (polyphonicOscillator_)
								{
									float feedback = convertFMFeedback(fmFeedback_);
									polyphonicOscillator_->setFMFeedback(feedback);
								}
							}
						}
						break;

						case kWaveTablePosition:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								waveTablePosition_ = static_cast<float>(value);
								// TODO: polyphonicOscillator_->setWaveTablePosition(waveTablePosition_);
							}
						}
						break;

						case kWaveTableMorph:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								waveTableMorph_ = static_cast<float>(value);
								// TODO: polyphonicOscillator_->setWaveTableMorph(waveTableMorph_);
							}
						}
						break;

						case kPCMMix:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								pcmMix_ = static_cast<float>(value);
								// TODO: polyphonicOscillator_->setPCMMix(pcmMix_);
							}
						}
						break;

						case kFMMix:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								fmMix_ = static_cast<float>(value);
								// TODO: polyphonicOscillator_->setFMMix(fmMix_);
							}
						}
						break;

						// LFO and Modulation Parameters
						case kLFORate:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								lfoRate_ = static_cast<float>(value);
								// TODO: float rate = convertLFORate(lfoRate_);
								// TODO: polyphonicOscillator_->setLFORate(rate);
							}
						}
						break;

						case kLFODepth:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								lfoDepth_ = static_cast<float>(value);
								// TODO: polyphonicOscillator_->setLFODepth(lfoDepth_);
							}
						}
						break;

						// Filter Parameters
						case kFilterCutoff:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								filterCutoff_ = static_cast<float>(value);
								// TODO: float cutoff = convertFilterCutoff(filterCutoff_);
								// TODO: polyphonicOscillator_->setFilterCutoff(cutoff);
							}
						}
						break;

						case kFilterResonance:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								filterResonance_ = static_cast<float>(value);
								// TODO: polyphonicOscillator_->setFilterResonance(filterResonance_);
							}
						}
						break;

						// Effects Parameters
						case kChorusRate:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								chorusRate_ = static_cast<float>(value);
								// TODO: float rate = convertChorusRate(chorusRate_);
								// TODO: polyphonicOscillator_->setChorusRate(rate);
							}
						}
						break;

						case kChorusDepth:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								chorusDepth_ = static_cast<float>(value);
								// TODO: polyphonicOscillator_->setChorusDepth(chorusDepth_);
							}
						}
						break;

						case kReverbRoom:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								reverbRoom_ = static_cast<float>(value);
								// TODO: polyphonicOscillator_->setReverbRoom(reverbRoom_);
							}
						}
						break;

						case kReverbMix:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								reverbMix_ = static_cast<float>(value);
								// TODO: polyphonicOscillator_->setReverbMix(reverbMix_);
							}
						}
						break;

						case kOverdrive:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								overdrive_ = static_cast<float>(value);
								// TODO: polyphonicOscillator_->setOverdrive(overdrive_);
							}
						}
						break;

						case kGlobalMix:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								globalMix_ = static_cast<float>(value);
								// Global mix is applied in audio output processing
							}
						}
						break;

						case kPCMSelector:
						{
							Vst::ParamValue value;
							int32 sampleOffset;
							if (paramQueue->getPoint(paramQueue->getPointCount() - 1, sampleOffset, value) == kResultOk)
							{
								// Convert normalized value to PCM index (12 options = 11 stepCount)
								int pcmIndex = static_cast<int>(value * 11.0 + 0.5);
								std::cout << "[PROC] kPCMSelector: norm=" << value << " index=" << pcmIndex << "\n"
										  << std::flush;

								// Map index to PCM filename
								static const char *pcmFiles[] = {
									nullptr, // 0 = None
									"pcm/cycle_sine_1024samples_32float.wav",
									"pcm/cycle_sawtooth_1024samples_32float.wav",
									"pcm/cycle_square_1024samples_32float.wav",
									"pcm/cycle_triangle_1024samples_32float.wav",
									"pcm/cycle_pulse_1024samples_32float.wav",
									"pcm/cycle_harmonic_rich_1024samples_32float.wav",
									"pcm/bank_basic_8waves_1024samples_32float.wav",
									"pcm/bank_harmonic_16waves_1024samples_32float.wav",
									"pcm/bank_morph_32waves_1024samples_32float.wav",
									"pcm/lavanda_wave_1024samples_48000Hz_32float.wav",
									"pcm/strange_wave_1024samples_48000Hz_32float.wav"};

								if (pcmIndex > 0 && pcmIndex < 12 && pcmFiles[pcmIndex])
								{
									auto waveform = std::make_shared<synth::PCMWaveform>();
									if (waveform->loadFromFile(pcmFiles[pcmIndex]))
									{
										std::cout << "[PROC] PCM loaded: " << pcmFiles[pcmIndex] << "\n"
												  << std::flush;
										if (polyphonicOscillator_)
										{
											polyphonicOscillator_->setCustomWaveform(waveform);
										}
									}
									else
									{
										std::cout << "[PROC] Failed to load PCM: " << pcmFiles[pcmIndex] << "\n"
												  << std::flush;
									}
								}
							}
						}
						break;
						}
					}
				}
			}
		}

		//--- Generate audio using the polyphonic oscillator -----------
		if (data.numSamples > 0 && polyphonicOscillator_)
		{
			// For now, generate audio to all available outputs
			for (int32 busIndex = 0; busIndex < data.numOutputs; busIndex++)
			{
				if (data.outputs[busIndex].numChannels >= 2)
				{
					// Stereo output
					float *leftChannel = data.outputs[busIndex].channelBuffers32[0];
					float *rightChannel = data.outputs[busIndex].channelBuffers32[1];

					// Generate interleaved stereo samples
					std::vector<float> interleavedBuffer(data.numSamples * 2);
					polyphonicOscillator_->generateStereoBuffer(interleavedBuffer.data(), data.numSamples);

					// Calculate pan coefficients (0.0 = left, 0.5 = center, 1.0 = right)
					float panRadians = masterPan_ * 1.5707963f; // 0 to π/2
					float leftGain = cosf(panRadians);
					float rightGain = sinf(panRadians);

					// De-interleave to separate L/R channels and apply master volume, oscillator volume, global mix, and pan
					for (int32 sample = 0; sample < data.numSamples; sample++)
					{
						float leftSample = interleavedBuffer[sample * 2];
						float rightSample = interleavedBuffer[sample * 2 + 1];

						// Mix to mono first, then apply pan
						float monoSample = (leftSample + rightSample) * 0.5f;

						leftChannel[sample] = monoSample * leftGain * masterVolume_ * oscillatorVolume_ * globalMix_;
						rightChannel[sample] = monoSample * rightGain * masterVolume_ * oscillatorVolume_ * globalMix_;
					}

					// Clear silence flags (audio is being generated)
					data.outputs[busIndex].silenceFlags = 0;
				}
				else if (data.outputs[busIndex].numChannels == 1)
				{
					// Mono output
					float *monoChannel = data.outputs[busIndex].channelBuffers32[0];
					polyphonicOscillator_->generateBuffer(monoChannel, data.numSamples);

					// Apply master volume, oscillator volume, and global mix
					for (int32 sample = 0; sample < data.numSamples; sample++)
					{
						monoChannel[sample] *= masterVolume_ * oscillatorVolume_ * globalMix_;
					}

					// Clear silence flags
					data.outputs[busIndex].silenceFlags = 0;
				}
				else
				{
					// Clear any additional channels and mark as silent
					for (int32 c = 0; c < data.outputs[busIndex].numChannels; c++)
					{
						memset(data.outputs[busIndex].channelBuffers32[c], 0,
							   data.numSamples * sizeof(Vst::Sample32));
					}
					data.outputs[busIndex].silenceFlags = ((uint64)1 << data.outputs[busIndex].numChannels) - 1;
				}
			}
		}
		else
		{
			// No audio generated - clear all outputs and mark as silent
			for (int32 busIndex = 0; busIndex < data.numOutputs; busIndex++)
			{
				for (int32 c = 0; c < data.outputs[busIndex].numChannels; c++)
				{
					memset(data.outputs[busIndex].channelBuffers32[c], 0,
						   data.numSamples * sizeof(Vst::Sample32));
				}
				data.outputs[busIndex].silenceFlags = ((uint64)1 << data.outputs[busIndex].numChannels) - 1;
			}
		}

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthProcessor::setupProcessing(Vst::ProcessSetup &newSetup)
	{
		//--- called before any processing ----
		sampleRate_ = static_cast<float>(newSetup.sampleRate);

		// Reinitialize the oscillator with the new sample rate if it exists
		if (polyphonicOscillator_)
		{
			polyphonicOscillator_ = std::make_unique<synth::PolyphonicOscillator>(synth::WaveType::SINE, sampleRate_);
			// TODO: Apply current parameter values
		}

		return AudioEffect::setupProcessing(newSetup);
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthProcessor::canProcessSampleSize(int32 symbolicSampleSize)
	{
		// by default kSample32 is supported
		if (symbolicSampleSize == Vst::kSample32)
			return kResultTrue;

		// disable the following comment if your processing support kSample64
		/* if (symbolicSampleSize == Vst::kSample64)
			return kResultTrue; */

		return kResultFalse;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthProcessor::setState(IBStream *state)
	{
		// called when we load a preset, the model has to be reloaded
		if (!state)
			return kResultFalse;

		IBStreamer streamer(state, kLittleEndian);

		// Load all parameters
		if (streamer.readFloat(masterVolume_) == false ||
			streamer.readFloat(masterPan_) == false ||
			streamer.readFloat(waveform_) == false ||
			streamer.readFloat(oscillatorVolume_) == false ||
			streamer.readFloat(attackTime_) == false ||
			streamer.readFloat(decayTime_) == false ||
			streamer.readFloat(sustainLevel_) == false ||
			streamer.readFloat(releaseTime_) == false ||
			streamer.readFloat(dryWetMix_) == false ||
			streamer.readFloat(fmModulatorRatio_) == false ||
			streamer.readFloat(fmModulationIndex_) == false ||
			streamer.readFloat(fmAlgorithm_) == false ||
			streamer.readFloat(fmOperator2Ratio_) == false ||
			streamer.readFloat(fmOperator2Level_) == false ||
			streamer.readFloat(fmOperator3Ratio_) == false ||
			streamer.readFloat(fmOperator3Level_) == false ||
			streamer.readFloat(fmOperator4Ratio_) == false ||
			streamer.readFloat(fmOperator4Level_) == false ||
			streamer.readFloat(fmFeedback_) == false ||
			streamer.readFloat(waveTablePosition_) == false ||
			streamer.readFloat(waveTableMorph_) == false ||
			streamer.readFloat(pcmMix_) == false ||
			streamer.readFloat(fmMix_) == false ||
			streamer.readFloat(lfoRate_) == false ||
			streamer.readFloat(lfoDepth_) == false ||
			streamer.readFloat(filterCutoff_) == false ||
			streamer.readFloat(filterResonance_) == false ||
			streamer.readFloat(chorusRate_) == false ||
			streamer.readFloat(chorusDepth_) == false ||
			streamer.readFloat(reverbRoom_) == false ||
			streamer.readFloat(reverbMix_) == false ||
			streamer.readFloat(overdrive_) == false ||
			streamer.readFloat(globalMix_) == false)
		{
			return kResultFalse;
		}

		// Apply parameters to oscillator if available
		if (polyphonicOscillator_)
		{
			// Set waveform
			int waveformIndex = convertWaveformValue(waveform_);
			polyphonicOscillator_->setWaveType(convertWaveformIndex(waveformIndex));

			// Set ADSR values
			float attack = convertAttackTime(attackTime_);
			float decay = convertDecayTime(decayTime_);
			float release = convertReleaseTime(releaseTime_);
			polyphonicOscillator_->setADSR(attack, decay, sustainLevel_, release);

			// Set dry/wet mix
			polyphonicOscillator_->setDryWetMix(dryWetMix_);

			// Set FM parameters
			float fmModRatio = convertFMRatio(fmModulatorRatio_);
			polyphonicOscillator_->setFMModulatorRatio(fmModRatio);

			float fmModIndex = convertFMIndex(fmModulationIndex_);
			polyphonicOscillator_->setFMModulationIndex(fmModIndex);

			int algorithmIndex = convertFMAlgorithm(fmAlgorithm_);
			synth::FMAlgorithm algorithm = static_cast<synth::FMAlgorithm>(algorithmIndex);
			polyphonicOscillator_->setFMAlgorithm(algorithm);

			float op2Ratio = convertFMRatio(fmOperator2Ratio_);
			polyphonicOscillator_->setFMOperatorRatio(2, op2Ratio);

			float op2Level = convertFMLevel(fmOperator2Level_);
			polyphonicOscillator_->setFMOperatorLevel(2, op2Level);

			float op3Ratio = convertFMRatio(fmOperator3Ratio_);
			polyphonicOscillator_->setFMOperatorRatio(3, op3Ratio);

			float op3Level = convertFMLevel(fmOperator3Level_);
			polyphonicOscillator_->setFMOperatorLevel(3, op3Level);

			float op4Ratio = convertFMRatio(fmOperator4Ratio_);
			polyphonicOscillator_->setFMOperatorRatio(4, op4Ratio);

			float op4Level = convertFMLevel(fmOperator4Level_);
			polyphonicOscillator_->setFMOperatorLevel(4, op4Level);

			float feedback = convertFMFeedback(fmFeedback_);
			polyphonicOscillator_->setFMFeedback(feedback);

			// Set wave table parameters
			// TODO: polyphonicOscillator_->setWaveTablePosition(waveTablePosition_);
			// TODO: polyphonicOscillator_->setWaveTableMorph(waveTableMorph_);
			// TODO: polyphonicOscillator_->setPCMMix(pcmMix_);
			// TODO: polyphonicOscillator_->setFMMix(fmMix_);

			// Set LFO parameters
			// TODO: float lfoRateHz = convertLFORate(lfoRate_);
			// TODO: polyphonicOscillator_->setLFORate(lfoRateHz);
			// TODO: polyphonicOscillator_->setLFODepth(lfoDepth_);

			// Set filter parameters
			// TODO: float cutoffHz = convertFilterCutoff(filterCutoff_);
			// TODO: polyphonicOscillator_->setFilterCutoff(cutoffHz);
			// TODO: polyphonicOscillator_->setFilterResonance(filterResonance_);

			// Set effects parameters
			// TODO: float chorusRateHz = convertChorusRate(chorusRate_);
			// TODO: polyphonicOscillator_->setChorusRate(chorusRateHz);
			// TODO: polyphonicOscillator_->setChorusDepth(chorusDepth_);
			// TODO: polyphonicOscillator_->setReverbRoom(reverbRoom_);
			// TODO: polyphonicOscillator_->setReverbMix(reverbMix_);
			// TODO: polyphonicOscillator_->setOverdrive(overdrive_);
		}

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthProcessor::getState(IBStream *state)
	{
		// here we need to save the model
		if (!state)
			return kResultFalse;

		IBStreamer streamer(state, kLittleEndian);

		// Save all parameters
		if (streamer.writeFloat(masterVolume_) == false ||
			streamer.writeFloat(masterPan_) == false ||
			streamer.writeFloat(waveform_) == false ||
			streamer.writeFloat(oscillatorVolume_) == false ||
			streamer.writeFloat(attackTime_) == false ||
			streamer.writeFloat(decayTime_) == false ||
			streamer.writeFloat(sustainLevel_) == false ||
			streamer.writeFloat(releaseTime_) == false ||
			streamer.writeFloat(dryWetMix_) == false ||
			streamer.writeFloat(fmModulatorRatio_) == false ||
			streamer.writeFloat(fmModulationIndex_) == false ||
			streamer.writeFloat(fmAlgorithm_) == false ||
			streamer.writeFloat(fmOperator2Ratio_) == false ||
			streamer.writeFloat(fmOperator2Level_) == false ||
			streamer.writeFloat(fmOperator3Ratio_) == false ||
			streamer.writeFloat(fmOperator3Level_) == false ||
			streamer.writeFloat(fmOperator4Ratio_) == false ||
			streamer.writeFloat(fmOperator4Level_) == false ||
			streamer.writeFloat(fmFeedback_) == false ||
			streamer.writeFloat(waveTablePosition_) == false ||
			streamer.writeFloat(waveTableMorph_) == false ||
			streamer.writeFloat(pcmMix_) == false ||
			streamer.writeFloat(fmMix_) == false ||
			streamer.writeFloat(lfoRate_) == false ||
			streamer.writeFloat(lfoDepth_) == false ||
			streamer.writeFloat(filterCutoff_) == false ||
			streamer.writeFloat(filterResonance_) == false ||
			streamer.writeFloat(chorusRate_) == false ||
			streamer.writeFloat(chorusDepth_) == false ||
			streamer.writeFloat(reverbRoom_) == false ||
			streamer.writeFloat(reverbMix_) == false ||
			streamer.writeFloat(overdrive_) == false ||
			streamer.writeFloat(globalMix_) == false)
		{
			return kResultFalse;
		}

		return kResultOk;
	}

	//------------------------------------------------------------------------
	synth::WaveType ObsSynthProcessor::convertWaveformIndex(int index)
	{
		switch (index)
		{
		case 0:
			return synth::WaveType::SINE;
		case 1:
			return synth::WaveType::SAW; // Sawtooth
		case 2:
			return synth::WaveType::SQUARE;
		case 3:
			return synth::WaveType::TRIANGLE;
		case 4:
			return synth::WaveType::PCM_CUSTOM; // PCM sample
		case 5:
			return synth::WaveType::FM_SYNTHESIS; // FM synthesis
		default:
			return synth::WaveType::SINE;
		}
	}

	//------------------------------------------------------------------------
	float ObsSynthProcessor::convertAttackTime(float normalized) const
	{
		// Map [0.0, 1.0] to [0.001, 5.0] seconds with exponential curve
		return 0.001f + normalized * normalized * 4.999f;
	}

	//------------------------------------------------------------------------
	float ObsSynthProcessor::convertDecayTime(float normalized) const
	{
		// Map [0.0, 1.0] to [0.001, 5.0] seconds with exponential curve
		return 0.001f + normalized * normalized * 4.999f;
	}

	//------------------------------------------------------------------------
	float ObsSynthProcessor::convertReleaseTime(float normalized) const
	{
		// Map [0.0, 1.0] to [0.001, 10.0] seconds with exponential curve
		return 0.001f + normalized * normalized * 9.999f;
	}

	//------------------------------------------------------------------------
	int ObsSynthProcessor::convertWaveformValue(float normalized) const
	{
		// Map [0.0, 1.0] to [0, 5] discrete values (6 waveforms including FM)
		return static_cast<int>(normalized * 5.0f + 0.5f); // Round to nearest integer
	}

	//------------------------------------------------------------------------
	float ObsSynthProcessor::convertFMRatio(float normalized) const
	{
		// Map [0.0, 1.0] to [0.1, 10.0] exponential curve for FM ratio
		return 0.1f + normalized * normalized * 9.9f;
	}

	//------------------------------------------------------------------------
	float ObsSynthProcessor::convertFMIndex(float normalized) const
	{
		// Map [0.0, 1.0] to [0.0, 20.0] linear curve for modulation index
		return normalized * 20.0f;
	}

	//------------------------------------------------------------------------
	int ObsSynthProcessor::convertFMAlgorithm(float normalized) const
	{
		// Map [0.0, 1.0] to [0, 4] discrete values (5 FM algorithms)
		return static_cast<int>(normalized * 4.0f + 0.5f); // Round to nearest integer
	}

	//------------------------------------------------------------------------
	float ObsSynthProcessor::convertFMLevel(float normalized) const
	{
		// Direct mapping [0.0, 1.0] to [0.0, 1.0] for operator levels
		return normalized;
	}

	//------------------------------------------------------------------------
	float ObsSynthProcessor::convertFMFeedback(float normalized) const
	{
		// Direct mapping [0.0, 1.0] to [0.0, 1.0] for feedback amount
		return normalized;
	}

	//------------------------------------------------------------------------
	float ObsSynthProcessor::convertLFORate(float normalized) const
	{
		// Map [0.0, 1.0] to [0.1, 20.0] Hz with exponential curve
		return 0.1f + normalized * normalized * 19.9f;
	}

	//------------------------------------------------------------------------
	float ObsSynthProcessor::convertFilterCutoff(float normalized) const
	{
		// Map [0.0, 1.0] to [20, 20000] Hz with exponential curve
		return 20.0f + normalized * normalized * normalized * 19980.0f;
	}

	//------------------------------------------------------------------------
	float ObsSynthProcessor::convertChorusRate(float normalized) const
	{
		// Map [0.0, 1.0] to [0.1, 10.0] Hz with linear curve
		return 0.1f + normalized * 9.9f;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthProcessor::notify(Vst::IMessage *message)
	{
		if (!message)
			return kResultFalse;

		const char *msgId = message->getMessageID();
		std::cout << "[PROC] notify: " << msgId << "\n"
				  << std::flush;

		if (strcmp(msgId, "LoadPCM") == 0)
		{
			Steinberg::Vst::TChar path[1024];
			if (message->getAttributes()->getString("path", path, sizeof(path)) == kResultOk)
			{
				// Convert TChar to string
				std::string pathStr;
				for (int i = 0; path[i] != 0; i++)
				{
					pathStr += static_cast<char>(path[i]);
				}

				std::cout << "[PROC] Loading PCM: " << pathStr << "\n"
						  << std::flush;

				// Load the PCM file
				auto waveform = std::make_shared<synth::PCMWaveform>();
				if (waveform->loadFromFile(pathStr))
				{
					std::cout << "[PROC] PCM loaded successfully, samples=" << waveform->getSampleCount() << "\n"
							  << std::flush;

					// Set the waveform on all oscillators
					if (polyphonicOscillator_)
					{
						polyphonicOscillator_->setCustomWaveform(waveform);
						std::cout << "[PROC] Waveform set on oscillators\n"
								  << std::flush;
					}
				}
				else
				{
					std::cout << "[PROC] Failed to load PCM file\n"
							  << std::flush;
				}

				return kResultOk;
			}
		}

		return AudioEffect::notify(message);
	}

	//------------------------------------------------------------------------
} // namespace ModularSistemas
