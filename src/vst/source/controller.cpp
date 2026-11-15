#include "controller.h"
#include "cids.h"
#include "parameters.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/controls/ioptionmenulistener.h"
#include "vstgui/lib/controls/cslider.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/csegmentbutton.h"
#include "vstgui/uidescription/uiattributes.h"
#include "vstgui/lib/cstring.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <csignal>
#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

using namespace Steinberg;

// File-based logger for VST debugging
static std::ofstream g_vstLog;
static bool g_logInitialized = false;

// Forward declaration
static void vstLog(const std::string &msg);

#ifdef _WIN32
// Windows crash handler
static LONG WINAPI VstCrashHandler(EXCEPTION_POINTERS *ExceptionInfo)
{
	vstLog("!!! CRASH DETECTED !!!");
	vstLog("Exception Code: " + std::to_string(ExceptionInfo->ExceptionRecord->ExceptionCode));
	vstLog("Exception Address: " + std::to_string((uintptr_t)ExceptionInfo->ExceptionRecord->ExceptionAddress));

	// Log stack trace if possible
	void *stack[64];
	USHORT frames = CaptureStackBackTrace(0, 64, stack, NULL);
	vstLog("Stack frames: " + std::to_string(frames));
	for (USHORT i = 0; i < frames; i++)
	{
		vstLog("  Frame " + std::to_string(i) + ": " + std::to_string((uintptr_t)stack[i]));
	}

	if (g_vstLog.is_open())
	{
		g_vstLog.flush();
		g_vstLog.close();
	}

	return EXCEPTION_CONTINUE_SEARCH;
}

