
#include <Windows.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <tlhelp32.h>

#pragma comment(linker, "/export:GetFileVersionInfoA=C:\\Windows\\System32\\version.GetFileVersionInfoA,@1")
#pragma comment(linker, "/export:GetFileVersionInfoW=C:\\Windows\\System32\\version.GetFileVersionInfoW,@2")
#pragma comment(linker, "/export:GetFileVersionInfoSizeA=C:\\Windows\\System32\\version.GetFileVersionInfoSizeA,@3")
#pragma comment(linker, "/export:GetFileVersionInfoSizeW=C:\\Windows\\System32\\version.GetFileVersionInfoSizeW,@4")
#pragma comment(linker, "/export:VerQueryValueA=C:\\Windows\\System32\\version.VerQueryValueA,@5")
#pragma comment(linker, "/export:VerQueryValueW=C:\\Windows\\System32\\version.VerQueryValueW,@6")

namespace fs = std::filesystem;

static std::ofstream g_log;
static HANDLE g_hConsole = nullptr;

void SetColor(WORD color) {
    if (g_hConsole) SetConsoleTextAttribute(g_hConsole, color);
}
void ResetColor() { SetColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); }

void Log(const std::string& msg) {
    if (g_log.is_open()) g_log << "[INFO] " << msg << std::endl;
    ResetColor();
    printf("[VoidLoader] %s\n", msg.c_str());
}
void LogWarn(const std::string& msg) {
    if (g_log.is_open()) g_log << "[WARN] " << msg << std::endl;
    SetColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    printf("[WARN] %s\n", msg.c_str());
    ResetColor();
}
void LogError(const std::string& msg) {
    if (g_log.is_open()) g_log << "[ERROR] " << msg << std::endl;
    SetColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
    printf("[ERROR] %s\n", msg.c_str());
    ResetColor();
}
void LogSuccess(const std::string& msg) {
    if (g_log.is_open()) g_log << "[OK] " << msg << std::endl;
    SetColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    printf("[OK] %s\n", msg.c_str());
    ResetColor();
}


typedef void* MonoDomain;
typedef void* MonoAssembly;
typedef void* MonoImage;
typedef void* MonoClass;
typedef void* MonoMethod;
typedef void* MonoObject;
typedef void* MonoThread;

typedef MonoDomain* (*mono_jit_init_version_t)(const char*, const char*);
typedef MonoDomain* (*mono_get_root_domain_t)();
typedef MonoThread* (*mono_thread_attach_t)(MonoDomain*);
typedef void          (*mono_thread_detach_t)(MonoThread*);
typedef MonoAssembly* (*mono_domain_assembly_open_t)(MonoDomain*, const char*);
typedef MonoImage* (*mono_assembly_get_image_t)(MonoAssembly*);
typedef MonoClass* (*mono_class_from_name_t)(MonoImage*, const char*, const char*);
typedef MonoMethod* (*mono_class_get_method_from_name_t)(MonoClass*, const char*, int);
typedef MonoObject* (*mono_runtime_invoke_t)(MonoMethod*, void*, void**, void**);
typedef void          (*mono_set_assemblies_path_t)(const char*);
typedef void          (*mono_config_parse_t)(const char*);

struct MonoFuncs {
    mono_jit_init_version_t           jit_init_version;
    mono_get_root_domain_t            get_root_domain;
    mono_thread_attach_t              thread_attach;
    mono_thread_detach_t              thread_detach;
    mono_domain_assembly_open_t       assembly_open;
    mono_assembly_get_image_t         assembly_get_image;
    mono_class_from_name_t            class_from_name;
    mono_class_get_method_from_name_t class_get_method;
    mono_runtime_invoke_t             runtime_invoke;
    mono_set_assemblies_path_t        set_assemblies_path;
    mono_config_parse_t               config_parse;
};

HMODULE FindAndLoadMonoDll(const fs::path& gameDir) {
    const char* monoNames[] = {
        "mono.dll", "mono-2.0-bdwgc.dll",
        "mono-2.0.dll", "mono-2.0-sgen.dll", nullptr
    };
    for (int i = 0; monoNames[i]; i++) {
        HMODULE h = GetModuleHandleA(monoNames[i]);
        if (h) { LogSuccess(std::string("Mono already loaded: ") + monoNames[i]); return h; }
    }
    const char* subPaths[] = {
        "MonoBleedingEdge\\EmbedRuntime\\mono-2.0-bdwgc.dll",
        "MonoBleedingEdge\\EmbedRuntime\\mono.dll",
        "Mono\\EmbedRuntime\\mono-2.0-bdwgc.dll",
        "_Data\\Mono\\mono.dll",
        nullptr
    };
    for (int i = 0; subPaths[i]; i++) {
        fs::path full = gameDir / subPaths[i];
        if (fs::exists(full)) {
            HMODULE h = LoadLibraryA(full.string().c_str());
            if (h) { LogSuccess("Loaded Mono from: " + full.string()); return h; }
        }
    }

    return nullptr;
}

