//------------------------------------------------------------------------
// Copyright(c) 2025 My Plug-in Company.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/icontroller.h"
#include "vstgui/lib/controls/ioptionmenulistener.h"
#include <vector>
#include <string>

namespace ModularSistemas
{

	//------------------------------------------------------------------------
	//  ObsSynthController
	//------------------------------------------------------------------------
	class ObsSynthController : public Steinberg::Vst::EditControllerEx1, public VSTGUI::IController
	{
	public:
		//------------------------------------------------------------------------
		ObsSynthController() = default;
		~ObsSynthController() SMTG_OVERRIDE = default;

		// Create function
		static Steinberg::FUnknown *createInstance(void * /*context*/)
		{
			return (Steinberg::Vst::IEditController *)new ObsSynthController;
		}

		//--- from IPluginBase -----------------------------------------------
		Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown *context) SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;

		//--- from EditController --------------------------------------------
		Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream *state) SMTG_OVERRIDE;
		Steinberg::IPlugView *PLUGIN_API createView(Steinberg::FIDString name) SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream *state) SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream *state) SMTG_OVERRIDE;

		//--- Parameter String Conversion ------------------------------------
		Steinberg::tresult PLUGIN_API getParamStringByValue(Steinberg::Vst::ParamID id,
															Steinberg::Vst::ParamValue valueNormalized, Steinberg::Vst::String128 string) SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API getParamValueByString(Steinberg::Vst::ParamID id,
															Steinberg::Vst::TChar *string, Steinberg::Vst::ParamValue &valueNormalized) SMTG_OVERRIDE;

		//--- from IController (VSTGUI) --------------------------------------
		VSTGUI::CView *verifyView(VSTGUI::CView *view, const VSTGUI::UIAttributes &attributes, const VSTGUI::IUIDescription *description) SMTG_OVERRIDE;
		void valueChanged(VSTGUI::CControl *control) SMTG_OVERRIDE;

		//---Interface---------
		DEFINE_INTERFACES
		// Here you can add more supported VST3 interfaces
		// DEF_INTERFACE (Vst::IXXX)
		// Don't add VSTGUI::IController to VST3 interface list
		END_DEFINE_INTERFACES(EditController)
		DELEGATE_REFCOUNT(EditController)

		//------------------------------------------------------------------------
	protected:
		VSTGUI::IOptionMenuListener *waveformMenuListener_{nullptr};
		VSTGUI::IOptionMenuListener *fmAlgorithmMenuListener_{nullptr};

		// PCM file management
		std::vector<std::string> pcmFilePaths_;
		bool pcmMenuPopulated_{false};
		void populatePCMMenu(VSTGUI::COptionMenu *menu);
		void loadPCMFile(int32_t index);
	};
	//------------------------------------------------------------------------
} // namespace ModularSistemas
