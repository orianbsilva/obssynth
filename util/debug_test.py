#!/usr/bin/env python3
"""
Debug test to see exactly what's being loaded from the XML
"""

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

import tkinter as tk
from vstgui_designer import VSTGUIDesigner

def debug_test():
    """Debug the text loading from editor.uidesc"""
    print("Debug Test of Editor.uidesc Text Loading")
    print("=" * 50)
    
    root = tk.Tk()
    root.withdraw()  # Hide the window
    
    try:
        designer = VSTGUIDesigner(root)
        designer._debug_text = True  # Enable debug output
        designer.load_from_xml('../src/vst/resource/editor.uidesc')
        
        print(f"\nLoaded {len(designer.elements)} elements")
        
        # Show text labels specifically
        text_labels = [e for e in designer.elements if e.element_type == "CTextLabel"]
        print(f"\nFound {len(text_labels)} text labels:")
        
        for i, label in enumerate(text_labels):
            text_content = label.properties.get("text", label.properties.get("title", "N/A"))
            print(f"  {i:2d}: at {str(label.origin):12s} - Text: '{text_content}'")
            print(f"      Properties: {dict(list(label.properties.items())[:5])}...")  # Show first 5 properties
            
        # Show knobs
        knobs = [e for e in designer.elements if e.element_type == "CKnob"]
        print(f"\nFound {len(knobs)} knobs:")
        for i, knob in enumerate(knobs):
            print(f"  {i:2d}: at {str(knob.origin):12s} size {str(knob.size):12s}")
            
        return True
        
    except Exception as e:
        print(f"✗ Error: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        root.destroy()

if __name__ == "__main__":
    debug_test()