static bool g_crashHandlerInstalled = false;
static void InstallCrashHandler()
{
	if (!g_crashHandlerInstalled)
	{
		SetUnhandledExceptionFilter(VstCrashHandler);
		g_crashHandlerInstalled = true;
		vstLog("[CrashHandler] Installed");
	}
}
#endif
static void vstLog(const std::string &msg)
{
	if (!g_logInitialized)
	{
		g_logInitialized = true;
		// Try multiple locations
		const char *paths[] = {
			"C:\\Users\\orian\\dev\\synth\\vst_debug.log",
			"C:\\temp\\vst_debug.log",
			"vst_debug.log"};
		for (const char *path : paths)
		{
			g_vstLog.open(path, std::ios::app);
			if (g_vstLog.is_open())
			{
				g_vstLog << "=== LOG OPENED AT " << path << " ===" << std::endl;
				g_vstLog.flush();
				break;
			}
		}
	}
	if (g_vstLog.is_open())
	{
		auto now = std::chrono::system_clock::now();
		auto time = std::chrono::system_clock::to_time_t(now);
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
		g_vstLog << std::put_time(std::localtime(&time), "%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count()
				 << " " << msg << std::endl;
		g_vstLog.flush();
	}
}

// UI-only control tags (not VST parameters)
enum UIControlTags : int32_t
{
	kPCMSelector = 108 // PCM file selector dropdown
};

// -----------------------------------------------------------------------
// Direct index listener for discrete COptionMenu params (FM algo)
// Bypasses VSTGUI's float value chain which loses the index in normalization
// -----------------------------------------------------------------------
class DiscreteMenuListener : public VSTGUI::OptionMenuListenerAdapter
{
public:
	DiscreteMenuListener(ModularSistemas::ObsSynthController *ctrl,
						 Steinberg::Vst::ParamID id, int32_t stepCount)
		: ctrl_(ctrl), id_(id), stepCount_(stepCount)
	{
	}

	bool onOptionMenuSetPopupResult(VSTGUI::COptionMenu *menu,
									VSTGUI::COptionMenu * /*selectedMenu*/,
									int32_t selectedIndex) override
	{
		if (selectedIndex < 0 || selectedIndex > stepCount_)
			return false;

		// index directly → normalized: 0/N, 1/N, 2/N …
		Steinberg::Vst::ParamValue normalized =
			static_cast<double>(selectedIndex) / static_cast<double>(stepCount_);

		std::cout << "[MENU] id=" << id_
				  << " index=" << selectedIndex
				  << " norm=" << normalized << "\n"
				  << std::flush;

		// Update visual state
		menu->setCurrent(selectedIndex);
		menu->setDirty();

		// Push directly to VST3 parameter system
		ctrl_->beginEdit(id_);
		ctrl_->setParamNormalized(id_, normalized);
		ctrl_->performEdit(id_, normalized);
		ctrl_->endEdit(id_);

		return true; // prevent the broken VSTGUI default flow
	}

private:
	ModularSistemas::ObsSynthController *ctrl_;
	Steinberg::Vst::ParamID id_;
	int32_t stepCount_;
};

// Macro to simplify parameter string assignments
#define SET_PARAM_STRING(field, text) \
	wcscpy_s(reinterpret_cast<wchar_t *>(paramInfo.field), 128, reinterpret_cast<const wchar_t *>(STR16(text)))

namespace ModularSistemas
{

	//------------------------------------------------------------------------
	// ObsSynthController Implementation
	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthController::initialize(FUnknown *context)
	{
		// Initialize file logging immediately
		vstLog("============ VST CONTROLLER INITIALIZE ============");

#ifdef _WIN32
		InstallCrashHandler();
#endif

		// Here the Plug-in will be instantiated

		//---do not forget to call parent ------
		tresult result = EditControllerEx1::initialize(context);
		if (result != kResultOk)
		{
			vstLog("[initialize] EditControllerEx1::initialize FAILED");
			return result;
		}
		vstLog("[initialize] EditControllerEx1::initialize OK");

		// Register parameters
		Vst::ParameterInfo paramInfo = {};

		// Master Volume
		paramInfo.id = kMasterVolume;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.7;
		SET_PARAM_STRING(title, "Master Volume");
		SET_PARAM_STRING(shortTitle, "Volume");
		SET_PARAM_STRING(units, "%");
		parameters.addParameter(paramInfo);

		// Master Pan
		paramInfo.id = kMasterPan;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.5; // Center
		SET_PARAM_STRING(title, "Master Pan");
		SET_PARAM_STRING(shortTitle, "Pan");
		SET_PARAM_STRING(units, "");
		parameters.addParameter(paramInfo);

		// Waveform Selection - StringListParameter for dropdown population
		auto *waveformParam = new Vst::StringListParameter(USTRING("Waveform"), kWaveform);
		waveformParam->appendString(USTRING("Sine"));
		waveformParam->appendString(USTRING("Sawtooth"));
		waveformParam->appendString(USTRING("Square"));
		waveformParam->appendString(USTRING("Triangle"));
		waveformParam->appendString(USTRING("PCM"));
		waveformParam->appendString(USTRING("FM Synth"));
		parameters.addParameter(waveformParam);

		// PCM Selector - StringListParameter for dropdown population
		auto *pcmSelectorParam = new Vst::StringListParameter(USTRING("PCM Sample"), kPCMSelector);
		pcmSelectorParam->appendString(USTRING("None"));
		pcmSelectorParam->appendString(USTRING("cycle_sine"));
		pcmSelectorParam->appendString(USTRING("cycle_sawtooth"));
		pcmSelectorParam->appendString(USTRING("cycle_square"));
		pcmSelectorParam->appendString(USTRING("cycle_triangle"));
		pcmSelectorParam->appendString(USTRING("cycle_pulse"));
		pcmSelectorParam->appendString(USTRING("cycle_harmonic_rich"));
		pcmSelectorParam->appendString(USTRING("bank_basic_8waves"));
		pcmSelectorParam->appendString(USTRING("bank_harmonic_16waves"));
		pcmSelectorParam->appendString(USTRING("bank_morph_32waves"));
		pcmSelectorParam->appendString(USTRING("lavanda_wave"));
		pcmSelectorParam->appendString(USTRING("strange_wave"));
		parameters.addParameter(pcmSelectorParam);

		// Oscillator Volume
		paramInfo.id = kOscillatorVolume;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.8;
		SET_PARAM_STRING(title, "Oscillator Volume");
		SET_PARAM_STRING(shortTitle, "Osc Vol");
		SET_PARAM_STRING(units, "%");
		parameters.addParameter(paramInfo);

		// Attack Time
		paramInfo.id = kAttackTime;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.03; // Maps to 0.15 seconds (0.03 * 5.0)
		SET_PARAM_STRING(title, "Attack Time");
		SET_PARAM_STRING(shortTitle, "Attack");
		SET_PARAM_STRING(units, "sec");
		parameters.addParameter(paramInfo);

		// Decay Time
		paramInfo.id = kDecayTime;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.07; // Maps to 0.35 seconds (0.07 * 5.0)
		SET_PARAM_STRING(title, "Decay Time");
		SET_PARAM_STRING(shortTitle, "Decay");
		SET_PARAM_STRING(units, "sec");
		parameters.addParameter(paramInfo);

		// Sustain Level
		paramInfo.id = kSustainLevel;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.5; // 50% sustain level
		SET_PARAM_STRING(title, "Sustain Level");
		SET_PARAM_STRING(shortTitle, "Sustain");
		SET_PARAM_STRING(units, "%");
		parameters.addParameter(paramInfo);

		// Release Time
		paramInfo.id = kReleaseTime;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.07; // Maps to 0.7 seconds (0.07 * 10.0)
		SET_PARAM_STRING(title, "Release Time");
		SET_PARAM_STRING(shortTitle, "Release");
		SET_PARAM_STRING(units, "sec");
		parameters.addParameter(paramInfo);

		// Dry/Wet Mix
		paramInfo.id = kDryWetMix;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 1.0; // 100% wet (envelope processed)
		SET_PARAM_STRING(title, "Dry/Wet Mix");
		SET_PARAM_STRING(shortTitle, "Mix");
		SET_PARAM_STRING(units, "%");
		parameters.addParameter(paramInfo);

		// FM Modulator Ratio
		paramInfo.id = kFMModulatorRatio;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.1; // Maps to ratio 1.0 (0.1 * 9.9 + 0.1)
		SET_PARAM_STRING(title, "FM Mod Ratio");
		SET_PARAM_STRING(shortTitle, "FM Ratio");
		SET_PARAM_STRING(units, "");
		parameters.addParameter(paramInfo);

		// FM Modulation Index
		paramInfo.id = kFMModulationIndex;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.1; // Maps to index 2.0 (0.1 * 20.0)
		SET_PARAM_STRING(title, "FM Mod Index");
		SET_PARAM_STRING(shortTitle, "FM Index");
		SET_PARAM_STRING(units, "");
		parameters.addParameter(paramInfo);

		// FM Algorithm - StringListParameter for dropdown population
		auto *fmAlgorithmParam = new Vst::StringListParameter(USTRING("FM Algorithm"), kFMAlgorithm);
		fmAlgorithmParam->appendString(USTRING("2-Op Simple"));
		fmAlgorithmParam->appendString(USTRING("2-Op Parallel"));
		fmAlgorithmParam->appendString(USTRING("3-Op Serial"));
		fmAlgorithmParam->appendString(USTRING("3-Op Parallel"));
		fmAlgorithmParam->appendString(USTRING("4-Op DX Style"));
		parameters.addParameter(fmAlgorithmParam);

		// FM Operator 2 Ratio
		paramInfo.id = kFMOperator2Ratio;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.1; // Maps to ratio 1.0
		SET_PARAM_STRING(title, "FM Op2 Ratio");
		SET_PARAM_STRING(shortTitle, "Op2 Ratio");
		SET_PARAM_STRING(units, "");
		parameters.addParameter(paramInfo);

		// FM Operator 2 Level
		paramInfo.id = kFMOperator2Level;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.5; // 50% level
		SET_PARAM_STRING(title, "FM Op2 Level");
		SET_PARAM_STRING(shortTitle, "Op2 Level");
		SET_PARAM_STRING(units, "%");
		parameters.addParameter(paramInfo);

		// FM Operator 3 Ratio
		paramInfo.id = kFMOperator3Ratio;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.1; // Maps to ratio 1.0
		SET_PARAM_STRING(title, "FM Op3 Ratio");
		SET_PARAM_STRING(shortTitle, "Op3 Ratio");
		SET_PARAM_STRING(units, "");
		parameters.addParameter(paramInfo);

		// FM Operator 3 Level
		paramInfo.id = kFMOperator3Level;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.5; // 50% level
		SET_PARAM_STRING(title, "FM Op3 Level");
		SET_PARAM_STRING(shortTitle, "Op3 Level");
		SET_PARAM_STRING(units, "%");
		parameters.addParameter(paramInfo);

		// FM Operator 4 Ratio
		paramInfo.id = kFMOperator4Ratio;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.1; // Maps to ratio 1.0
		SET_PARAM_STRING(title, "FM Op4 Ratio");
		SET_PARAM_STRING(shortTitle, "Op4 Ratio");
		SET_PARAM_STRING(units, "");
		parameters.addParameter(paramInfo);

		// FM Operator 4 Level
		paramInfo.id = kFMOperator4Level;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.5; // 50% level
		SET_PARAM_STRING(title, "FM Op4 Level");
		SET_PARAM_STRING(shortTitle, "Op4 Level");
		SET_PARAM_STRING(units, "%");
		parameters.addParameter(paramInfo);

		// FM Feedback
		paramInfo.id = kFMFeedback;
		paramInfo.flags = Vst::ParameterInfo::kCanAutomate;
		paramInfo.stepCount = 0;
		paramInfo.defaultNormalizedValue = 0.0; // No feedback by default
		SET_PARAM_STRING(title, "FM Feedback");
		SET_PARAM_STRING(shortTitle, "Feedback");
		SET_PARAM_STRING(units, "%");
		parameters.addParameter(paramInfo);

		return result;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthController::terminate()
	{
		delete waveformMenuListener_;
		waveformMenuListener_ = nullptr;
		delete fmAlgorithmMenuListener_;
		fmAlgorithmMenuListener_ = nullptr;

		return EditControllerEx1::terminate();
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthController::setComponentState(IBStream *state)
	{
		// Here you get the state of the component (Processor part)
		if (!state)
			return kResultFalse;

		IBStreamer streamer(state, kLittleEndian);

		// Read all parameters from processor state
		float masterVolume, masterPan, waveform, oscillatorVolume;
		float attackTime, decayTime, sustainLevel, releaseTime, dryWetMix;

		if (streamer.readFloat(masterVolume) == false ||
			streamer.readFloat(masterPan) == false ||
			streamer.readFloat(waveform) == false ||
			streamer.readFloat(oscillatorVolume) == false ||
			streamer.readFloat(attackTime) == false ||
			streamer.readFloat(decayTime) == false ||
			streamer.readFloat(sustainLevel) == false ||
			streamer.readFloat(releaseTime) == false ||
			streamer.readFloat(dryWetMix) == false)
		{
			return kResultFalse;
		}

		// Update all parameters in the controller
		setParamNormalized(kMasterVolume, masterVolume);
		setParamNormalized(kMasterPan, masterPan);
		setParamNormalized(kWaveform, waveform);
		setParamNormalized(kOscillatorVolume, oscillatorVolume);
		setParamNormalized(kAttackTime, attackTime);
		setParamNormalized(kDecayTime, decayTime);
		setParamNormalized(kSustainLevel, sustainLevel);
		setParamNormalized(kReleaseTime, releaseTime);
		setParamNormalized(kDryWetMix, dryWetMix);

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthController::setState(IBStream *state)
	{
		// Here the host tells us which state to load. Here we only read the UI size - the rest is read by the processor
		if (!state)
			return kResultFalse;

		// For now, just return OK - processor handles the main parameter state
		return kResultTrue;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthController::getState(IBStream *state)
	{
		// Here the host asks us to deliver the state (if needed)
		// Note: The real state of our parameters is saved in the processor
		if (!state)
			return kResultFalse;

		// For now, just return OK - processor handles the main parameter state
		return kResultTrue;
	}

	//------------------------------------------------------------------------
	IPlugView *PLUGIN_API ObsSynthController::createView(FIDString name)
	{
		vstLog("[createView] ENTER name=" + std::string(name ? name : "null"));
		// Here the Host wants to open your editor (if you have one)
		if (FIDStringsEqual(name, Vst::ViewType::kEditor))
		{
			vstLog("[createView] Creating VST3Editor");
			// create your editor here and return a IPlugView ptr of it
			auto *view = new VSTGUI::VST3Editor(this, "view", "editor.uidesc");
			vstLog("[createView] VST3Editor created: " + std::to_string((uintptr_t)view));
			return view;
		}
		vstLog("[createView] Not an editor view, returning nullptr");
		return nullptr;
	}

	//------------------------------------------------------------------------
	VSTGUI::CView *ObsSynthController::verifyView(VSTGUI::CView *view, const VSTGUI::UIAttributes &attributes, const VSTGUI::IUIDescription *description)
	{
		try
		{
			vstLog("[verifyView] ENTER");

			// Check control tags for special handling
			const std::string *controlTagString = attributes.getAttributeValue("control-tag");
			if (controlTagString)
			{
				int32_t controlTag = std::stoi(*controlTagString);
				vstLog("[verifyView] controlTag=" + std::to_string(controlTag));

				// Handle waveform Row 1 (SIN, SAW, SQR = indices 0,1,2)
				if (controlTag == kWaveform)
				{
					vstLog("[verifyView] kWaveform - casting to CSegmentButton");
					auto segBtn = dynamic_cast<VSTGUI::CSegmentButton *>(view);
					if (segBtn)
					{
						Vst::ParamValue currentValue = getParamNormalized(kWaveform);
						uint32_t waveIndex = static_cast<uint32_t>(currentValue * 5 + 0.5);
						vstLog("[verifyView] waveIndex=" + std::to_string(waveIndex));
						// Row 1 handles indices 0,1,2 (SIN, SAW, SQR)
						if (waveIndex <= 2)
							segBtn->setSelectedSegment(waveIndex);
						else
							segBtn->setSelectedSegment(VSTGUI::CSegmentButton::kPushBack); // deselect
						vstLog("[verifyView] Row1 done");
					}
				}
				// PCM File Selector
				else if (controlTag == kPCMSelector)
				{
					vstLog("[PCM] Setting up PCM selector");
					auto optionMenu = dynamic_cast<VSTGUI::COptionMenu *>(view);
					if (optionMenu)
					{
						vstLog("[PCM] COptionMenu cast OK");
						optionMenu->removeAllEntry();
						// List PCM files from pcm directory - try relative then absolute
						std::filesystem::path pcmDir = "pcm";
						vstLog("[PCM] Checking dir: " + pcmDir.string());

						if (!std::filesystem::exists(pcmDir))
						{
							// Try path relative to executable
							pcmDir = std::filesystem::current_path() / "pcm";
							vstLog("[PCM] Not found, trying: " + pcmDir.string());
						}

						int fileCount = 0;
						if (std::filesystem::exists(pcmDir) && std::filesystem::is_directory(pcmDir))
						{
							vstLog("[PCM] Directory exists, listing files");
							for (const auto &entry : std::filesystem::directory_iterator(pcmDir))
							{
								vstLog("[PCM] Entry: " + entry.path().string());
								if (entry.is_regular_file() && entry.path().extension() == ".wav")
								{
									std::string filename = entry.path().filename().string();
									if (filename.length() > 25)
									{
										filename = filename.substr(0, 22) + "...";
									}
									optionMenu->addEntry(VSTGUI::UTF8String(filename.c_str()));
									fileCount++;
								}
							}
						}
						else
						{
							vstLog("[PCM] Directory not found");
						}

						if (fileCount == 0)
						{
							optionMenu->addEntry(VSTGUI::UTF8String("(no PCM files)"));
						}
						optionMenu->setCurrent(0);
						vstLog("[PCM] selector done with " + std::to_string(fileCount) + " files");
					}
				}
				// FM Algorithm still uses COptionMenu
				else if (controlTag == kFMAlgorithm)
				{
					vstLog("[verifyView] kFMAlgorithm");
					auto optionMenu = dynamic_cast<VSTGUI::COptionMenu *>(view);
					if (optionMenu)
					{
						optionMenu->removeAllEntry();
						optionMenu->addEntry(VSTGUI::UTF8String("2-Op Simple"));
						optionMenu->addEntry(VSTGUI::UTF8String("2-Op Feedback"));
						optionMenu->addEntry(VSTGUI::UTF8String("4-Op Serial"));
						optionMenu->addEntry(VSTGUI::UTF8String("4-Op Parallel"));
						optionMenu->addEntry(VSTGUI::UTF8String("4-Op Mixed"));
						Vst::ParamValue currentValue = getParamNormalized(kFMAlgorithm);
						int32_t menuIndex = static_cast<int32_t>(currentValue * 4 + 0.5f);
						if (menuIndex < 0)
							menuIndex = 0;
						if (menuIndex > 4)
							menuIndex = 4;
						optionMenu->setCurrent(menuIndex);
						optionMenu->checkEntryAlone(menuIndex);
						optionMenu->setDirty();

						// Register direct-index listener
						if (!fmAlgorithmMenuListener_)
							fmAlgorithmMenuListener_ = new DiscreteMenuListener(this, kFMAlgorithm, 4);
						optionMenu->registerOptionMenuListener(fmAlgorithmMenuListener_);
					}
				}
			}

			// Style ADSR sliders (XML draw-* attributes handle rendering; set only colors here)
			if (auto *slider = dynamic_cast<VSTGUI::CSlider *>(view))
			{
				slider->setFrameColor(VSTGUI::CColor(0, 104, 128));
				slider->setValueColor(VSTGUI::CColor(0, 200, 240));
				slider->setBackColor(VSTGUI::CColor(42, 42, 42));
			}

			vstLog("[verifyView] EXIT OK");
			// Always return the view (parent call not needed for IController)
			return view;
		}
		catch (const std::exception &e)
		{
			vstLog("[verifyView] EXCEPTION: " + std::string(e.what()));
			return view;
		}
		catch (...)
		{
			vstLog("[verifyView] UNKNOWN EXCEPTION");
			return view;
		}
	}

	//------------------------------------------------------------------------
	void ObsSynthController::valueChanged(VSTGUI::CControl *control)
	{
		try
		{
			vstLog("[valueChanged] ENTER");

			if (!control)
			{
				vstLog("[valueChanged] control is NULL");
				return;
			}

			int32_t tag = control->getTag();
			vstLog("[valueChanged] tag=" + std::to_string(tag));

			// Handle waveform Row 1 (SIN, SAW, SQR = indices 0,1,2)
			if (tag == kWaveform)
			{
				vstLog("[valueChanged] kWaveform row1");
				auto segBtn = dynamic_cast<VSTGUI::CSegmentButton *>(control);
				if (segBtn)
				{
					uint32_t selectedSegment = segBtn->getSelectedSegment();
					// Row 1: segment 0,1,2 -> waveform 0,1,2
					Vst::ParamValue norm = static_cast<double>(selectedSegment) / 5.0;
					vstLog("[SEG-R1] Waveform=" + std::to_string(selectedSegment) + " norm=" + std::to_string(norm));

					beginEdit(kWaveform);
					setParamNormalized(kWaveform, norm);
					performEdit(kWaveform, norm);
					endEdit(kWaveform);
					return;
				}
			}
			// Handle PCM selector
			if (tag == kPCMSelector)
			{
				vstLog("[valueChanged] kPCMSelector");
				auto optionMenu = dynamic_cast<VSTGUI::COptionMenu *>(control);
				if (optionMenu)
				{
					// Populate menu on first access
					if (!pcmMenuPopulated_)
					{
						populatePCMMenu(optionMenu);
					}

					int32_t selectedIndex = static_cast<int32_t>(control->getValue());
					vstLog("[PCM] Selected index=" + std::to_string(selectedIndex));

					// Load the selected PCM file
					if (selectedIndex >= 0 && selectedIndex < (int32_t)pcmFilePaths_.size())
					{
						loadPCMFile(selectedIndex);
					}
				}
				return;
			}
			if (tag == kFMAlgorithm)
			{
				vstLog("[valueChanged] kFMAlgorithm");
				int32_t idx = static_cast<int32_t>(control->getValue() + 0.5f);
				if (idx < 0)
					idx = 0;
				if (idx > 4)
					idx = 4;
				Vst::ParamValue norm = static_cast<double>(idx) / 4.0;
				vstLog("[CTRL] kFMAlgorithm idx=" + std::to_string(idx) + " norm=" + std::to_string(norm));
				setParamNormalized(tag, norm);
				performEdit(tag, norm);
				return;
			}

			Vst::ParamValue normalizedValue = control->getValueNormalized();
			setParamNormalized(tag, normalizedValue);
			performEdit(tag, normalizedValue);
			vstLog("[valueChanged] EXIT OK tag=" + std::to_string(tag));
		}
		catch (const std::exception &e)
		{
			vstLog("[valueChanged] EXCEPTION: " + std::string(e.what()));
		}
		catch (...)
		{
			vstLog("[valueChanged] UNKNOWN EXCEPTION");
		}
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthController::getParamStringByValue(Vst::ParamID id,
																 Vst::ParamValue valueNormalized, Vst::String128 string)
	{
		// Handle waveform parameter display
		if (id == kWaveform)
		{
			int waveformIndex = static_cast<int>(valueNormalized * 5 + 0.5f);
			const char *waveformNames[] = {"Sine", "Sawtooth", "Square", "Triangle", "PCM", "FM Synth"};

			if (waveformIndex >= 0 && waveformIndex < 6)
			{
				// UString128(string) copies string INTO internal buffer (bug).
				// UString(string, 128) sets thisBuffer = string directly (correct).
				Steinberg::UString(string, 128).fromAscii(waveformNames[waveformIndex]);
				return kResultTrue;
			}
		}

		// Handle FM Algorithm parameter display
		if (id == kFMAlgorithm)
		{
			int algorithmIndex = static_cast<int>(valueNormalized * 4 + 0.5f);
			const char *algorithmNames[] = {"2-Op Simple", "2-Op Feedback", "4-Op Serial", "4-Op Parallel", "4-Op Mixed"};

			if (algorithmIndex >= 0 && algorithmIndex < 5)
			{
				Steinberg::UString(string, 128).fromAscii(algorithmNames[algorithmIndex]);
				return kResultTrue;
			}
		}

		// Default behavior for unhandled parameters
		return EditControllerEx1::getParamStringByValue(id, valueNormalized, string);
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API ObsSynthController::getParamValueByString(Vst::ParamID id,
																 Vst::TChar *string, Vst::ParamValue &valueNormalized)
	{
		// For now, use default behavior - can be enhanced later
		return EditControllerEx1::getParamValueByString(id, string, valueNormalized);
	}

	//------------------------------------------------------------------------
	void ObsSynthController::populatePCMMenu(VSTGUI::COptionMenu *menu)
	{
		vstLog("[PCM] Populating menu");
		pcmFilePaths_.clear();
		menu->removeAllEntry();

		// Try to find PCM directory
		std::filesystem::path pcmDir = "pcm";
		if (!std::filesystem::exists(pcmDir))
		{
			pcmDir = std::filesystem::current_path() / "pcm";
		}

		vstLog("[PCM] Looking in: " + pcmDir.string());

		if (std::filesystem::exists(pcmDir) && std::filesystem::is_directory(pcmDir))
		{
			for (const auto &entry : std::filesystem::directory_iterator(pcmDir))
			{
				if (entry.is_regular_file() && entry.path().extension() == ".wav")
				{
					std::string fullPath = entry.path().string();
					std::string filename = entry.path().filename().string();

					// Shorten long names for display
					std::string displayName = filename;
					if (displayName.length() > 25)
					{
						displayName = displayName.substr(0, 22) + "...";
					}

					pcmFilePaths_.push_back(fullPath);
					menu->addEntry(VSTGUI::UTF8String(displayName.c_str()));
					vstLog("[PCM] Added: " + filename);
				}
			}
		}

		if (pcmFilePaths_.empty())
		{
			menu->addEntry(VSTGUI::UTF8String("(no PCM files)"));
			vstLog("[PCM] No files found");
		}
		else
		{
			vstLog("[PCM] Found " + std::to_string(pcmFilePaths_.size()) + " files");
		}

		menu->setCurrent(0);
		pcmMenuPopulated_ = true;
	}

	//------------------------------------------------------------------------
	void ObsSynthController::loadPCMFile(int32_t index)
	{
		if (index < 0 || index >= (int32_t)pcmFilePaths_.size())
		{
			vstLog("[PCM] Invalid index: " + std::to_string(index));
			return;
		}

		std::string filePath = pcmFilePaths_[index];
		vstLog("[PCM] Loading: " + filePath);

		// Send message to processor to load the PCM file
		// Using VST3 message system
		if (auto message = allocateMessage())
		{
			message->setMessageID("LoadPCM");

			// Convert to TChar (wide string on Windows)
			Steinberg::Vst::TChar pathWide[1024];
			for (size_t i = 0; i < filePath.size() && i < 1023; i++)
			{
				pathWide[i] = static_cast<Steinberg::Vst::TChar>(filePath[i]);
			}
			pathWide[filePath.size() < 1023 ? filePath.size() : 1023] = 0;

			message->getAttributes()->setString("path", pathWide);
			sendMessage(message);
			message->release();
			vstLog("[PCM] Message sent to processor");
		}
	}

	//------------------------------------------------------------------------
} // namespace ModularSistemas
