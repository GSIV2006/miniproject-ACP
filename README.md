# 2D ASCII Graphics Editor

An interactive, color-themed, vector-based Terminal User Interface (TUI) 2D graphics editor written in **C** using the standard Unix **curses** library. 

This application is specifically designed and optimized to run out-of-the-box on standard macOS (`80x24` rows/columns) and Linux terminal windows. It supports smooth vector-based zooming and a 7-color palette.

---

## Features

- **Vector-Raster Engine**: Stored shapes are dynamically rasterized onto a 2D canvas. You can layer, select, modify, or delete shapes on the fly without leaving stray character artifacts on the background.
- **Advanced Rasterization**:
  - **Lines**: Rendered using Bresenham's integer line algorithm.
  - **Rectangles**: Empty outline or solid fill.
  - **Circles**: Aspect-ratio corrected (compensated for narrow terminal cells) so circles look perfectly round. Uses a gap-free double boundary check.
  - **Triangles**: Outline drawing or solid fill using barycentric/edge sign checks.
- **Dynamic 7-Color Palette**: Assign individual colors to shapes: Green, Cyan, Yellow (Default), Red, Blue, Magenta, and White.
- **Selected Shape Highlighting**: Scroll through the shape sidebar inspector using the arrow keys to instantly light up the selected shape on the canvas using inverted color blocks (`A_REVERSE`).
- **Interactive Vector Zoom**: Press `+` and `-` to scale the entire canvas between `20%` and `400%` in real-time.
- **Resizing Protection**: Safely halts drawing and warns you if your terminal window drops below the required `80x24` size, resuming instantly once resized back.
- **Art Print on Exit**: On hitting `Q` to quit, the program cleanly closes curses and prints a copy-pasteable copy of your final ASCII drawing straight to standard output (`stdout`).

---

## Controls

| Key | Action | Description |
| :--- | :--- | :--- |
| **`A` / `a`** | Add Object | Opens prompt to create a Line, Rectangle, Circle, or Triangle. |
| **`D` / `d`** | Delete Object | Deletes the currently highlighted shape from the canvas. |
| **`M` / `m`** | Modify Object | Edit coordinates/dimensions, draw character, toggle fill state, or color of the selected shape. |
| **`+` / `=`** | Zoom In | Scales up the drawing by `10%` (max `400%`). |
| **`-` / `_`** | Zoom Out | Scales down the drawing by `10%` (min `20%`). |
| **`Up / Down`** | Select Shape | Scroll through the active shapes sidebar list and highlights the shape on the canvas. |
| **`Q` / `q`** | Exit Editor | Quits the TUI and dumps the final ASCII drawing to standard output. |

---

## Compilation & Execution

### Prerequisites
You need a standard C compiler (`gcc` or `clang`) and the development headers for the `curses`/`ncurses` library (pre-installed on macOS).

### Compile
Run this command in your terminal to build the editor, making sure to link the curses and math libraries:
```bash
gcc -Wall -O2 editor.c -o editor -lcurses -lm
