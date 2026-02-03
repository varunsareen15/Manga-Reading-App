# C Manga Reader

A lightweight, cross-platform manga reader written in C. It reads `.cbz` files directly from memory, supports High-DPI (Retina) displays, and automatically adapts controls for Manga (Right-to-Left) or Comics (Left-to-Right).

## Features

* **Format Support:** Reads `.cbz` (Zip) archives containing PNG or JPG images.
* **Zero Extraction:** Loads pages directly from RAM without extracting files to disk.
* **Smart Library:** Automatically detects reading direction (Manga vs. Comic) based on file location.
* **Context-Aware Navigation:**
    * **Manga Mode:** Left Arrow = Next Page.
    * **Comic Mode:** Right Arrow = Next Page.
* **High Quality Rendering:** Uses Anisotropic filtering and High-DPI support for crisp text.
* **Robust Bookmarks:** Uses a reliable **SQLite database** to save progress, even if the app crashes.
* **Power Tools:** "Go to Page" jump, 10-page skipping, and fullscreen toggle.

## Prerequisites

You need a C compiler (`gcc`) and the development headers for **SDL2**, **SDL2_image**, **SDL2_ttf**, **libzip**, and **SQLite3**.

### macOS (Homebrew)
```bash
brew install sdl2 sdl2_image sdl2_ttf libzip sqlite
```
### Linux (Debian/Ubuntu)
```bash
sudo apt-get install build-essential libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libzip-dev libsqlite3-dev
```
### Windows (MSYS2 MinGW 64-bit)
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-SDL2_ttf mingw-w64-x86_64-libzip mingw-w64-x86_64-sqlite3
```

## Library Setup
To enable **Auto-Reading Mode** (Manga vs Comic), organize your files into a `library` folder with specific subfolders:
```text
manga_reader/
└── library/
    ├── manga/   <-- Files here use Right-to-Left navigation
    │   └── One Piece/
    │       └── vol1.cbz
    └── comic/   <-- Files here use Left-to-Right navigation
        └── Batman/
            └── issue1.cbz
```

* Manga Mode: Activated if path contains /manga/.
* Comic Mode: Activated if path contains /comic/.

## Building the Project

1.  **Open Terminal:** Open your terminal or command prompt.
2.  **Navigate:** Go to the project root directory (where the `Makefile` is located):
    ```bash
    cd path/to/manga_reader
    ```
3.  **Download a Font:** Download any ttf font and place in the root directory. Rename the font file to `font.ttf`.
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
./manga_reader "library/manga/One Piece/vol1.cbz"
```
### Windows (PowerShell / Command Prompt)
```bash
.\manga_reader.exe "library\comic\Batman\issue1.cbz"
```
Note: If your filename contains spaces (e.g., One Piece Vol 1.cbz), you must enclose the path in quotes.

## Controls

| Key | Action |
| :--- | :--- |
| **Left/Right** | **Next/Prev Page** (Context Sensitive) |
| **Shift + Left/Right** | **Skip 10 Pages** (Follows reading direction) |
| **B / E** | Jump to First / Last Page |
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
├── Makefile           # Build configuration
├── README.md          # Documentation
├── library.db         # SQLite Database (Auto-generated)
├── font.ttf           # Font file (User provided)
├── library/           # Your Manga/Comic files
├── include/           # Header files
│   ├── bookmark_manager.h
│   ├── cbz_handler.h
│   └── render_engine.h
├── src/               # Source code
│   ├── main.c
│   ├── bookmark_manager.c
│   ├── cbz_handler.c
│   └── render_engine.c
└── build/             # Compiled object files
```

## License
MIT License
