/**
 * ============================================================================
 * DOOM 2016 FONT TOOL
 * Author: Darkil
 * Version: 1.0
 * * Description:
 * A command-line utility for modding DOOM (2016) font files.
 * It extracts .bimage (idTech 6 texture format) to editable .png files,
 * and repacks edited .png files back into engine-ready .bimage formats.
 * Features automatic mipmap generation using Lanczos filtering and
 * dynamic Big-Endian header patching.
 * ============================================================================
 */

#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <iomanip>

 // --- STB IMPLEMENTATION ---
 // Single-header libraries used for image processing without heavy dependencies
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

namespace fs = std::filesystem;

// --- CONSTANTS & SETTINGS ---
const int HEADER_SIZE = 62;                  // Standard idTech 6 .bimage header size
const int GAP_SIZE = 20;                     // Padding size between mipmap levels
const std::string APP_VERSION = "1.0";
const std::string SIGNATURE_STR = "Tool by Darkil"; // 14 bytes authorship signature

// --- PRESETS DATABASE ---
// Fallback hex headers for various font types if the original .bimage is missing
std::map<std::string, std::string> PRESETS_DB = {
    {"courier", "57801502074D49420000000000000380000003800000000000000001000000000500000005000000040000000000000000000000038000000380000C4000"},
    {"dflihei_bd", "58262422074D4942000000000000108000001080000000000000000100000000050000000500000004000000000000000000000010800000108001104000"},
    {"eurostileconreg", "578FC25B074D49420000000000000300000003800000000000000001000000000500000005000000040000000000000000000000030000000380000A8000"},
    {"idakagi_semibold", "57801503074D49420000000000000380000003800000000000000001000000000500000005000000040000000000000000000000038000000380000C4000"},
    {"korataki_rg", "57801503074D4942000000000000048000000480000000000000000100000000050000000500000004000000000000000000000004800000048000144000"},
    {"microgrammadbolext", "57801503074D4942000000000000040000000400000000000000000B00000000050000000500000004000000000000000000000004000000040000100000"},
    {"pragmatica_book", "578FC73C074D4942000000000000040000000400000000000000000B00000000050000000500000004000000000000000000000004000000040000100000"},
    {"square721_cn_tl", "56DF3239074D49420000000000000380000004000000000000000001000000000500000005000000040000000000000000000000038000000400000E0000"},
    {"square721_ex_tl", "5786B685074D4942000000000000048000000480000000000000000100000000050000000500000004000000000000000000000004800000048000144000"},
    {"tt_supermolot", "57801505074D4942000000000000040000000400000000000000000B00000000050000000500000004000000000000000000000004000000040000100000"},
    {"tt_supermolot_bold", "580AF35A074D4942000000000000040000000400000000000000000B00000000050000000500000004000000000000000000000004000000040000100000"},
    {"tt_supermolot_light", "57801505074D49420000000000000380000003800000000000000001000000000500000005000000040000000000000000000000038000000380000C4000"},
    {"tt_supermolot_thin", "5809A1B7074D49420000000000000380000004000000000000000001000000000500000005000000040000000000000000000000038000000400000E0000"},
    {"tt_supermolot_thin_mono", "57801505074D49420000000000000380000004000000000000000001000000000500000005000000040000000000000000000000038000000400000E0000"},
    {"uniwars_rg", "57801505074D4942000000000000040000000480000000000000000100000000050000000500000004000000000000000000000004000000048000120000"},
    {"v7gothic_medium_p", "582607C4074D49420000000000000D8000000E0000000000000000010000000005000000050000000400000000000000000000000D8000000E0000BD0000"},
    {"vd_logog_medium_p", "582605FF074D49420000000000000B8000000C0000000000000000010000000005000000050000000400000000000000000000000B8000000C00008A0000"},
    {"venacti_rg", "57801505074D49420000000000000380000003800000000000000001000000000500000005000000040000000000000000000000038000000380000C4000"},
    {"zero_threes", "57801506074D49420000000000000380000003800000000000000001000000000500000005000000040000000000000000000000038000000380000C4000"}
};

