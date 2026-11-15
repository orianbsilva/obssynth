# VSTGUI Designer Documentation

## Overview

The VSTGUI Designer is a Python application that provides a visual, drag-and-drop interface for creating VST plugin user interfaces. It generates XML files compatible with the VSTGUI framework used by VST3 plugins.

## Installation & Requirements

### Requirements
- Python 3.7+
- tkinter (usually included with Python)
- xml libraries (included with Python)

### Running the Application
```bash
cd c:\Users\orian\dev\synth\util
python vstgui_designer.py
```

## VSTGUI Concepts

### 1. UI Description File Structure
A VSTGUI `.uidesc` file contains:
- **Fonts**: Font definitions used throughout the interface
- **Colors**: Named color palette for consistent theming
- **Templates**: The main UI layout container
- **Views**: Individual UI elements (knobs, labels, buttons, etc.)
- **Custom**: Metadata and editor-specific settings
- **Bitmaps**: Image resources (optional)

### 2. Coordinate System
- Origin (0,0) is at the top-left corner
- X increases to the right, Y increases downward
- All measurements are in pixels
- Elements are positioned by their top-left corner

### 3. Common UI Elements

#### CKnob (Rotary Knob)
- **Purpose**: Continuous parameter control
- **Key Properties**:
  - `control-tag`: Parameter ID (0-based)
  - `angle-range`: Total rotation range in degrees (usually 270)
  - `angle-start`: Starting angle in degrees (usually 135)
  - `circle-drawing`: Enable circular knob appearance
  - `corona-drawing`: Enable outer ring
  - `handle-color`: Color of the knob indicator

#### CTextLabel (Text Display)
- **Purpose**: Static text display
- **Key Properties**:
  - `text`: The text content
  - `font`: Font name from fonts section
  - `font-color`: Text color name
  - `text-alignment`: left, center, right

#### COptionMenu (Dropdown Menu)
- **Purpose**: Discrete parameter selection
- **Key Properties**:
  - `control-tag`: Parameter ID
  - `title`: Default/initial text
  - `max-value`: Maximum option index
  - `min-value`: Minimum option index (usually 0)

#### CViewContainer (Panel/Group)
- **Purpose**: Grouping and organizing other elements
- **Key Properties**:
  - `background-color`: Panel background color
  - `transparent`: true/false for background visibility
  - Can contain child `<view>` elements

#### CSlider (Linear Slider)
- **Purpose**: Linear parameter control
- **Key Properties**:
  - `control-tag`: Parameter ID
  - `handle-color`: Slider handle color
  - `back-color`: Track background color
  - `orientation`: horizontal/vertical

#### CTextButton (Clickable Button)
- **Purpose**: Momentary or toggle actions
- **Key Properties**:
  - `control-tag`: Parameter ID
  - `title`: Button text
  - `font`: Font for text

### 4. Color System
Colors are defined once and referenced by name:
```xml
<colors>
    <color name="Background" rgba="#2a1f1aff"/>
    <color name="Text" rgba="#ffffffff"/>
</colors>
```
- Colors use RGBA format: #RRGGBBAA
- Alpha channel: ff = opaque, 00 = transparent
- Reference colors by name in elements: `font-color="Text"`

### 5. Font System
Fonts are defined once and referenced by name:
```xml
<fonts>
    <font font-name="Arial" name="default" size="12"/>
    <font font-name="Arial" name="title" size="16"/>
</fonts>
```

## Using the VSTGUI Designer

### 1. Interface Layout

#### Left Panel - Controls Palette
- **Control Buttons**: Click to create new UI elements
  - Knob: Creates rotary knobs for continuous parameters
  - Text Label: Creates static text displays
  - Option Menu: Creates dropdown menus
  - View Container: Creates grouping panels
  - Slider: Creates linear sliders
  - Button: Creates clickable buttons

- **Canvas Size Controls**: Set the overall window dimensions
  - Standard VST sizes: 500x400, 800x600, 1024x768

#### Center Panel - Design Canvas
- **Visual Design Area**: Drag and drop interface
- **Grid**: Alignment guides (10-pixel grid)
- **Element Selection**: Click elements to select
- **Drag to Move**: Drag selected elements to reposition
- **Visual Feedback**: Selected elements highlighted in orange

#### Right Panel - Properties
- **Position & Size**: Precise numeric control
  - X, Y: Element position
  - W, H: Element dimensions
- **Element Properties**: All VSTGUI attributes
  - control-tag: Links to VST parameters
  - Visual properties: colors, fonts, styles
  - Behavior properties: mouse-enabled, etc.

### 2. Workflow

