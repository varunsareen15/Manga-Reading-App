# C Manga Reader

A lightweight, cross-platform manga reader written in C. It reads `.cbz` files directly from memory, supports High-DPI (Retina) displays, and uses high-quality texture filtering for crisp text rendering.

## Features

* **Format Support:** Reads `.cbz` (Zip) archives containing PNG or JPG images.
* **Zero Extraction:** Loads pages directly from RAM without extracting files to the disk.
* **High Quality Rendering:** Uses Anisotropic/Linear filtering and High-DPI support to prevent pixelation on modern screens.
* **Smart Navigation:** Includes "Go to Page" jump, 10-page skipping, and standard manga (RTL) controls.
* **Page Counter:** Visual overlay showing current progress (e.g., "5 / 200").
* **Modular Architecture:** Clean separation between App Logic, Zip Handling, and Rendering.

## Prerequisites

You need a C compiler (`gcc`) and the development headers for **SDL2**, **SDL2_image**, and **libzip**.

### macOS (Homebrew)
```bash
brew install sdl2 sdl2_image sdl2_ttf libzip
```
### Linux (Debian/Ubuntu)
```bash
sudo apt-get install build-essential libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libzip-dev
```
### Windows (MSYS2 MinGW 64-bit)
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-SDL2_ttf mingw-w64-x86_64-libzip
```

## Building the Project

1.  **Open Terminal:** Open your terminal or command prompt.
2.  **Navigate:** Go to the project root directory (where the `Makefile` is located):
    ```bash
    cd path/to/manga_reader
    ```
3.  **Download a Font:** Download any ttf font and place in the root directory. Rename the font file to font.ttf.
4.  **Compile:** Run the make command:
    ```bash
    make
    ```
5.  **Verify:** If successful, an executable binary named `manga_reader` (or `manga_reader.exe` on Windows) will appear in the folder.
6.  **Rebuilding (Optional):** To remove old build files and start fresh, run:
    ```bash
    make clean
    ```

## How to Run

Pass the path to a `.cbz` file as a command-line argument.

### macOS / Linux
```bash
./manga_reader "path/to/my_manga.cbz"
```
### Windows (PowerShell / Command Prompt)
```bash
.\manga_reader.exe "path\to\my_manga.cbz"
```
Note: If your filename contains spaces (e.g., One Piece Vol 1.cbz), you must enclose the path in quotes.

## Controls

| Key | Action |
| :--- | :--- |
| **Left Arrow** | **Next Page** (Advances forward, Manga style) |
| **Right Arrow** | **Previous Page** (Goes back) |
| **Shift + Left** | **Skip Forward 10 Pages** |
| **Shift + Right** | **Skip Backward 10 Pages** |
| **Home / End** | Jump to First / Last Page |
| **G** | **Go to Page** (Opens input box) |
| **F** | Toggle Fullscreen Mode |
| **ESC** | Cancel Input / Exit Fullscreen / Quit |

### Inside "Go to Page" Mode
| Key | Action |
| :--- | :--- |
| **0-9** | Type Page Number |
| **Enter** | Confirm Jump |
| **Backspace** | Delete Digit |
| **ESC** | Cancel |


## Project Structure

```text
manga_reader/
├── Makefile            # Build configuration (run 'make' here)
├── README.md           # Project documentation
├── font.ttf            # Required font file (User provided)
├── include/            # Header files (.h)
│   ├── cbz_handler.h   # Definitions for Zip extraction & Image handling
│   └── render_engine.h # Definitions for SDL2 windowing & texture logic
├── src/                # Source code (.c)
│   ├── main.c          # Entry point, Event Loop, & Input handling
│   ├── cbz_handler.c   # Logic for reading .cbz archives from memory
│   └── render_engine.c # Logic for high-DPI rendering & scaling
└── build/              # Intermediate object files (generated automatically)
```

## License
MIT License