// --- COLORS ---
// ANSI escape codes for terminal formatting
namespace Color {
    const std::string HEADER = "\033[95m";
    const std::string BLUE = "\033[94m";
    const std::string CYAN = "\033[96m";
    const std::string GREEN = "\033[92m";
    const std::string WARNING = "\033[93m";
    const std::string FAIL = "\033[91m";
    const std::string ENDC = "\033[0m";
    const std::string BOLD = "\033[1m";
}

struct Stats {
    int success = 0;
    int errors = 0;
} stats;

// --- UTILS (Platform & Bytes) ---

// Enable Colors on Windows Terminal
#ifdef _WIN32
#include <windows.h>
void enable_ansi() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
    SetConsoleMode(hOut, dwMode);
}
void cls() { system("cls"); }
#else
void enable_ansi() {}
void cls() { system("clear"); }
#endif

// idTech engines generally use Big-Endian byte order for binary assets.
// These helpers safely read/write memory bypassing architecture differences.

uint32_t read_be_uint32(const uint8_t* buffer) {
    return (uint32_t(buffer[0]) << 24) |
        (uint32_t(buffer[1]) << 16) |
        (uint32_t(buffer[2]) << 8) |
        (uint32_t(buffer[3]));
}

void write_be_uint32(std::vector<uint8_t>& buf, size_t offset, uint32_t val) {
    buf[offset + 0] = (val >> 24) & 0xFF;
    buf[offset + 1] = (val >> 16) & 0xFF;
    buf[offset + 2] = (val >> 8) & 0xFF;
    buf[offset + 3] = (val) & 0xFF;
}

void write_be_uint16(std::vector<uint8_t>& buf, size_t offset, uint16_t val) {
    buf[offset + 0] = (val >> 8) & 0xFF;
    buf[offset + 1] = (val) & 0xFF;
}

