## 🪟 Easy Windows Support

This fork includes a **precompiled Windows version** of `jpegqs`, along with drag-and-drop `.bat` scripts for easy use.

### 📦 Files Added

| File                        | Purpose                                 |
|-----------------------------|------------------------------------------|
| `jpegqs.exe`                | Precompiled JPEG smoothing tool (Windows) |
| `tools/jpegqs_dragdrop.bat`| Drag-and-drop JPEG smoother script       |

---

### 🧰 How to Use (Step-by-Step)

#### ▶ 1. Download

- Clone this repo or download it as ZIP:
https://github.com/SoulByte07/jpeg-quantsmooth


- Or download the compiled `.exe` and `.bat` scripts from [Releases](https://github.com/SoulByte07/jpeg-quantsmooth/releases) *(if available)*

---

#### ▶ 2. Smooth + Enhance JPG Images

**To denoise/upscale JPEGs using `jpegqs`:**

1. Go to the `tools/` folder
2. Drag any `.jpg` or `.jpeg` file onto `jpegqs_dragdrop.bat`
3. It will generate a new image in the same folder named:  original_clean.png

> ℹ️ This uses a patched version of `jpeg2png` that preserves quality and removes junk metadata.

---

### 🧪 Example

If you drag:

C:\Users\You\Pictures\wallpaper.jpg

onto `jpegqs_dragdrop.bat`, you'll get:

C:\Users\You\Pictures\wallpaper_upscale.jpg

Then drag that onto `jpeg2png_dragdrop.bat` and you’ll get:

C:\Users\You\Pictures\wallpaper_upscale_clean.png

---

### 📁 File Tree Example

jpeg-quantsmooth/
├── jpegqs.exe
├── tools/
│ ├── jpegqs_dragdrop.bat
│ └── jpeg2png_dragdrop.bat
├── README.md
└── ... original source files

---

### 🧠 Notes

- These scripts are designed for Windows 10/11 users who prefer not to use the command line.
- The `.exe` was built using MSYS2 (`UCRT64`) with `libjpeg-turbo`.

---

Let us know via issues or pull requests if you'd like GUI support or want to suggest improvements!
