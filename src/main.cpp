#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <stdexcept>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct THEME {
    std::string background;
    std::string foreground;
    std::string cursor;
    std::string accent;
    std::string altBackground;
    std::string red;
    std::string green;
    std::string yellow;
    std::string orange;
    std::string blue;
    std::string magenta;
};

void clearScreen() {
    std::cout << "\033[2J\033[H" << std::flush;
}

// Convert hex into ANSI colored text
void printHex(const std::string& hex, const std::string& text) {
    std::string clean_hex = hex;
    if (!clean_hex.empty() && clean_hex[0] == '#') {
        clean_hex.erase(0, 1);
    }

    // Validate that we have a valid 6-character hex substring left
    if (clean_hex.length() < 6) {
        std::cout << text;
        return;

    }

    // Parse RGB components from hex substring
    int r = std::stoi(clean_hex.substr(0, 2), nullptr, 16);
    int g = std::stoi(clean_hex.substr(2, 2), nullptr, 16);
    int b = std::stoi(clean_hex.substr(4, 2), nullptr, 16);

    // Print using ANSI 24-bit truecolor escape sequence
    std::cout << "\033[38;2;" << r << ";" << g << ";" << b << "m"
              << text
              << "\033[0m"; // Reset color
}

THEME loadTheme(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open theme file at: " + filepath);
    }

    json themeData;
    file >> themeData;

    THEME theme;
    theme.background    = themeData.value("background", "#000000");
    theme.foreground    = themeData.value("foreground", "#ffffff");
    theme.cursor        = themeData.value("cursor", "#ffffff");
    theme.accent        = themeData.value("accent", "#ffffff");
    theme.altBackground = themeData.value("alt_background", "#000000");
    theme.red           = themeData.value("red", "#ff0000");
    theme.green         = themeData.value("green", "#00ff00");
    theme.yellow        = themeData.value("yellow", "#ffff00");
    theme.orange        = themeData.value("orange", "#ffa500");
    theme.blue          = themeData.value("blue", "#0000ff");
    theme.magenta       = themeData.value("magenta", "#ff00ff");

    return theme;
}


int main() {
    try {
        // Read theme file
        THEME currentTheme = loadTheme("themes/theme.json");

        clearScreen();

        // Testing out the parsed colors
        printHex(currentTheme.background, "Background\n");
        printHex(currentTheme.foreground, "Foreground\n");
        printHex(currentTheme.cursor, "Cursor\n");
        printHex(currentTheme.accent, "Accent\n");
        printHex(currentTheme.altBackground, "Alt Background\n");
        printHex(currentTheme.red, "Red\n");
        printHex(currentTheme.green, "Green\n");
        printHex(currentTheme.yellow, "Yellow\n");
        printHex(currentTheme.orange, "Orange\n");
        printHex(currentTheme.blue, "Blue\n");
        printHex(currentTheme.magenta, "Magenta\n");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}