std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = (uint8_t)strtol(byteString.c_str(), nullptr, 16);
        bytes.push_back(byte);
    }
    return bytes;
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool ends_with(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

// --- FORWARD DECLARATIONS ---
void unpack(const fs::path& path);
void try_repack(const fs::path& path);
void process_item(const std::string& path_str);
void scan_directory(const fs::path& folder_path);
void process_single_file(const fs::path& path);
void pack_with_generated_mips(const fs::path& png_path, const std::string& original_path, const std::string& header_hex);

// --- FUNCTIONS ---

/**
 * UNPACK
 * Reads a DOOM .bimage file, strips the header, and extracts the raw pixel data.
 * The raw format is single-channel grayscale (L). This function converts it to
 * RGBA by placing the raw data into the Alpha channel and filling RGB with white,
 * making it easily editable in Photoshop/GIMP.
 */
void unpack(const fs::path& bimage_path) {
    std::string filename = bimage_path.filename().string();
    std::string clean_name = filename;

    // Remove engine-specific suffix artifacts
    size_t pos;
    while ((pos = clean_name.find(";image")) != std::string::npos) clean_name.replace(pos, 6, "");
    while ((pos = clean_name.find(";type=Unknown")) != std::string::npos) clean_name.replace(pos, 13, "");

    fs::path output_dir = bimage_path.parent_path();
    fs::path png_out_path = output_dir / (clean_name + ".png");

    if (fs::exists(png_out_path)) return;

    std::cout << "?? Unpacking: " << Color::BOLD << filename << Color::ENDC << "\n";

    try {
        std::ifstream f(bimage_path, std::ios::binary);
        if (!f) throw std::runtime_error("File not found");

        std::vector<uint8_t> header(HEADER_SIZE);
        f.read(reinterpret_cast<char*>(header.data()), HEADER_SIZE);

        // Fetch resolution from header bytes 12-19
        uint32_t width = read_be_uint32(&header[12]);
        uint32_t height = read_be_uint32(&header[16]);

        if (width == 0 || width > 16384) {
            std::cout << Color::FAIL << "Invalid dimensions." << Color::ENDC << "\n";
            stats.errors++;
            return;
        }

        std::vector<uint8_t> pixel_data(width * height);
        f.read(reinterpret_cast<char*>(pixel_data.data()), width * height);

        // Convert L (Grayscale/Alpha) to RGBA (White RGB + Alpha Mask)
        std::vector<uint8_t> rgba_data(width * height * 4);
        for (size_t i = 0; i < pixel_data.size(); ++i) {
            rgba_data[i * 4 + 0] = 255;
            rgba_data[i * 4 + 1] = 255;
            rgba_data[i * 4 + 2] = 255;
            rgba_data[i * 4 + 3] = pixel_data[i];
        }

        if (stbi_write_png(png_out_path.string().c_str(), width, height, 4, rgba_data.data(), width * 4)) {
            std::cout << Color::GREEN << "PNG Created: " << png_out_path.filename().string() << Color::ENDC << "\n";
            stats.success++;
        }
    }
    catch (const std::exception& e) {
        std::cout << Color::FAIL << "Error: " << e.what() << Color::ENDC << "\n";
        stats.errors++;
    }
}

/**
 * PRESET SELECTOR
 * Prompts the user to select a fallback header if the original .bimage
 * file is missing, attempting to match the font name automatically.
 */
std::string select_preset_menu(const std::string& filename) {
    std::cout << "\n" << Color::WARNING << "Original file not found for: " << filename << Color::ENDC << "\n";
    std::cout << Color::CYAN << "Select target font type to generate a valid header:" << Color::ENDC << "\n";

    std::vector<std::string> keys;
    for (auto const& [key, val] : PRESETS_DB) keys.push_back(key);

    std::string clean_fname = to_lower(filename);
    size_t ext_pos = clean_fname.find(".png");
    if (ext_pos != std::string::npos) clean_fname.erase(ext_pos, 4);

    for (size_t i = 0; i < keys.size(); ++i) {
        std::string prefix = "   ";
        if (clean_fname.find(keys[i]) != std::string::npos) {
            prefix = "?? "; // Highlight likely matches
        }
        std::cout << prefix << (i + 1) << ". " << keys[i] << "\n";
    }

    std::cout << "\n(Enter number, or '0' to skip)\n";
    while (true) {
        std::cout << ">>> ";
        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "0") return "";
        try {
            int idx = std::stoi(choice) - 1;
            if (idx >= 0 && idx < (int)keys.size()) {
                std::cout << "Selected: " << Color::BOLD << keys[idx] << Color::ENDC << "\n";
                return PRESETS_DB[keys[idx]];
            }
        }
        catch (...) {}
        std::cout << "Invalid selection.\n";
    }
}

/**
 * TRY REPACK
 * Determines the best header source (original file or preset DB)
 * before passing the image down the pipeline for packing.
 */
void try_repack(const fs::path& png_path) {
    std::string base = png_path.string();
    size_t ext_pos = base.find(".png");
    if (ext_pos != std::string::npos) base.erase(ext_pos, 4);

    std::vector<std::string> candidates = { base, base + ";image", base + ".bimage" };
    std::string original_path = "";

    // Exact match search
    for (const auto& c : candidates) {
        if (fs::exists(c)) {
            original_path = c;
            break;
        }
    }

    // Fuzzy search in directory
    if (original_path.empty()) {
        fs::path dir_name = png_path.parent_path();
        std::string base_name = png_path.stem().string();

        try {
            for (const auto& entry : fs::directory_iterator(dir_name)) {
                std::string f_name = entry.path().filename().string();
                std::string f_lower = to_lower(f_name);
                std::string base_lower = to_lower(base_name);

                if (f_lower.find(base_lower) != std::string::npos &&
                    f_lower.find(".bimage") != std::string::npos &&
                    f_lower.find(".png") == std::string::npos &&
                    !ends_with(f_lower, "_p") &&
                    !ends_with(f_lower, ".bak")) {
                    original_path = entry.path().string();
                    break;
                }
            }
        }
        catch (...) {}
    }

    if (!original_path.empty()) {
        pack_with_generated_mips(png_path, original_path, "");
    }
    else {
        std::string preset = select_preset_menu(png_path.filename().string());
        if (!preset.empty()) {
            pack_with_generated_mips(png_path, "", preset);
        }
        else {
            std::cout << Color::WARNING << "Skipped" << Color::ENDC << "\n";
        }
    }
}

