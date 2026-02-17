#pragma once
#include <string>
#include <vector>

// Load themes from themes.toml and apply by name (identifier).
// Returns true if the theme was found and applied, false otherwise (caller can fall back to default).
bool applyThemeFromToml(const std::string& themeName);

// Apply ImHex design language (layout, spacing, rounding, sizes only). Does not touch colors.
// Call after applying a theme so colors come from the theme and layout always matches ImHex.
void applyImHexDesignLanguage();

// Path to themes.toml (REVER_PROJECT_DIR, bundle Resources, or next to executable).
std::string getThemesTomlPath();

// All theme names from themes.toml (in order). Empty if file missing or unreadable.
std::vector<std::string> getThemeNamesFromToml();