#### Creating a Basic Interface
1. **Set Canvas Size**: Enter desired width/height, click "Apply Size"
2. **Add Elements**: Click control buttons to create elements
3. **Position Elements**: Drag elements or use Properties panel
4. **Configure Properties**: 
   - Set control-tag to link to VST parameters
   - Adjust visual properties (colors, fonts)
   - Set text content for labels and buttons
5. **Group Elements**: Use View Containers for organization
6. **Export**: File → Export XML to save .uidesc file

#### Parameter Mapping
- VST parameters are numbered starting from 0
- Set `control-tag` property to link UI elements to parameters
- Example: control-tag="0" links to first parameter (Master Volume)

#### Best Practices
- **Consistent Spacing**: Use 10-pixel grid for alignment
- **Logical Grouping**: Use View Containers for related controls
- **Consistent Colors**: Stick to defined color palette
- **Clear Labels**: Use descriptive text for all controls
- **Reasonable Sizes**: 
  - Knobs: 40-80 pixels diameter
  - Buttons: minimum 60x25 pixels
  - Labels: match content width

### 3. Menu Functions

#### File Menu
- **New**: Clear all elements, start fresh
- **Open**: Load existing .uidesc file
- **Save/Save As**: Export current design
- **Export XML**: Generate VSTGUI-compatible XML

#### Edit Menu
- **Delete Selected**: Remove selected element (or press Delete key)
- **Clear All**: Remove all elements

#### Colors Menu
- **Manage Colors**: View and edit color palette

### 4. Keyboard Shortcuts
- **Delete**: Remove selected element
- **Click**: Select element
- **Drag**: Move selected element

## Integration with VST Project

### 1. File Placement
Save the generated .uidesc file to:
```
c:\Users\orian\dev\synth\src\vst\resource\editor.uidesc
```

### 2. Parameter Mapping
Ensure control-tag values match your VST parameter IDs:
```cpp
// In parameters.h
enum ParameterID {
    kMasterVolume = 0,    // control-tag="0"
    kMasterPan = 1,       // control-tag="1"
    kWaveform = 2,        // control-tag="2"
    // etc.
};
```

### 3. Rebuild Project
After updating the .uidesc file:
```bash
.\build_cmake.bat
```

## Advanced Features

### 1. Custom Colors
Add new colors to the palette by editing the colors list in the designer or manually in the XML:
```xml
<color name="CustomBlue" rgba="#4a90e2ff"/>
```

### 2. Complex Layouts
- Use nested View Containers for complex groupings
- Create sections for different parameter categories
- Consider visual hierarchy and user workflow

### 3. Control Properties Reference

#### Common Properties (All Elements)
- `class`: Element type (CKnob, CTextLabel, etc.)
- `origin`: "x, y" position
- `size`: "width, height" dimensions
- `mouse-enabled`: "true"/"false"

#### Knob-Specific Properties
- `control-tag`: Parameter index
- `angle-range`: "270" (degrees of rotation)
- `angle-start`: "135" (starting angle)
- `circle-drawing`: "true"
- `corona-drawing`: "true" (outer ring)
- `handle-color`: Color name
- `handle-line-width`: "2" (pixels)
- `wheel-inc-value`: "0.1" (mouse wheel sensitivity)

#### Text Properties
- `text`: Text content
- `font`: Font name
- `font-color`: Color name
- `text-alignment`: "left"/"center"/"right"

#### Container Properties
- `background-color`: Background color name
- `transparent`: "true"/"false"

## Troubleshooting

### Common Issues
1. **Elements not visible**: Check background colors and transparency
2. **Controls not responding**: Verify control-tag matches parameter ID
3. **Text not showing**: Check font-color contrasts with background
4. **Layout looks wrong**: Verify canvas size matches VST window size

### Validation
- Use VST validator to check generated plugin
- Test in DAW to verify UI functionality
- Check console output for VSTGUI errors

## Example Layouts

### Simple Synthesizer Layout
```
+----------------------------------+
| ObsSynth - Polyphonic Synthesizer|
+----------+----------+------------+
| Master   | Oscillator| ADSR      |
| [Vol][Pan]| [Wave][Vol]|[A][D][S][R]|
+----------+----------+------------+
| Effects                          |
| [Reverb] [Chorus] [Filter]       |
+----------------------------------+
```

### Organized with Panels
```
+----------------------------------+
|             Title                |
+--------+--------+--------+-------+
|Master  |Basic   |Wave    |FM     |
|Section |Osc     |Table   |Synth  |
|[Vol]   |[Wave]  |[Pos]   |[Algo] |
|[Pan]   |[Vol]   |[Morph] |[Mod]  |
+--------+--------+--------+-------+
|ADSR Envelope    |Effects         |
|[A] [D] [S] [R]  |[Rev] [Cho]     |
+-----------------+----------------+
```

This designer provides a visual way to create professional VST interfaces without manually editing XML files.