/**
 * PACK WITH GENERATED MIPS
 * The core packing logic.
 * 1. Modifies the header with new dimensions.
 * 2. Extracts Alpha channel.
 * 3. Generates Mipmaps down to 1x1 using stb_image_resize2 (Lanczos filter).
 * 4. Injects authorship signature seamlessly into binary structure.
 */
void pack_with_generated_mips(const fs::path& png_path, const std::string& original_path, const std::string& header_hex) {
    std::cout << "?? Processing: " << Color::BOLD << png_path.filename().string() << Color::ENDC << "\n";

    unsigned char* img_data = nullptr;
    try {
        int w, h, ch;
        // Force 4 channels to reliably fetch Alpha at index 3
        img_data = stbi_load(png_path.string().c_str(), &w, &h, &ch, 4);
        if (!img_data) throw std::runtime_error("Failed to load PNG");

        std::vector<uint8_t> header_bytes;

        // --- A. HEADER PREPARATION ---
        if (!original_path.empty()) {
            std::ifstream f(original_path, std::ios::binary);
            header_bytes.resize(HEADER_SIZE);
            f.read(reinterpret_cast<char*>(header_bytes.data()), HEADER_SIZE);
        }
        else {
            header_bytes = hex_to_bytes(header_hex);
            std::cout << Color::BLUE << "Generated Header from Preset" << Color::ENDC << "\n";
        }

        // Overwrite dimensions and stride in Big Endian format
        write_be_uint32(header_bytes, 12, w);
        write_be_uint32(header_bytes, 16, h);
        write_be_uint16(header_bytes, 52, w);
        write_be_uint16(header_bytes, 56, h);
        uint16_t stride = w / 64;
        write_be_uint16(header_bytes, 58, stride);

        // Update mipmap count if needed
        if (header_bytes[27] > 1) {
            int mip_count = (int)std::log2(std::max(w, h)) + 1;
            header_bytes[27] = (uint8_t)mip_count;
        }

        // --- B. DATA & MIPMAP GENERATION ---
        // Font format uses only 1 channel (Alpha mask). 
        std::vector<uint8_t> current_img(w * h);
        for (int i = 0; i < w * h; i++) {
            current_img[i] = img_data[i * 4 + 3];
        }

        int current_w = w;
        int current_h = h;
        std::vector<uint8_t> new_payload;
        bool signature_injected = false;
        bool use_mips = header_bytes[27] > 1;

        while (true) {
            new_payload.insert(new_payload.end(), current_img.begin(), current_img.end());

            if (!use_mips) break;
            if (current_w == 1 && current_h == 1) break;

            // Inject signature padding between mipmaps for binary watermarking
            if (current_w > 4) {
                std::vector<uint8_t> padding(GAP_SIZE, 0);
                if (SIGNATURE_STR.size() <= GAP_SIZE) {
                    for (size_t i = 0; i < SIGNATURE_STR.size(); i++) padding[i] = SIGNATURE_STR[i];
                    signature_injected = true;
                }
                new_payload.insert(new_payload.end(), padding.begin(), padding.end());
            }

            int next_w = std::max(1, current_w / 2);
            int next_h = std::max(1, current_h / 2);
            std::vector<uint8_t> next_img(next_w * next_h);

            // High-quality downscaling using STBIR_1CHANNEL logic
            stbir_resize_uint8_linear(current_img.data(), current_w, current_h, 0,
                next_img.data(), next_w, next_h, 0,
                STBIR_1CHANNEL);

            current_img = next_img;
            current_w = next_w;
            current_h = next_h;
        }

        // Fallback signature injection at EOF
        if (!signature_injected) {
            bool ends_with_sig = false;
            if (new_payload.size() >= SIGNATURE_STR.size()) {
                std::string tail(new_payload.end() - SIGNATURE_STR.size(), new_payload.end());
                if (tail == SIGNATURE_STR) ends_with_sig = true;
            }
            if (!ends_with_sig) {
                for (char c : SIGNATURE_STR) new_payload.push_back(c);
            }
        }

        // --- C. FILE OUTPUT ---
        fs::path dir_name = png_path.parent_path();
        std::string new_name;

        // Ensure processed files get a _p suffix to prevent infinite loops
        if (!original_path.empty()) {
            std::string orig_name = fs::path(original_path).filename().string();
            if (!ends_with(orig_name, "_p")) new_name = orig_name + "_p";
            else new_name = orig_name;
        }
        else {
            new_name = "64_df.tga$borderclamp$alpha.bimage;image";
        }

        fs::path out_path = dir_name / new_name;

        // Auto-backup original output file
        if (fs::exists(out_path)) {
            fs::path bak_path = out_path.string() + ".bak";
            if (!fs::exists(bak_path)) {
                try { fs::copy_file(out_path, bak_path); }
                catch (...) {}
            }
        }

        std::ofstream outfile(out_path, std::ios::binary);
        outfile.write(reinterpret_cast<char*>(header_bytes.data()), header_bytes.size());
        outfile.write(reinterpret_cast<char*>(new_payload.data()), new_payload.size());

        std::cout << Color::GREEN << "   ? SUCCESS -> " << new_name << Color::ENDC << "\n";
        stats.success++;

    }
    catch (const std::exception& e) {
        std::cout << Color::FAIL << "   ? Error: " << e.what() << Color::ENDC << "\n";
        stats.errors++;
    }

    if (img_data) stbi_image_free(img_data);
}