bool GetMonoFuncs(HMODULE monoLib, MonoFuncs& funcs) {
    funcs.jit_init_version = (mono_jit_init_version_t)GetProcAddress(monoLib, "mono_jit_init_version");
    funcs.get_root_domain = (mono_get_root_domain_t)GetProcAddress(monoLib, "mono_get_root_domain");
    funcs.thread_attach = (mono_thread_attach_t)GetProcAddress(monoLib, "mono_thread_attach");
    funcs.thread_detach = (mono_thread_detach_t)GetProcAddress(monoLib, "mono_thread_detach");
    funcs.assembly_open = (mono_domain_assembly_open_t)GetProcAddress(monoLib, "mono_domain_assembly_open");
    funcs.assembly_get_image = (mono_assembly_get_image_t)GetProcAddress(monoLib, "mono_assembly_get_image");
    funcs.class_from_name = (mono_class_from_name_t)GetProcAddress(monoLib, "mono_class_from_name");
    funcs.class_get_method = (mono_class_get_method_from_name_t)GetProcAddress(monoLib, "mono_class_get_method_from_name");
    funcs.runtime_invoke = (mono_runtime_invoke_t)GetProcAddress(monoLib, "mono_runtime_invoke");
    funcs.set_assemblies_path = (mono_set_assemblies_path_t)GetProcAddress(monoLib, "mono_set_assemblies_path");
    funcs.config_parse = (mono_config_parse_t)GetProcAddress(monoLib, "mono_config_parse");

    return funcs.get_root_domain && funcs.thread_attach &&
        funcs.assembly_open && funcs.assembly_get_image &&
        funcs.class_from_name && funcs.class_get_method &&
        funcs.runtime_invoke;
}

enum class GameType { Unknown, Mono, Il2Cpp };

GameType DetectGameType() {
    if (GetModuleHandleA("GameAssembly.dll")) return GameType::Il2Cpp;
    const char* monoNames[] = {
        "mono.dll", "mono-2.0-bdwgc.dll",
        "mono-2.0.dll", "mono-2.0-sgen.dll", nullptr
    };
    for (int i = 0; monoNames[i]; i++)
        if (GetModuleHandleA(monoNames[i])) return GameType::Mono;
    return GameType::Unknown;
}

void LoadPlugin(MonoFuncs& mono, MonoDomain* domain, const fs::path& dllPath) {
    std::string pathStr = dllPath.string();
    Log("  Opening: " + dllPath.filename().string());

    std::string ns = "";
    fs::path nsFile = fs::path(dllPath).replace_extension(".namespace.txt");
    if (fs::exists(nsFile)) {
        std::ifstream f(nsFile);
        std::getline(f, ns);
        Log("  Namespace: '" + ns + "'");
    }

    MonoAssembly* assembly = mono.assembly_open(domain, pathStr.c_str());
    if (!assembly) { LogError("assembly_open failed: " + dllPath.filename().string()); return; }

    MonoImage* image = mono.assembly_get_image(assembly);
    if (!image) { LogError("assembly_get_image failed"); return; }

    MonoClass* klass = mono.class_from_name(image, ns.c_str(), "Plugin");
    if (!klass) { LogError("class 'Plugin' not found in namespace '" + ns + "'"); return; }

    MonoMethod* init = mono.class_get_method(klass, "Init", 0);
    if (!init) { LogError("method 'Init' not found on 'Plugin'"); return; }

    Log("  Calling Plugin.Init()...");
    mono.runtime_invoke(init, nullptr, nullptr, nullptr);
    LogSuccess("Loaded: " + dllPath.filename().string());
}


