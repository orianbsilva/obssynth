# VSTGUI COptionMenu Implementation Guide

## Overview
This document describes the correct way to implement dropdown menus (COptionMenu/select) in VSTGUI for VST3 plugins.

## Problem
COptionMenu controls don't automatically populate with options. The menu items must be provided through the VST3 parameter system.

## Solution: Use StringListParameter

### 1. Controller - Define the Parameter

In the controller's `initialize()` method, use `StringListParameter` instead of a regular parameter:

```cpp
#include "public.sdk/source/vst/vstparameters.h"

// In initialize():
auto* filterTypeParam = new Steinberg::Vst::StringListParameter(
    USTRING("Filter Type"),    // Display name
    kParamFilterType           // Parameter ID
);

// Add each option with appendString()
filterTypeParam->appendString(USTRING("Lowpass"));
filterTypeParam->appendString(USTRING("Highpass"));
filterTypeParam->appendString(USTRING("Bandpass"));

// Add to parameters collection
parameters.addParameter(filterTypeParam);
```

### 2. UIDESC - Link Control to Parameter

In `editor.uidesc`, the COptionMenu must have:
- `control-tag` matching the parameter tag defined in `<control-tags>`
- `min-value="0"` and `max-value` = number of options - 1

```xml
<control-tags>
    <control-tag name="FilterType" tag="10"/>
</control-tags>

<!-- In views: -->
<view class="COptionMenu" 
      control-tag="FilterType"
      min-value="0" 
      max-value="2"
      menu-popup-style="true"
      ... />
```

### 3. Processor - Handle the Value

In the processor, convert normalized value to index:

```cpp
case kParamFilterType:
{
    // Convert normalized (0.0-1.0) to integer index
    int filterType = std::min<int>(
        (int)(NUM_FILTER_TYPES * value), 
        NUM_FILTER_TYPES - 1
    );
    paramState.filterType = filterType;
    break;
}
```

## Key Points

| Aspect | Correct Approach |
|--------|------------------|
| Parameter type | `StringListParameter` (NOT regular Parameter) |
| Add options | `appendString(USTRING("Option"))` for each |
| Flag | Automatically sets `ParameterInfo::kIsList` |
| UIDESC | `control-tag` must match parameter tag |
| min/max | `min-value="0"`, `max-value = options - 1` |

## Why Regular Parameters Don't Work

Regular parameters don't have the `kIsList` flag, so VSTGUI doesn't know to:
1. Query the parameter for string representations
2. Populate the menu with options

`StringListParameter` automatically:
- Sets `kIsList` flag
- Stores option strings
- Implements `toString()` to convert index to string
- Updates `stepCount` as options are added

## Example: PCM Selector

For dynamic menus (like file selectors), you have two options:

### Option A: Pre-populate with StringListParameter
```cpp
auto* pcmParam = new StringListParameter(USTRING("PCM Sample"), kPCMSelector);
pcmParam->appendString(USTRING("None"));
pcmParam->appendString(USTRING("sine_wave.wav"));
pcmParam->appendString(USTRING("saw_wave.wav"));
parameters.addParameter(pcmParam);
```

### Option B: Dynamic population in valueChanged
For truly dynamic content (scanning folders), populate the COptionMenu directly:
```cpp
void valueChanged(CControl* control) {
    if (control->getTag() == kPCMSelector) {
        auto* menu = dynamic_cast<COptionMenu*>(control);
        if (menu && menu->getNbEntries() == 0) {
            // First access - populate menu
            for (auto& file : pcmFiles) {
                menu->addEntry(file.c_str());
            }
        }
    }
}
```

## SDK References

- `public.sdk/source/vst/vstparameters.h` - StringListParameter class
- `vstgui4/vstgui/lib/controls/coptionmenu.h` - COptionMenu control
- `public.sdk/samples/vst/note_expression_synth/` - Working example

## Tested in VST SDK 3.7+
