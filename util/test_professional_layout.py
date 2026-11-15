#!/usr/bin/env python3
"""
Test script to load and verify the professional synth layout
"""

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

import tkinter as tk
from vstgui_designer import VSTGUIDesigner

def test_professional_layout():
    """Test loading the professional synth layout"""
    print("Testing Professional Synth Layout")
    print("=" * 40)
    
    root = tk.Tk()
    root.withdraw()  # Hide the window
    
    try:
        designer = VSTGUIDesigner(root)
        designer.load_from_xml('professional_synth.uidesc')
        
        print(f"✓ Loaded {len(designer.elements)} elements")
        print(f"✓ Canvas size: {designer.canvas_size}")
        
        # Count element types
        element_counts = {}
        for element in designer.elements:
            element_type = element.element_type
            element_counts[element_type] = element_counts.get(element_type, 0) + 1
            
        print("\nElement breakdown:")
        for element_type, count in sorted(element_counts.items()):
            print(f"  {element_type}: {count}")
            
        # Verify we have the expected sections
        text_labels = [e for e in designer.elements if e.element_type == "CTextLabel"]
        section_headers = [e for e in text_labels if e.properties.get("text") in ["MASTER", "OSCILLATOR", "ADSR ENVELOPE"]]
        
        print(f"\n✓ Found {len(section_headers)} section headers")
        for header in section_headers:
            print(f"  - {header.properties.get('text')}")
            
        # Verify we have knobs and other controls
        knobs = [e for e in designer.elements if e.element_type == "CKnob"]
        print(f"✓ Found {len(knobs)} knobs")
        
        containers = [e for e in designer.elements if e.element_type == "CViewContainer"]
        print(f"✓ Found {len(containers)} view containers")
        
        return True
        
    except Exception as e:
        print(f"✗ Error: {e}")
        return False
    finally:
        root.destroy()

if __name__ == "__main__":
    success = test_professional_layout()
    if success:
        print("\n🎉 Professional layout test passed!")
    else:
        print("\n❌ Professional layout test failed!")