#pragma once
#include <filesystem>
#include <string>

// Resolve paths once without changing the process working directory.
namespace RuntimePaths {
void Initialize();
const std::filesystem::path& ExecutableRoot();
const std::filesystem::path& AssetRoot();
const std::filesystem::path& SaveRoot();
std::filesystem::path Asset(const std::string& relative);
std::filesystem::path Save(const std::string& filename);
}
