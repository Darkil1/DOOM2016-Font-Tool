# DOOM 2016 Font Tool

A fast, standalone font texture converter (`.bimage` ↔ `.png`) for DOOM (2016).

This tool extracts engine-specific font files into editable images with an alpha channel and repacks them back with automatic mipmap generation (Lanczos filter) and Big-Endian header patching.

> [!WARNING]
> This tool is specifically designed, reverse-engineered, and tested **only for font textures**.  
> While DOOM (2016) uses the `.bimage` format for many other types of textures (models, environments, UI, etc.), this tool will **not decode them correctly**.  
> Please use it strictly for font modding.

---

## ⚙️ Requirements

- Windows 10/11 (64-bit)
- Microsoft Visual C++ Redistributable:  
  https://aka.ms/vs/17/release/vc_redist.x64.exe

---

## 🚀 Usage

Simply drag and drop files onto the executable, or pass them as arguments:

- **Unpack:**  
  Drag & drop `.bimage` (or `.bimage;image`) → outputs `.png`

- **Repack:**  
  Drag & drop edited `.png` → outputs game-ready file with `_p` suffix

> If the original `.bimage` is missing, the tool will prompt you to select a fallback header from an internal preset database.

---

## 🛠️ Compiling from Source

The project is written in **C++17** and uses `stb` single-header libraries.

### 1. Clone the repository

```bash
git clone https://github.com/Darkil1/DOOM2016-Font-Tool.git
cd DOOM2016-Font-Tool
```
2. Download required headers and place them in the project folder:

   - `stb_image.h`
   - `stb_image_write.h`
   - `stb_image_resize2.h`

3. Compile:

   ```bash
   g++ main.cpp -o doom_font_tool.exe -std=c++17 -O3
   ```
## 🙏 Credits

If you use this tool or its code, please give credit to the original author.

Created by Darkil (https://github.com/Darkil1)
