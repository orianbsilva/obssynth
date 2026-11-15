import tkinter as tk
from vstgui_designer import VSTGUIDesigner

root = tk.Tk()
root.withdraw()
designer = VSTGUIDesigner(root)
designer.load_from_xml('sample_synth.uidesc')
print(f'Loaded {len(designer.elements)} elements:')
for i, element in enumerate(designer.elements):
    text = element.properties.get('text', element.properties.get('control-tag', 'N/A'))
    print(f'  {i}: {element.element_type} at {element.origin} - {text}')
root.destroy()