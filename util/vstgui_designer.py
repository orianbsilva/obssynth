"""
VSTGUI Designer - Visual VST UI Layout Designer
A Python application for designing VSTGUI layouts with drag and drop functionality.

Requirements:
- tkinter (usually comes with Python)
- PIL/Pillow for image handling

Usage:
python vstgui_designer.py
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog, colorchooser
import xml.etree.ElementTree as ET
import xml.dom.minidom as minidom
import json
import os
import math
from dataclasses import dataclass, asdict
from typing import Dict, List, Tuple, Optional

@dataclass
class UIColor:
    name: str
    rgba: str

@dataclass
class UIFont:
    name: str
    font_name: str
    size: int

@dataclass
class UIElement:
    element_type: str  # CKnob, CTextLabel, COptionMenu, CViewContainer
    origin: Tuple[int, int]
    size: Tuple[int, int]
    properties: Dict[str, str]

class VSTGUIDesigner:
    def __init__(self, root):
        self.root = root
        self.root.title("VSTGUI Designer")
        self.root.geometry("1200x800")
        
        # Data structures
        self.colors = [
            UIColor("Background", "#1a1a1aff"),         # Darker background
            UIColor("Panel", "#2a2a2aff"),              # Dark panel color
            UIColor("Text", "#f0f0f0ff"),               # Light text
            UIColor("TextSecondary", "#ccccccff"),      # Secondary text
            UIColor("Knob", "#cc7722ff"),               # Orange knob color
            UIColor("KnobHandle", "#ffffffff"),         # White handle
            UIColor("ButtonOff", "#444444ff"),          # Dark button
            UIColor("ButtonOn", "#ff9933ff"),           # Orange accent
            UIColor("Border", "#555555ff"),             # Border color
            UIColor("Highlight", "#ffaa44ff"),          # Highlight color
        ]
        
        self.fonts = [
            UIFont("default", "Arial", 10),
            UIFont("normal", "Arial", 12),
            UIFont("title", "Arial", 14),
            UIFont("small", "Arial", 9),
            UIFont("header", "Arial", 16),
        ]
        
        self.elements: List[UIElement] = []
        self.selected_element = None
        self.canvas_size = (500, 400)  # Default VST window size
        
        # Create UI
        self.setup_ui()
        
        # Drag and drop state
        self.drag_start_x = 0
        self.drag_start_y = 0
        self.dragging = False
        
    def setup_ui(self):
        # Main layout
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Left panel - Tool palette
        self.setup_tool_palette(main_frame)
        
        # Center - Canvas
        self.setup_canvas(main_frame)
        
        # Right panel - Properties
        self.setup_properties_panel(main_frame)
        
        # Bottom - Menu and status
        self.setup_menu_bar()
        
    def setup_tool_palette(self, parent):
        tool_frame = ttk.LabelFrame(parent, text="Controls", width=200)
        tool_frame.pack(side=tk.LEFT, fill=tk.Y, padx=(0, 5))
        tool_frame.pack_propagate(False)
        
        controls = [
            ("Knob", "CKnob"),
            ("Text Label", "CTextLabel"),
            ("Option Menu", "COptionMenu"),
            ("View Container", "CViewContainer"),
            ("Slider", "CSlider"),
            ("Button", "CTextButton"),
            ("Horizontal Slider", "CHorizontalSlider"),
            ("Vertical Slider", "CVerticalSlider"),
        ]
        
        for i, (display_name, class_name) in enumerate(controls):
            btn = ttk.Button(
                tool_frame, 
                text=display_name,
                command=lambda cn=class_name: self.create_element(cn)
            )
            btn.pack(fill=tk.X, padx=5, pady=2)
        
        # Separator
        ttk.Separator(tool_frame, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=10)
        
        # Canvas size controls
        size_frame = ttk.LabelFrame(tool_frame, text="Canvas Size")
        size_frame.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Label(size_frame, text="Width:").pack()
        self.width_var = tk.StringVar(value=str(self.canvas_size[0]))
        width_entry = ttk.Entry(size_frame, textvariable=self.width_var, width=10)
        width_entry.pack()
        
        ttk.Label(size_frame, text="Height:").pack()
        self.height_var = tk.StringVar(value=str(self.canvas_size[1]))
        height_entry = ttk.Entry(size_frame, textvariable=self.height_var, width=10)
        height_entry.pack()
        
        ttk.Button(
            size_frame, 
            text="Apply Size", 
            command=self.update_canvas_size
        ).pack(pady=5)
        
    def setup_canvas(self, parent):
        canvas_frame = ttk.LabelFrame(parent, text="Design Canvas")
        canvas_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5)
        
        # Canvas with scrollbars
        canvas_container = ttk.Frame(canvas_frame)
        canvas_container.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.canvas = tk.Canvas(
            canvas_container,
            bg="#1a1a1a",  # Darker background to match VST
            width=self.canvas_size[0],
            height=self.canvas_size[1],
            scrollregion=(0, 0, self.canvas_size[0], self.canvas_size[1])
        )
        
        # Scrollbars
        v_scrollbar = ttk.Scrollbar(canvas_container, orient=tk.VERTICAL, command=self.canvas.yview)
        h_scrollbar = ttk.Scrollbar(canvas_container, orient=tk.HORIZONTAL, command=self.canvas.xview)
        self.canvas.configure(yscrollcommand=v_scrollbar.set, xscrollcommand=h_scrollbar.set)
        
        # Pack scrollbars and canvas
        v_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        h_scrollbar.pack(side=tk.BOTTOM, fill=tk.X)
        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        
        # Canvas bindings
        self.canvas.bind("<Button-1>", self.on_canvas_click)
        self.canvas.bind("<B1-Motion>", self.on_canvas_drag)
        self.canvas.bind("<ButtonRelease-1>", self.on_canvas_release)
        self.canvas.bind("<Delete>", self.delete_selected)
        self.canvas.focus_set()  # Allow keyboard events
        
    def setup_properties_panel(self, parent):
        props_frame = ttk.LabelFrame(parent, text="Properties", width=250)
        props_frame.pack(side=tk.RIGHT, fill=tk.Y)
        props_frame.pack_propagate(False)
        
        # Scrollable properties
        canvas = tk.Canvas(props_frame)
        scrollbar = ttk.Scrollbar(props_frame, orient="vertical", command=canvas.yview)
        self.props_frame = ttk.Frame(canvas)
        
        self.props_frame.bind(
            "<Configure>",
            lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )
        
        canvas.create_window((0, 0), window=self.props_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        
        self.property_vars = {}
        self.update_properties_panel()
        
    def setup_menu_bar(self):
        menubar = tk.Menu(self.root)
        self.root.config(menu=menubar)
        
        # File menu
        file_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="File", menu=file_menu)
        file_menu.add_command(label="New", command=self.new_project)
        file_menu.add_command(label="Open...", command=self.open_file)
        file_menu.add_command(label="Save", command=self.save_file)
        file_menu.add_command(label="Save As...", command=self.save_as_file)
        file_menu.add_separator()
        file_menu.add_command(label="Export XML", command=self.export_xml)
        
        # Edit menu
        edit_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Edit", menu=edit_menu)
        edit_menu.add_command(label="Delete Selected", command=self.delete_selected)
        edit_menu.add_command(label="Clear All", command=self.clear_all)
        
        # Colors menu
        colors_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Colors", menu=colors_menu)
        colors_menu.add_command(label="Manage Colors...", command=self.manage_colors)
        
    def create_element(self, element_type):
        """Create a new UI element at a default position"""
        default_props = self.get_default_properties(element_type)
        
        # Set appropriate default sizes for different element types
        if element_type == "CKnob":
            default_size = (60, 60)
        elif element_type == "CVerticalSlider":
            default_size = (20, 100)
        elif element_type in ["CSlider", "CHorizontalSlider"]:
            default_size = (100, 20)
        elif element_type == "CViewContainer":
            default_size = (150, 100)
        else:
            default_size = (100, 25)
        
        element = UIElement(
            element_type=element_type,
            origin=(50, 50),
            size=default_size,
            properties=default_props
        )
        
        self.elements.append(element)
        self.selected_element = len(self.elements) - 1
        self.update_canvas()
        self.update_properties_panel()
        
    def get_default_properties(self, element_type):
        """Get default properties for each element type"""
        base_props = {
            "class": element_type,
            "mouse-enabled": "true"
        }
        
        if element_type == "CKnob":
            base_props.update({
                "control-tag": "0",
                "angle-range": "270",
                "angle-start": "135",
                "circle-drawing": "true",
                "corona-drawing": "true",
                "handle-color": "Text",
                "handle-line-width": "2"
            })
        elif element_type == "CTextLabel":
            base_props.update({
                "font": "default",
                "font-color": "Text",
                "text-alignment": "center"
            })
        elif element_type == "COptionMenu":
            base_props.update({
                "control-tag": "0",
                "font": "default",
                "font-color": "Text",
                "back-color": "~ BlackCColor",
                "frame-color": "~ BlackCColor"
            })
        elif element_type == "CViewContainer":
            base_props.update({
                "background-color": "Panel",
                "transparent": "false"
            })
        elif element_type == "CSlider":
            base_props.update({
                "control-tag": "0",
                "handle-color": "Text",
                "back-color": "~ BlackCColor"
            })
        elif element_type == "CHorizontalSlider":
            base_props.update({
                "control-tag": "0",
                "handle-color": "Text",
                "back-color": "~ BlackCColor",
                "orientation": "horizontal"
            })
        elif element_type == "CVerticalSlider":
            base_props.update({
                "control-tag": "0",
                "handle-color": "Text",
                "back-color": "~ BlackCColor",
                "orientation": "vertical"
            })
        elif element_type == "CTextButton":
            base_props.update({
                "control-tag": "0",
                "font": "default",
                "font-color": "Text",
                "title": "Button"
            })
            
        return base_props
        
    def update_canvas(self):
        """Redraw all elements on the canvas"""
        self.canvas.delete("all")
        
        # Draw grid
        self.draw_grid()
        
        # Draw elements
        for i, element in enumerate(self.elements):
            self.draw_element(element, i == self.selected_element)
            
    def draw_grid(self):
        """Draw a grid on the canvas for alignment"""
        grid_size = 10
        for x in range(0, self.canvas_size[0], grid_size):
            self.canvas.create_line(x, 0, x, self.canvas_size[1], fill="#333", width=1)
        for y in range(0, self.canvas_size[1], grid_size):
            self.canvas.create_line(0, y, self.canvas_size[0], y, fill="#333", width=1)
            
    def draw_element(self, element, selected=False):
        """Draw a single UI element"""
        x, y = element.origin
        w, h = element.size
        
        # Base rectangle
        color = "#5a4a3d" if element.element_type == "CViewContainer" else "#cc7722"
        outline = "#ff9933" if selected else "#666"
        
        rect = self.canvas.create_rectangle(
            x, y, x + w, y + h,
            fill=color,
            outline=outline,
            width=2 if selected else 1
        )
        
        # Element-specific drawing
        if element.element_type == "CKnob":
            # Draw professional-looking knob like in the VST
            margin = max(2, min(w, h) // 10)
            
            # Outer ring (darker)
            self.canvas.create_oval(
                x + margin, y + margin, 
                x + w - margin, y + h - margin,
                fill="#2a2a2a", outline="#444", width=2
            )
            
            # Inner knob surface (gradient effect with multiple circles)
            inner_margin = margin + 3
            self.canvas.create_oval(
                x + inner_margin, y + inner_margin,
                x + w - inner_margin, y + h - inner_margin,
                fill="#cc7722", outline="#ff9933", width=1
            )
            
            # Highlight for 3D effect
            highlight_margin = inner_margin + 2
            self.canvas.create_arc(
                x + highlight_margin, y + highlight_margin,
                x + w - highlight_margin, y + h - highlight_margin,
                start=45, extent=90, fill="#ffaa44", outline=""
            )
            
            # Handle (more prominent)
            center_x, center_y = x + w//2, y + h//2
            radius = min(w, h) // 3
            angle = math.radians(-45)  # Default knob position
            handle_x = center_x + radius * math.cos(angle)
            handle_y = center_y + radius * math.sin(angle)
            
            # Handle line with rounded end
            self.canvas.create_line(
                center_x, center_y, handle_x, handle_y,
                fill="#fff", width=4, capstyle=tk.ROUND
            )
            
            # Center dot
            self.canvas.create_oval(
                center_x - 2, center_y - 2, center_x + 2, center_y + 2,
                fill="#fff", outline=""
            )
        elif element.element_type == "CTextLabel":
            text = element.properties.get("text", element.properties.get("title", "Label"))
            # Better text rendering with outline for visibility
            font_size = 10
            # Draw text shadow for better visibility
            self.canvas.create_text(
                x + w//2 + 1, y + h//2 + 1,
                text=text, fill="#000", font=("Arial", font_size), anchor="center"
            )
            self.canvas.create_text(
                x + w//2, y + h//2,
                text=text, fill="#f0f0f0", font=("Arial", font_size), anchor="center"
            )
        elif element.element_type == "COptionMenu":
            # For option menus, we might need to show a placeholder or the waveform type
            text = element.properties.get("title", element.properties.get("text", "▼ Menu"))
            self.canvas.create_text(
                x + 5, y + h//2,
                text=text, fill="#fff", font=("Arial", 10), anchor="w"
            )
            # Draw dropdown arrow
            self.canvas.create_polygon(
                x + w - 15, y + h//2 - 3,
                x + w - 10, y + h//2 + 3,
                x + w - 5, y + h//2 - 3,
                fill="#fff"
            )
        elif element.element_type == "CTextButton":
            text = element.properties.get("title", element.properties.get("text", "Button"))
            self.canvas.create_text(
                x + w//2, y + h//2,
                text=text, fill="#fff", font=("Arial", 10)
            )
        elif element.element_type in ["CSlider", "CHorizontalSlider", "CVerticalSlider"]:
            # Draw slider background
            if element.element_type == "CVerticalSlider":
                # Vertical slider
                track_x = x + w//2 - 2
                self.canvas.create_rectangle(
                    track_x, y + 5, track_x + 4, y + h - 5,
                    fill="#333", outline="#555", width=1
                )
                # Handle
                handle_y = y + h//3  # Default position
                self.canvas.create_rectangle(
                    x + 2, handle_y - 5, x + w - 2, handle_y + 5,
                    fill="#cc7722", outline="#ff9933", width=1
                )
            else:
                # Horizontal slider
                track_y = y + h//2 - 2
                self.canvas.create_rectangle(
                    x + 5, track_y, x + w - 5, track_y + 4,
                    fill="#333", outline="#555", width=1
                )
                # Handle
                handle_x = x + w//3  # Default position
                self.canvas.create_rectangle(
                    handle_x - 5, y + 2, handle_x + 5, y + h - 2,
                    fill="#cc7722", outline="#ff9933", width=1
                )
            
        # Add type label for identification
        self.canvas.create_text(
            x, y - 2,
            text=element.element_type[1:],  # Remove 'C' prefix
            fill="#aaa", font=("Arial", 8), anchor="sw"
        )
        
    def on_canvas_click(self, event):
        """Handle canvas click events"""
        x, y = self.canvas.canvasx(event.x), self.canvas.canvasy(event.y)
        
        # Find clicked element
        clicked_element = None
        for i, element in enumerate(self.elements):
            ex, ey = element.origin
            ew, eh = element.size
            if ex <= x <= ex + ew and ey <= y <= ey + eh:
                clicked_element = i
                break
                
        if clicked_element is not None:
            self.selected_element = clicked_element
            self.drag_start_x = x - self.elements[clicked_element].origin[0]
            self.drag_start_y = y - self.elements[clicked_element].origin[1]
            self.dragging = True
        else:
            self.selected_element = None
            
        self.update_canvas()
        self.update_properties_panel()
        
    def on_canvas_drag(self, event):
        """Handle canvas drag events"""
        if self.dragging and self.selected_element is not None:
            x, y = self.canvas.canvasx(event.x), self.canvas.canvasy(event.y)
            new_x = max(0, min(x - self.drag_start_x, self.canvas_size[0] - self.elements[self.selected_element].size[0]))
            new_y = max(0, min(y - self.drag_start_y, self.canvas_size[1] - self.elements[self.selected_element].size[1]))
            
            self.elements[self.selected_element].origin = (int(new_x), int(new_y))
            self.update_canvas()
            self.update_properties_panel()
            
    def on_canvas_release(self, event):
        """Handle mouse release"""
        self.dragging = False
        
    def update_properties_panel(self):
        """Update the properties panel for the selected element"""
        # Clear existing properties
        for widget in self.props_frame.winfo_children():
            widget.destroy()
        self.property_vars.clear()
        
        if self.selected_element is None:
            ttk.Label(self.props_frame, text="No element selected").pack(pady=10)
            return
            
        element = self.elements[self.selected_element]
        
        # Element info
        ttk.Label(self.props_frame, text=f"Type: {element.element_type}", font=("Arial", 12, "bold")).pack(pady=5)
        
        # Position and size
        pos_frame = ttk.LabelFrame(self.props_frame, text="Position & Size")
        pos_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # Origin
        ttk.Label(pos_frame, text="X:").grid(row=0, column=0, sticky="w", padx=2)
        x_var = tk.StringVar(value=str(element.origin[0]))
        self.property_vars["origin_x"] = x_var
        ttk.Entry(pos_frame, textvariable=x_var, width=8).grid(row=0, column=1, padx=2)
        
        ttk.Label(pos_frame, text="Y:").grid(row=0, column=2, sticky="w", padx=2)
        y_var = tk.StringVar(value=str(element.origin[1]))
        self.property_vars["origin_y"] = y_var
        ttk.Entry(pos_frame, textvariable=y_var, width=8).grid(row=0, column=3, padx=2)
        
        # Size
        ttk.Label(pos_frame, text="W:").grid(row=1, column=0, sticky="w", padx=2)
        w_var = tk.StringVar(value=str(element.size[0]))
        self.property_vars["size_w"] = w_var
        ttk.Entry(pos_frame, textvariable=w_var, width=8).grid(row=1, column=1, padx=2)
        
        ttk.Label(pos_frame, text="H:").grid(row=1, column=2, sticky="w", padx=2)
        h_var = tk.StringVar(value=str(element.size[1]))
        self.property_vars["size_h"] = h_var
        ttk.Entry(pos_frame, textvariable=h_var, width=8).grid(row=1, column=3, padx=2)
        
        # Properties
        props_frame_inner = ttk.LabelFrame(self.props_frame, text="Properties")
        props_frame_inner.pack(fill=tk.X, padx=5, pady=5)
        
        row = 0
        for key, value in element.properties.items():
            ttk.Label(props_frame_inner, text=f"{key}:").grid(row=row, column=0, sticky="w", padx=2, pady=1)
            var = tk.StringVar(value=str(value))
            self.property_vars[f"prop_{key}"] = var
            ttk.Entry(props_frame_inner, textvariable=var, width=20).grid(row=row, column=1, padx=2, pady=1)
            row += 1
            
        # Update button
        ttk.Button(
            self.props_frame, 
            text="Apply Changes", 
            command=self.apply_property_changes
        ).pack(pady=10)
        
    def apply_property_changes(self):
        """Apply property changes to the selected element"""
        if self.selected_element is None:
            return
            
        element = self.elements[self.selected_element]
        
        # Update position and size
        try:
            x = int(self.property_vars["origin_x"].get())
            y = int(self.property_vars["origin_y"].get())
            w = int(self.property_vars["size_w"].get())
            h = int(self.property_vars["size_h"].get())
            
            element.origin = (x, y)
            element.size = (w, h)
        except ValueError:
            messagebox.showerror("Error", "Invalid position or size values")
            return
            
        # Update properties
        for key, var in self.property_vars.items():
            if key.startswith("prop_"):
                prop_name = key[5:]  # Remove "prop_" prefix
                element.properties[prop_name] = var.get()
                
        self.update_canvas()
        
    def update_canvas_size(self):
        """Update canvas size from the input fields"""
        try:
            width = int(self.width_var.get())
            height = int(self.height_var.get())
            self.canvas_size = (width, height)
            self.canvas.configure(width=width, height=height, scrollregion=(0, 0, width, height))
            self.update_canvas()
        except ValueError:
            messagebox.showerror("Error", "Invalid canvas size values")
            
    def delete_selected(self, event=None):
        """Delete the selected element"""
        if self.selected_element is not None:
            del self.elements[self.selected_element]
            self.selected_element = None
            self.update_canvas()
            self.update_properties_panel()
            
    def clear_all(self):
        """Clear all elements"""
        if messagebox.askyesno("Confirm", "Delete all elements?"):
            self.elements.clear()
            self.selected_element = None
            self.update_canvas()
            self.update_properties_panel()
            
    def manage_colors(self):
        """Open color management dialog"""
        color_window = tk.Toplevel(self.root)
        color_window.title("Manage Colors")
        color_window.geometry("400x300")
        
        # Color list
        listbox = tk.Listbox(color_window)
        listbox.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        for color in self.colors:
            listbox.insert(tk.END, f"{color.name}: {color.rgba}")
            
    def export_xml(self):
        """Export the current design to VSTGUI XML format"""
        filename = filedialog.asksaveasfilename(
            defaultextension=".uidesc",
            filetypes=[("VSTGUI files", "*.uidesc"), ("All files", "*.*")]
        )
        
        if filename:
            self.save_to_xml(filename)
            messagebox.showinfo("Success", f"Exported to {filename}")
            
    def save_to_xml(self, filename):
        """Save the design as VSTGUI XML"""
        root = ET.Element("vstgui-ui-description", version="1")
        
        # Fonts
        fonts_elem = ET.SubElement(root, "fonts")
        for font in self.fonts:
            ET.SubElement(fonts_elem, "font", {
                "font-name": font.font_name,
                "name": font.name,
                "size": str(font.size)
            })
            
        # Colors
        colors_elem = ET.SubElement(root, "colors")
        for color in self.colors:
            ET.SubElement(colors_elem, "color", {
                "name": color.name,
                "rgba": color.rgba
            })
            
        # Template
        template_attrs = {
            "background-color": "Background",
            "background-color-draw-style": "filled",
            "class": "CViewContainer",
            "mouse-enabled": "true",
            "name": "view",
            "opacity": "1",
            "origin": "0, 0",
            "size": f"{self.canvas_size[0]}, {self.canvas_size[1]}",
            "transparent": "false",
            "wants-focus": "false"
        }
        template = ET.SubElement(root, "template", template_attrs)
        
        # Elements
        for element in self.elements:
            view_attrs = {
                "class": element.element_type,
                "origin": f"{element.origin[0]}, {element.origin[1]}",
                "size": f"{element.size[0]}, {element.size[1]}"
            }
            view_attrs.update(element.properties)
            ET.SubElement(template, "view", view_attrs)
            
        # Custom section
        custom = ET.SubElement(root, "custom")
        ET.SubElement(custom, "attributes", name="FocusDrawing")
        ET.SubElement(custom, "attributes", {
            "Path": filename,
            "name": "VST3Editor"
        })
        ET.SubElement(custom, "attributes", {
            "SelectedTemplate": "view",
            "name": "UITemplateController"
        })
        
        # Bitmaps (empty)
        ET.SubElement(root, "bitmaps")
        
        # Write to file with pretty formatting
        rough_string = ET.tostring(root, 'utf-8')
        reparsed = minidom.parseString(rough_string)
        with open(filename, 'w', encoding='utf-8') as f:
            f.write(reparsed.toprettyxml(indent="\t")[23:])  # Remove XML declaration
            
    def new_project(self):
        """Create a new project"""
        if messagebox.askyesno("Confirm", "Create new project? This will clear all elements."):
            self.clear_all()
            
    def open_file(self):
        """Open a VSTGUI file"""
        filename = filedialog.askopenfilename(
            filetypes=[("VSTGUI files", "*.uidesc"), ("All files", "*.*")]
        )
        if filename:
            self.load_from_xml(filename)
            
    def load_from_xml(self, filename):
        """Load a VSTGUI XML file"""
        try:
            tree = ET.parse(filename)
            root = tree.getroot()
            
            # Clear current elements
            self.elements.clear()
            self.selected_element = None
            
            # Find template
            template = root.find("template")
            if template is not None:
                # Update canvas size
                size_attr = template.get("size", "500, 400")
                width, height = map(int, size_attr.split(", "))
                self.canvas_size = (width, height)
                self.width_var.set(str(width))
                self.height_var.set(str(height))
                self.update_canvas_size()
                
                # Load elements recursively from template
                self._load_views_recursive(template)
                    
            self.update_canvas()
            self.update_properties_panel()
            messagebox.showinfo("Success", f"Loaded {filename}")
            
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load file: {str(e)}")
    
    def _load_views_recursive(self, parent_element, offset_x=0, offset_y=0):
        """Recursively load view elements from XML, handling nested containers"""
        for view in parent_element.findall("view"):
            element_type = view.get("class", "CViewContainer")
            origin_str = view.get("origin", "0, 0")
            size_str = view.get("size", "100, 100")
            
            try:
                # Handle different coordinate formats (with or without spaces)
                origin_parts = [part.strip() for part in origin_str.replace(',', ', ').split(', ') if part.strip()]
                size_parts = [part.strip() for part in size_str.replace(',', ', ').split(', ') if part.strip()]
                
                if len(origin_parts) >= 2 and len(size_parts) >= 2:
                    origin = (int(float(origin_parts[0])) + offset_x, int(float(origin_parts[1])) + offset_y)
                    size = (int(float(size_parts[0])), int(float(size_parts[1])))
                else:
                    print(f"Warning: Invalid coordinate format for element {element_type}")
                    continue
            except (ValueError, IndexError) as e:
                print(f"Error parsing coordinates for {element_type}: {e}")
                continue
                
            # Get default properties and merge with XML properties
            default_properties = self.get_default_properties(element_type)
            properties = default_properties.copy()  # Start with defaults
            
            # Override with XML attributes (excluding position/size attributes)
            for key, value in view.attrib.items():
                if key not in ["class", "origin", "size"]:
                    properties[key] = value
                    
            element = UIElement(
                element_type=element_type,
                origin=origin,
                size=size,
                properties=properties
            )
            self.elements.append(element)
            
            # If this is a container, recursively load its children
            if element_type == "CViewContainer" and len(list(view.findall("view"))) > 0:
                self._load_views_recursive(view, origin[0], origin[1])
            
    def save_file(self):
        """Save current project"""
        # For simplicity, just export as XML
        self.export_xml()
        
    def save_as_file(self):
        """Save as new file"""
        self.export_xml()

def main():
    root = tk.Tk()
    app = VSTGUIDesigner(root)
    root.mainloop()

if __name__ == "__main__":
    main()