// --- SYSTEM HANDLERS ---

void process_single_file(const fs::path& path) {
    std::string name = to_lower(path.filename().string());
    if (name.find(".bimage") != std::string::npos && name.find(".png") == std::string::npos) {
        unpack(path);
    }
    else if (name.find(".png") != std::string::npos) {
        try_repack(path);
    }
}

void scan_directory(const fs::path& folder_path) {
    std::cout << Color::CYAN << "?? Scanning folder: " << folder_path.string() << Color::ENDC << "\n";
    try {
        for (const auto& entry : fs::recursive_directory_iterator(folder_path)) {
            std::string file = entry.path().filename().string();
            // Skip backups and already patched files
            if (ends_with(file, "_p") || ends_with(file, ".bak")) continue;

            std::string name = to_lower(file);
            if ((name.find(".bimage") != std::string::npos && name.find(".png") == std::string::npos) ||
                name.find(".png") != std::string::npos) {
                process_single_file(entry.path());
            }
        }
    }
    catch (...) {}
}

void process_item(const std::string& path_str) {
    fs::path p(path_str);
    if (!fs::exists(p)) return;

    if (fs::is_directory(p)) scan_directory(p);
    else process_single_file(p);
}

// --- ENTRY POINT ---
int main(int argc, char* argv[]) {
    enable_ansi();
    cls();

    std::cout << Color::HEADER << "==============================================\n";
    std::cout << "   DOOM 2016 FONT TOOL v" << APP_VERSION << "\n";
    std::cout << "   Created by Darkil \n";
    std::cout << "==============================================" << Color::ENDC << "\n";
    std::cout << "Drag and drop files or folders here (.bimage / .png)\n";

    // Support drag and drop multiple items directly to the executable
    if (argc > 1) {
        std::cout << "----------------------------------------\n";
        for (int i = 1; i < argc; i++) {
            process_item(argv[i]);
        }
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
    }
    // Interactive terminal mode
    else {
        while (true) {
            std::cout << "----------------------------------------\n";
            std::cout << ">>> ";
            std::string input;
            std::getline(std::cin, input);

            // Strip quotes from paths drag&dropped into terminal window
            if (input.size() >= 2 && input.front() == '"' && input.back() == '"') {
                input = input.substr(1, input.size() - 2);
            }

            if (input == "exit" || input == "quit" || input == "q") break;
            if (input.empty()) continue;

            stats = { 0, 0 };
            process_item(input);

            std::cout << "\n" << "==============================" << "\n";
            std::cout << "?? SUMMARY:  " << Color::GREEN << "OK: " << stats.success << Color::ENDC
                << "  |  " << Color::FAIL << "Errors: " << stats.errors << Color::ENDC << "\n";
            std::cout << "==============================" << "\n";
        }
    }
    return 0;
}
