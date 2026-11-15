#!/usr/bin/env python3
"""
Visual test to verify knobs and text are rendering correctly
"""

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

import tkinter as tk
from vstgui_designer import VSTGUIDesigner

def visual_test():
    """Create a simple visual test of the editor.uidesc rendering"""
    print("Visual Test of Editor.uidesc Rendering")
    print("=" * 40)
    
    root = tk.Tk()
    root.title("Editor.uidesc Visual Test")
    
    try:
        designer = VSTGUIDesigner(root)
        designer.load_from_xml('../src/vst/resource/editor.uidesc')
        
        print(f"✓ Loaded {len(designer.elements)} elements successfully")
        print("✓ Designer window should now be visible with all knobs and text rendered")
        print("\nExpected to see:")
        print("  - 8 knobs with 3D orange styling")
        print("  - 18 text labels with proper text content")
        print("  - 1 option menu (waveform selector)")
        print("  - 4 view containers (panels)")
        print("\nIf you can see all these elements, the rendering is working correctly!")
        print("\nClose the window to continue...")
        
        # Run the GUI
        root.mainloop()
        
        return True
        
    except Exception as e:
        print(f"✗ Error: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    visual_test()