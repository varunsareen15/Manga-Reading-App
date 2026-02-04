# C Manga Reader

A lightweight, cross-platform manga reader written in C. It reads `.cbz` files directly from memory, supports High-DPI (Retina) displays, and automatically adapts controls for **Manga** (Right-to-Left), **Comics** (Left-to-Right), **Manhua** (Left-to-Right) or **Manhwa/Webtoons** (Vertical Scroll).

## Features

* **Format Support:** Reads `.cbz` (Zip) archives containing PNG or JPG images.
* **Zero Extraction:** Loads pages directly from RAM without extracting files to disk.
* **Smart Library:** Automatically detects reading direction based on file location (Manga, Comic, Manhwa/Webtoon).
* **Infinite Scrolling:** Supports **continuous vertical scrolling** for Webtoons (seamlessly stitches pages together).
* **Context-Aware Navigation:**
    * **Manga:** Left Arrow = Next Page.
    * **Comic/Manhua:** Right Arrow = Next Page.
    * **Manhwa:** Down Arrow / Mouse Wheel = Scroll Down.
* **High Quality Rendering:** Uses Anisotropic filtering and High-DPI support for crisp text.
* **Robust Bookmarks:** Uses a reliable **SQLite database** to save progress and exact scroll position.
* **Seamless Volume Switching:** Automatically detects the next file in the folder and prompts you to continue when you reach the last page.
* **Viewing Modes:**
    * **Standard:** Single Page, Double Page, and Double Page (Cover Offset).
    * **Webtoon:** Fit Height (Whole Page) or Fit Width (Zoomed Scroll).

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
    ├── manga/   <-- Right-to-Left (Standard Manga)
    │   └── One Piece/
    │       └── vol1.cbz
    ├── comic/   <-- Left-to-Right (Western Comics)
    │   └── Batman/
    │       └── issue1.cbz
    ├── manhua/  <-- Left-to-Right (Chinese Manhua)
    │   └── Tales of Demons/
    │       └── ch1.cbz
    └── manhwa/  <-- Vertical Scroll (Korean Webtoons)
        └── Solo Leveling/
            └── ch1.cbz
```

* Manga Mode: Activated if path contains /manga/.
* Comic Mode: Activated if path contains /comic/.
* Manhua Mode: Activated if path contains /manhua/.
* Manhwa Mode: Activated if path contains /manhwa/ (or /webtoon/).

## File Naming Convention (Auto-Next)

To use the "Next Volume" feature, your files must be named sequentially. The reader sorts files alphabetically.

**Important:** Use **Zero-Padding** for numbers to ensure correct order.

* ✅ **Good:** `vol01.cbz`, `vol02.cbz`, ... `vol10.cbz`
* ❌ **Bad:** `vol1.cbz`, `vol2.cbz`, ... `vol10.cbz` (Computer sorting places `10` before `2`)

**Example Structure:**
```text
library/manga/Naruto/
├── v01.cbz
├── v02.cbz
└── v03.cbz
```

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

The controls change automatically depending on the type of book you are reading.

### Standard Mode (Manga / Comic / Manhua)
| Key | Action |
| :--- | :--- |
| **Left / Right** | **Next / Prev Page** (Context Sensitive) |
| **Shift + Arrows** | **Skip 10 Pages** (Follows reading direction) |
| **S** | **Single Page View** |
| **D** | **Double Page View** (Standard 0-1, 2-3) |
| **Shift + D** | **Double Page View** (Cover Offset 1-2, 3-4) |

### Webtoon Mode (Manhwa)
| Key | Action |
| :--- | :--- |
| **Down / MouseWheel** | **Scroll Down** (Seamlessly loads next page) |
| **Up / MouseWheel** | **Scroll Up** |
| **Left / Right** | **Jump to Prev / Next Page** |
| **S** | **Fit Height** (Fit whole page on screen) |
| **D** | **Fit Width** (Zoomed in for scrolling) |

### Global Shortcuts
| Key | Action |
| :--- | :--- |
| **G** | **Go to Page** (Opens input box) |
| **B / E** | Jump to **Beginning / End** |
| **F** | Toggle Fullscreen |
| **H** | Toggle Help Menu |
| **ESC** | Cancel Input / Exit Fullscreen / Quit |

> **Next Volume Navigation:** When you reach the last page of a book, press the **Next Page** key again (Left for Manga, Right for Comic) to open the "End of Volume" prompt. Press it one more time to confirm and load the next file.

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
