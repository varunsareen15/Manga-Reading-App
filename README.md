# C Manga Reader

A lightweight, cross-platform manga reader written in C. It reads `.cbz` files directly from memory, supports High-DPI (Retina) displays, and automatically adapts controls for **Manga** (Right-to-Left), **Comics** (Left-to-Right), **Manhua** (Left-to-Right) or **Manhwa/Webtoons** (Vertical Scroll).

Includes a built-in **Komga server integration** with a graphical library browser, page streaming, downloads, and two-way read progress syncing.

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
* **Komga Integration:**
    * Browse your Komga libraries with a graphical cover art grid.
    * Stream pages directly from the server — no downloads required.
    * Download volumes as `.cbz` for offline reading.
    * Two-way read progress sync with Komga (works with Paperback, KMReader, and other Komga-compatible apps).

## Prerequisites

You need a C compiler (`gcc`) and the development headers for **SDL2**, **SDL2_image**, **SDL2_ttf**, **libzip**, **SQLite3**, and **libcurl**.

### macOS (Homebrew)
```bash
brew install sdl2 sdl2_image sdl2_ttf libzip sqlite curl
```
### Linux (Debian/Ubuntu)
```bash
sudo apt-get install build-essential libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libzip-dev libsqlite3-dev libcurl4-openssl-dev
```
### Windows (MSYS2 MinGW 64-bit)
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-SDL2_ttf mingw-w64-x86_64-libzip mingw-w64-x86_64-sqlite3 mingw-w64-x86_64-curl
```

## Library Setup (Local Files)
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

## Komga Server Setup

To connect to a [Komga](https://komga.org/) media server, create a config file:

```bash
mkdir -p ~/.config/manga_reader
```

Create `~/.config/manga_reader/config.ini`:
```ini
[komga]
url = http://<your-server-ip>:25600
api_key = <your-komga-api-key>

[downloads]
path = ./downloads
```

**Getting an API key:** In the Komga web UI, go to your user settings and generate an API key.

**Reading mode detection:** The reader auto-detects the mode from your Komga library names — name them `manga`, `manhwa`, `manhua`, or `comics` to match the correct reading direction.

### Cross-Device Sync

Read progress is synced to the Komga server, so you can pick up where you left off on any device. Compatible Komga clients include:

| Platform | App |
| :--- | :--- |
| **Desktop** | This reader, Komga web UI |
| **iOS** | Paperback, KMReader, Komic |
| **Android** | Mihon (formerly Tachiyomi) |

## File Naming Convention (Auto-Next)

To use the "Next Volume" feature, your files must be named sequentially. The reader sorts files alphabetically.

**Important:** Use **Zero-Padding** for numbers to ensure correct order.

* **Good:** `vol01.cbz`, `vol02.cbz`, ... `vol10.cbz`
* **Bad:** `vol1.cbz`, `vol2.cbz`, ... `vol10.cbz` (Computer sorting places `10` before `2`)

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

### Local File Mode
Pass the path to a `.cbz` file as a command-line argument.

```bash
./manga_reader "library/manga/One Piece/vol1.cbz"
```

### Komga Browser Mode
Run with no arguments (requires config file):

```bash
./manga_reader
```

This opens a graphical library browser with your Komga libraries as tabs.

### Direct Komga Book Mode
Open a specific Komga book by ID:

```bash
./manga_reader --book <komga-book-id>
```

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
| **ESC** | Cancel Input / Exit Fullscreen / Back / Quit |

> **Next Volume Navigation:** When you reach the last page of a book, press the **Next Page** key again (Left for Manga, Right for Comic) to open the "End of Volume" prompt. Press it one more time to confirm and load the next file. In Komga mode, this loads the next book from the series.

### Inside "Go to Page" Mode
| Key | Action |
| :--- | :--- |
| **0-9** | Type Page Number |
| **Enter** | Confirm Jump |
| **Backspace** | Delete Digit |
| **ESC** | Cancel |

### Library Browser Controls
| Key | Action |
| :--- | :--- |
| **Arrow Keys** | Navigate cover grid |
| **Enter / Click** | Open series or read book |
| **D** | Download selected book as `.cbz` |
| **Tab / 1-4** | Switch library tab |
| **PageUp / PageDown** | Paginate results |
| **Backspace / ESC** | Go back / Quit |
| **Mouse Wheel** | Scroll grid |

## Project Structure
```text
manga_reader/
├── Makefile              # Build configuration
├── README.md             # Documentation
├── library.db            # SQLite Database (Auto-generated)
├── font.ttf              # Font file (User provided)
├── library/              # Your local Manga/Comic files
├── vendor/
│   └── cJSON/            # Vendored JSON parser
├── include/              # Header files
│   ├── bookmark_manager.h
│   ├── browser_ui.h
│   ├── cbz_handler.h
│   ├── config.h
│   ├── file_utils.h
│   ├── komga_client.h
│   ├── page_provider.h
│   └── render_engine.h
├── src/                  # Source code
│   ├── main.c            # Entry point and reader loops
│   ├── bookmark_manager.c # SQLite bookmarks + Komga progress sync
│   ├── browser_ui.c      # Komga library browser UI
│   ├── cbz_handler.c     # CBZ/ZIP file handling
│   ├── config.c          # INI config parser
│   ├── file_utils.c      # Local file navigation
│   ├── komga_client.c    # Komga REST API client
│   ├── page_provider.c   # Abstraction: local CBZ or Komga stream
│   └── render_engine.c   # SDL2 rendering engine
└── build/                # Compiled object files
```

## License
MIT License