void LoadAllPlugins() {
    char gamePathBuf[MAX_PATH];
    GetModuleFileNameA(nullptr, gamePathBuf, MAX_PATH);
    fs::path gameDir = fs::path(gamePathBuf).parent_path();

    g_log.open(gameDir / "VoidLoader.log", std::ios::out | std::ios::trunc);
    Log("=== VoidLoader Starting ===");
    Log("Game dir: " + gameDir.string());
    Log("Process:  " + std::string(gamePathBuf));

    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            MODULEENTRY32 me{}; me.dwSize = sizeof(me);
            if (Module32First(snap, &me)) {
                Log("--- Loaded Modules ---");
                do { Log(std::string("  ") + me.szModule); } while (Module32Next(snap, &me));
                Log("--- End Modules ---");
            }
            CloseHandle(snap);
        }
    }

    Log("Detecting runtime...");
    GameType type = GameType::Unknown;
    for (int i = 0; i < 600; i++) {
        type = DetectGameType();
        if (type != GameType::Unknown) {
            LogSuccess("Runtime detected after " + std::to_string(i * 100) + "ms");
            break;
        }
        Sleep(100);
    }

    if (type == GameType::Unknown) {
        LogError("Could not detect Mono or IL2CPP after 60s");
        MessageBoxA(nullptr, "VoidLoader: Unknown runtime.\nCheck VoidLoader.log.", "VoidLoader", MB_ICONERROR);
        return;
    }

    if (type == GameType::Mono)   Log("Runtime: Mono");
    if (type == GameType::Il2Cpp) Log("Runtime: IL2CPP — will use MonoBleedingEdge for mods");

    HMODULE monoLib = FindAndLoadMonoDll(gameDir);
    if (!monoLib) {
        LogError("Could not find Mono DLL");
        return;
    }

    MonoFuncs mono{};
    if (!GetMonoFuncs(monoLib, mono)) {
        LogError("Failed to get Mono function pointers");
        return;
    }
    LogSuccess("Mono functions loaded");

    MonoDomain* domain = nullptr;

    if (type == GameType::Mono) {
        domain = mono.get_root_domain();
        LogSuccess("Got existing Mono root domain");
    }
    else {
        fs::path managedDir = gameDir;
        for (auto& entry : fs::directory_iterator(gameDir)) {
            if (entry.is_directory()) {
                fs::path candidate = entry.path() / "Managed";
                if (fs::exists(candidate)) {
                    managedDir = candidate;
                    Log("Found Managed dir: " + managedDir.string());
                    break;
                }
            }
        }

        if (mono.set_assemblies_path)
            mono.set_assemblies_path(managedDir.string().c_str());

        if (mono.config_parse)
            mono.config_parse(nullptr);

        if (mono.jit_init_version) {
            domain = mono.jit_init_version("VoidLoader", "v4.0.30319");
            LogSuccess("Created new Mono JIT domain for IL2CPP mods");
        }
        else {
            domain = mono.get_root_domain();
        }
    }

    if (!domain) {
        LogError("Mono domain is null");
        return;
    }

    MonoThread* thread = mono.thread_attach(domain);
    LogSuccess("Thread attached to Mono domain");

    Log("Waiting 5s for Unity to finish init...");
    Sleep(5000);

    fs::path pluginsDir = gameDir / "Plugins";
    Log("Plugins dir: " + pluginsDir.string());

    if (!fs::exists(pluginsDir)) {
        LogWarn("Plugins folder not found — creating it");
        fs::create_directory(pluginsDir);
        Log("Drop your mod DLLs in the Plugins folder and relaunch");
        if (thread) mono.thread_detach(thread);
        return;
    }

    int count = 0;
    for (auto& entry : fs::directory_iterator(pluginsDir)) {
        if (entry.path().extension() == ".dll") {
            Log("Loading: " + entry.path().filename().string());
            LoadPlugin(mono, domain, entry.path());
            count++;
        }
    }

    LogSuccess("=== Done — loaded " + std::to_string(count) + " plugin(s) ===");
    if (thread) mono.thread_detach(thread);
    g_log.close();
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    AllocConsole();
    g_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    freopen_s(&f, "CONIN$", "r", stdin);

    SetConsoleTitleA("VoidLoader");

    SetColor(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    printf("╔═══════════════════════════════╗\n");
    printf("║        VoidLoader v1.1        ║\n");
    printf("╚═══════════════════════════════╝\n");
    ResetColor();

    LoadAllPlugins();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        HANDLE hMutex = CreateMutexA(nullptr, TRUE, "VoidLoader_SingleInstance");
        if (hMutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
            if (hMutex) CloseHandle(hMutex);
            return TRUE;
        }

        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}