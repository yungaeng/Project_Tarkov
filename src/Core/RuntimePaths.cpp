#include "RuntimePaths.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <utility>

namespace {
struct Paths { std::filesystem::path assets, saves, executable; std::string saveError; };
std::filesystem::path EnvironmentPath(const wchar_t* name) {
    const DWORD size=GetEnvironmentVariableW(name,nullptr,0);
    if (!size) return {};
    std::vector<wchar_t> buffer(size);
    const DWORD written=GetEnvironmentVariableW(name,buffer.data(),size);
    if (!written || written>=size) throw std::runtime_error("Cannot read path override");
    std::filesystem::path result(buffer.data());
    if (!result.is_absolute()) throw std::runtime_error("TARKOV path overrides must be absolute");
    return result.lexically_normal();
}
Paths Locate() {
    std::vector<wchar_t> buffer(32768);
    const DWORD size=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
    if (!size || size>=buffer.size()) throw std::runtime_error("Cannot locate executable");
    const auto executable=std::filesystem::path(buffer.data()).parent_path();
    std::filesystem::path repository;
    for(auto candidate=executable; !candidate.empty(); candidate=candidate.parent_path()) {
        if (std::filesystem::exists(candidate/"vcpkg.json") && std::filesystem::is_directory(candidate/"src")) {
            repository=candidate; break;
        }
        if(candidate==candidate.parent_path()) break;
    }
    Paths paths;
    paths.executable=executable;
    paths.assets=EnvironmentPath(L"TARKOV_ASSET_ROOT");
    if(paths.assets.empty()) paths.assets=repository.empty()?executable/"Assets":repository/"Assets";
    if(!std::filesystem::exists(paths.assets/"Shaders"/"cube.vs"))
        throw std::runtime_error("Assets missing: "+paths.assets.u8string());
    paths.saves=EnvironmentPath(L"TARKOV_SAVE_ROOT");
    if(paths.saves.empty()) {
        const auto canonical=executable/"Saves";
        std::vector<std::filesystem::path> candidates{canonical};
        if(!repository.empty()) {
            candidates.push_back(repository/"Saves");
            candidates.push_back(repository/"Project_Tarkov"/"Project_Tarkov"/"Saves");
            candidates.push_back(repository/"Project_Tarkov"/"Saves");
        }
        candidates.push_back(executable/"Saves");
        std::vector<std::filesystem::path> existing;
        for(const auto& candidate:candidates) {
            if(!std::filesystem::exists(candidate/"hideout.txt")) continue;
            bool duplicate=false;
            for(const auto& previous:existing)
                if(std::filesystem::equivalent(previous,candidate)) duplicate=true;
            if(!duplicate) existing.push_back(candidate);
        }
        const bool canonicalExists=std::filesystem::exists(canonical/"hideout.txt");
        if(existing.size()>1 && !canonicalExists) {
            std::string message="Multiple save profiles found; set TARKOV_SAVE_ROOT to the profile directory to keep:";
            for(const auto& candidate:existing) message+="\n"+candidate.u8string();
            paths.saveError=std::move(message);
        }
        // Reuse an old profile in place. Never migrate, merge or overwrite it here.
        paths.saves=canonicalExists||existing.empty()?canonical:existing.front();
    }
    return paths;
}
const Paths& Get() { static const Paths paths=Locate(); return paths; }
}
namespace RuntimePaths {
void Initialize() { (void)AssetRoot(); (void)SaveRoot(); }
const std::filesystem::path& ExecutableRoot() { return Get().executable; }
const std::filesystem::path& AssetRoot() { return Get().assets; }
const std::filesystem::path& SaveRoot() {
    if(!Get().saveError.empty()) throw std::runtime_error(Get().saveError);
    return Get().saves;
}
std::filesystem::path Asset(const std::string& relative) {
    auto path=std::filesystem::u8path(relative);
    if(path.is_absolute()) return path;
    if(path.begin()!=path.end() && *path.begin()=="Assets") path=path.lexically_relative("Assets");
    return AssetRoot()/path;
}
std::filesystem::path Save(const std::string& filename) { return SaveRoot()/std::filesystem::u8path(filename); }
}
