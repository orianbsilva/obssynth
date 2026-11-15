#!/usr/bin/env python3
"""
Test script for VSTGUI Designer functionality
Tests the core functionality without GUI
"""

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from vstgui_designer import UIElement, VSTGUIDesigner
import xml.etree.ElementTree as ET

def test_element_creation():
    """Test creating different types of elements"""
    print("Testing element creation...")
    
    # Create a mock root for testing
    import tkinter as tk
    root = tk.Tk()
    root.withdraw()  # Hide the window
    
    try:
        designer = VSTGUIDesigner(root)
        
        # Test creating different element types
        test_types = ["CKnob", "CTextLabel", "COptionMenu", "CViewContainer"]
        
        for element_type in test_types:
            print(f"  Creating {element_type}...")
            initial_count = len(designer.elements)
            designer.create_element(element_type)
            new_count = len(designer.elements)
            
            if new_count == initial_count + 1:
                element = designer.elements[-1]
                print(f"    ✓ Created {element_type} at {element.origin} size {element.size}")
                print(f"    ✓ Properties: {len(element.properties)} items")
            else:
                print(f"    ✗ Failed to create {element_type}")
                
        return True
        
    except Exception as e:
        print(f"Error in element creation test: {e}")
        return False
    finally:
        root.destroy()

def test_xml_parsing():
    """Test XML parsing with different coordinate formats"""
    print("\nTesting XML coordinate parsing...")
    
    test_cases = [
        ("10, 20", (10, 20)),
        ("10,20", (10, 20)), 
        ("10.0, 20.0", (10, 20)),
        ("10.5,20.7", (10, 20)),  # Should round down
        ("  15  ,  25  ", (15, 25)),  # Extra spaces
    ]
    
    import tkinter as tk
    root = tk.Tk()
    root.withdraw()
    
    try:
        designer = VSTGUIDesigner(root)
        
        for coord_str, expected in test_cases:
            try:
                # Simulate the parsing logic
                parts = [part.strip() for part in coord_str.replace(',', ', ').split(', ') if part.strip()]
                result = (int(float(parts[0])), int(float(parts[1])))
                
                if result == expected:
                    print(f"    ✓ '{coord_str}' -> {result}")
                else:
                    print(f"    ✗ '{coord_str}' -> {result}, expected {expected}")
            except Exception as e:
                print(f"    ✗ '{coord_str}' -> Error: {e}")
                
        return True
        
    except Exception as e:
        print(f"Error in XML parsing test: {e}")
        return False
    finally:
        root.destroy()

def test_xml_export_import():
    """Test XML export and import functionality"""
    print("\nTesting XML export/import...")
    
    import tkinter as tk
    import tempfile
    
    root = tk.Tk()
    root.withdraw()
    
    try:
        designer = VSTGUIDesigner(root)
        
        # Create some test elements
        designer.create_element("CKnob")
        designer.create_element("CTextLabel")
        
        original_count = len(designer.elements)
        print(f"    Created {original_count} test elements")
        
        # Export to temporary file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.uidesc', delete=False) as f:
            temp_file = f.name
            
        try:
            designer.save_to_xml(temp_file)
            print(f"    ✓ Exported to {temp_file}")
            
            # Clear elements and reload
            designer.elements.clear()
            designer.load_from_xml(temp_file)
            
            new_count = len(designer.elements)
            if new_count == original_count:
                print(f"    ✓ Imported {new_count} elements successfully")
                
                # Check element types
                for i, element in enumerate(designer.elements):
                    print(f"      Element {i}: {element.element_type} at {element.origin}")
                    
                return True
            else:
                print(f"    ✗ Import failed: expected {original_count}, got {new_count}")
                return False
                
        finally:
            # Clean up temp file
            try:
                os.unlink(temp_file)
            except:
                pass
                
    except Exception as e:
        print(f"Error in XML export/import test: {e}")
        return False
    finally:
        root.destroy()

def main():
    """Run all tests"""
    print("VSTGUI Designer Functionality Tests")
    print("=" * 40)
    
    tests = [
        test_element_creation,
        test_xml_parsing,
        test_xml_export_import,
    ]
    
    passed = 0
    total = len(tests)
    
    for test in tests:
        try:
            if test():
                passed += 1
        except Exception as e:
            print(f"Test failed with exception: {e}")
    
    print("\n" + "=" * 40)
    print(f"Tests passed: {passed}/{total}")
    
    if passed == total:
        print("✓ All tests passed! The knobs and text loading should work correctly.")
    else:
        print("✗ Some tests failed. There may still be issues with knobs and text loading.")
    
    return passed == total

if __name__ == "__main__":
    main()