#!/usr/bin/env python3
"""
Test script to load the actual editor.uidesc file
"""

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

import tkinter as tk
from vstgui_designer import VSTGUIDesigner

def test_editor_loading():
    """Test loading the actual editor.uidesc file"""
    print("Testing Editor.uidesc Loading")
    print("=" * 40)
    
    root = tk.Tk()
    root.withdraw()  # Hide the window
    
    try:
        designer = VSTGUIDesigner(root)
        designer.load_from_xml('../src/vst/resource/editor.uidesc')
        
        print(f"✓ Loaded {len(designer.elements)} elements")
        print(f"✓ Canvas size: {designer.canvas_size}")
        
        print("\nElement details:")
        for i, element in enumerate(designer.elements):
            text_content = element.properties.get("text", element.properties.get("title", "N/A"))
            print(f"  {i:2d}: {element.element_type:15s} at {str(element.origin):12s} size {str(element.size):12s} - '{text_content}'")
            
        # Count element types
        element_counts = {}
        for element in designer.elements:
            element_type = element.element_type
            element_counts[element_type] = element_counts.get(element_type, 0) + 1
            
        print(f"\nElement breakdown:")
        for element_type, count in sorted(element_counts.items()):
            print(f"  {element_type}: {count}")
            
        # Look for specific issues
        knobs = [e for e in designer.elements if e.element_type == "CKnob"]
        text_labels = [e for e in designer.elements if e.element_type == "CTextLabel"]
        
        print(f"\n✓ Found {len(knobs)} knobs")
        print(f"✓ Found {len(text_labels)} text labels")
        
        if len(knobs) == 0:
            print("❌ No knobs found - this indicates a parsing issue")
        if len(text_labels) == 0:
            print("❌ No text labels found - this indicates a parsing issue")
            
        return True
        
    except Exception as e:
        print(f"✗ Error: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        root.destroy()

if __name__ == "__main__":
    test_editor_loading()