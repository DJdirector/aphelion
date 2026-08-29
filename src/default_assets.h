#pragma once

#include <string>

// Built-in default theme/thinker content, embedded directly in the binary so
// a fresh ~/.aphelion/ can be seeded on first run without depending on being
// run from the source tree (see config_paths::ensureConfigLayout). Content
// is byte-for-byte identical to the themes/*.json and thinkers/*.json files
// that used to ship as loose files next to the binary.
namespace default_assets {

inline const std::string kThemeAphelionDark = R"({
  "background":     "#23282b",
  "foreground":     "#d8e2e3",
  "cursor":         "#4bc0c8",
  "accent":         "#3eb4be",
  "alt_background": "#32393d",
  "red":            "#e06c75",
  "green":          "#8bb88a",
  "yellow":         "#e5c07b",
  "orange":         "#e09163",
  "blue":           "#6cb6eb",
  "magenta":        "#c678dd"
}
)";

inline const std::string kThemeTokyoNight = R"({
  "background":     "#1a1b26",
  "foreground":     "#c0caf5",
  "cursor":         "#c0caf5",
  "accent":         "#7aa2f7",
  "alt_background": "#24283b",
  "red":            "#f7768e",
  "green":          "#9ece6a",
  "yellow":         "#e0af68",
  "orange":         "#ff9e64",
  "blue":           "#7aa2f7",
  "magenta":        "#bb9af7"
}
)";

inline const std::string kThemeGruvboxMaterialSoftDark = R"({
  "background":     "#32302f",
  "foreground":     "#d4be98",
  "cursor":         "#d4b398",
  "accent":         "#ea6962",
  "alt_background": "#504945",
  "red":            "#ea6962",
  "green":          "#a9b665",
  "yellow":         "#d8a657",
  "orange":         "#e78a4e",
  "blue":           "#7daea3",
  "magenta":        "#d3869b"
}
)";

inline const std::string kThinkerDefault = R"({
  "speed_ms": 100,
  "frames": [
    "⠋",
    "⠙",
    "⠹",
    "⠸",
    "⠼",
    "⠴",
    "⠦",
    "⠧",
    "⠇",
    "⠏"
  ]
}
)";

inline const std::string kThinkerBar = R"({
  "speed_ms": 300,
  "frames": [
    "[=---]",
    "[-=--]",
    "[--=-]",
    "[---=]",
    "[----]"
  ]
}
)";

inline const std::string kThinkerSpinner = R"({
  "speed_ms": 100,
  "frames": [
    "|",
    "/",
    "-",
    "\\"
  ]
}
)";

inline const std::string kThinkerSwipeRunner = R"({
  "speed_ms": 100,
  "frames": [
    "░░░░░░",
    "░░░░░░",
    "░░░░░░",
    "▓░░░░░",
    "▒▓░░░░",
    "░▒▓░░░",
    "░░▒▓░░",
    "░░░▒▓░",
    "░░░░▒▓",
    "░░░░░▒"
  ]
}
)";

}  // namespace default_assets
