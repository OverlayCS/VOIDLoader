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

// --- Logging ---
static std::ofstream g_log;
static HANDLE g_hConsole = nullptr;

void SetColor(WORD color) {
    if (g_hConsole) SetConsoleTextAttribute(g_hConsole, color);
}

void ResetColor() {
    SetColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void Log(const std::string& msg) {
    if (g_log.is_open())
        g_log << "[INFO] " << msg << std::endl;

    ResetColor();
    printf("[VoidLoader] %s\n", msg.c_str());
}

void LogWarn(const std::string& msg) {
    if (g_log.is_open())
        g_log << "[WARN] " << msg << std::endl;

    SetColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY); // yellow
    printf("[WARN] %s\n", msg.c_str());
    ResetColor();
}

void LogError(const std::string& msg) {
    if (g_log.is_open())
        g_log << "[ERROR] " << msg << std::endl;

    SetColor(FOREGROUND_RED | FOREGROUND_INTENSITY); // red
    printf("[ERROR] %s\n", msg.c_str());
    ResetColor();
}

void LogSuccess(const std::string& msg) {
    if (g_log.is_open())
        g_log << "[OK] " << msg << std::endl;

    SetColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY); // green
    printf("[OK] %s\n", msg.c_str());
    ResetColor();
}

// --- Mono API typedefs ---
typedef void* MonoDomain;
typedef void* MonoAssembly;
typedef void* MonoImage;
typedef void* MonoClass;
typedef void* MonoMethod;
typedef void* MonoObject;

typedef MonoDomain* (*mono_get_root_domain_t)();
typedef void          (*mono_thread_attach_t)(MonoDomain*);
typedef MonoAssembly* (*mono_domain_assembly_open_t)(MonoDomain*, const char*);
typedef MonoImage* (*mono_assembly_get_image_t)(MonoAssembly*);
typedef MonoClass* (*mono_class_from_name_t)(MonoImage*, const char*, const char*);
typedef MonoMethod* (*mono_class_get_method_from_name_t)(MonoClass*, const char*, int);
typedef MonoObject* (*mono_runtime_invoke_t)(MonoMethod*, void*, void**, void**);

struct MonoFuncs {
    mono_get_root_domain_t            get_root_domain;
    mono_thread_attach_t              thread_attach;
    mono_domain_assembly_open_t       assembly_open;
    mono_assembly_get_image_t         assembly_get_image;
    mono_class_from_name_t            class_from_name;
    mono_class_get_method_from_name_t class_get_method;
    mono_runtime_invoke_t             runtime_invoke;
};

// --- Find and load Mono from the game process ---
bool LoadMonoFuncs(MonoFuncs& funcs) {
    static bool dumped = false;
    if (!dumped) {
        dumped = true;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            MODULEENTRY32 me{};
            me.dwSize = sizeof(me);
            if (Module32First(snap, &me)) {
                Log("--- Loaded Modules ---");
                do { Log(std::string("  ") + me.szModule); } while (Module32Next(snap, &me));
                Log("--- End Modules ---");
            }
            CloseHandle(snap);
        }
    }

    const char* monoNames[] = {
        "mono.dll",
        "mono-2.0-bdwgc.dll",
        "mono-2.0.dll",
        "mono-2.0-sgen.dll",
        nullptr
    };

    HMODULE mono = nullptr;
    for (int i = 0; monoNames[i]; i++) {
        mono = GetModuleHandleA(monoNames[i]);
        if (mono) {
            LogSuccess(std::string("Found Mono: ") + monoNames[i]);
            break;
        }
    }

    // Try loading from MonoBleedingEdge folder manually if not found
    if (!mono) {
        char gamePath[MAX_PATH];
        GetModuleFileNameA(nullptr, gamePath, MAX_PATH);
        fs::path gameDir = fs::path(gamePath).parent_path();

        const char* subPaths[] = {
            "MonoBleedingEdge\\EmbedRuntime\\mono-2.0-bdwgc.dll",
            "MonoBleedingEdge\\EmbedRuntime\\mono.dll",
            "_Data\\Mono\\mono.dll",
            nullptr
        };

        for (int i = 0; subPaths[i]; i++) {
            fs::path fullPath = gameDir / subPaths[i];
            if (fs::exists(fullPath)) {
                mono = LoadLibraryA(fullPath.string().c_str());
                if (mono) {
                    LogSuccess("Found Mono via path: " + fullPath.string());
                    break;
                }
            }
        }
    }

    if (!mono) return false;

    funcs.get_root_domain = (mono_get_root_domain_t)GetProcAddress(mono, "mono_get_root_domain");
    funcs.thread_attach = (mono_thread_attach_t)GetProcAddress(mono, "mono_thread_attach");
    funcs.assembly_open = (mono_domain_assembly_open_t)GetProcAddress(mono, "mono_domain_assembly_open");
    funcs.assembly_get_image = (mono_assembly_get_image_t)GetProcAddress(mono, "mono_assembly_get_image");
    funcs.class_from_name = (mono_class_from_name_t)GetProcAddress(mono, "mono_class_from_name");
    funcs.class_get_method = (mono_class_get_method_from_name_t)GetProcAddress(mono, "mono_class_get_method_from_name");
    funcs.runtime_invoke = (mono_runtime_invoke_t)GetProcAddress(mono, "mono_runtime_invoke");

    return funcs.get_root_domain && funcs.thread_attach &&
        funcs.assembly_open && funcs.assembly_get_image &&
        funcs.class_from_name && funcs.class_get_method &&
        funcs.runtime_invoke;
}

// --- Load a single C# plugin assembly ---
void LoadPlugin(MonoFuncs& mono, MonoDomain* domain, const fs::path& dllPath) {
    std::string pathStr = dllPath.string();
    Log("  Opening: " + pathStr);

    // Check for optional .namespace.txt metadata file
    std::string namespaceName = "";
    fs::path nsFile = dllPath;
    nsFile.replace_extension(".namespace.txt");
    if (fs::exists(nsFile)) {
        std::ifstream f(nsFile);
        std::getline(f, namespaceName);
        Log("  Using namespace: '" + namespaceName + "'");
    }

    MonoAssembly* assembly = mono.assembly_open(domain, pathStr.c_str());
    if (!assembly) { LogError("assembly_open failed: " + pathStr); return; }

    MonoImage* image = mono.assembly_get_image(assembly);
    if (!image) { LogError("assembly_get_image failed"); return; }

    MonoClass* klass = mono.class_from_name(image, namespaceName.c_str(), "Plugin");
    if (!klass) { LogError("class 'Plugin' not found in namespace '" + namespaceName + "'"); return; }

    MonoMethod* initMethod = mono.class_get_method(klass, "Init", 0);
    if (!initMethod) { LogError("method 'Init' not found on class 'Plugin'"); return; }

    Log("  Invoking Plugin.Init()...");
    mono.runtime_invoke(initMethod, nullptr, nullptr, nullptr);
    LogSuccess("Plugin.Init() returned: " + dllPath.filename().string());
}

// --- Main loader logic ---
void LoadAllPlugins() {
    // Open log file
    char gamePath[MAX_PATH];
    GetModuleFileNameA(nullptr, gamePath, MAX_PATH);
    fs::path gameDir = fs::path(gamePath).parent_path();
    g_log.open(gameDir / "VoidLoader.log", std::ios::out | std::ios::trunc);

    Log("=== VoidLoader Starting ===");
    Log("Game dir: " + gameDir.string());
    Log("Process: " + std::string(gamePath));

    // Poll for Mono
    MonoFuncs mono{};
    Log("Waiting for Mono...");

    bool found = false;
    for (int i = 0; i < 600; i++) {
        if (LoadMonoFuncs(mono)) {
            LogSuccess("Mono ready after " + std::to_string(i * 100) + "ms");
            found = true;
            break;
        }
        Sleep(100);
    }

    if (!found || !mono.get_root_domain) {
        LogError("Mono not found after 60 seconds — is this a Mono/Unity game?");
        MessageBoxA(nullptr,
            "VoidLoader: Failed to find Mono.\nCheck VoidLoader.log for details.",
            "VoidLoader", MB_ICONERROR);
        return;
    }

    MonoDomain* domain = mono.get_root_domain();
    if (!domain) { LogError("mono_get_root_domain returned null"); return; }
    LogSuccess("Got root domain");

    mono.thread_attach(domain);
    LogSuccess("Thread attached to Mono domain");

    // Wait for Unity to finish initializing before invoking anything
    Log("Waiting 5s for Unity to finish init...");
    Sleep(5000);
    Log("Done waiting — loading plugins");

    // Locate or create Plugins folder
    fs::path pluginsDir = gameDir / "Plugins";
    Log("Plugins dir: " + pluginsDir.string());

    if (!fs::exists(pluginsDir)) {
        LogWarn("Plugins folder not found — creating it");
        fs::create_directory(pluginsDir);
        Log("Drop your mod DLLs in the Plugins folder and relaunch");
        return;
    }

    // Load every .dll in Plugins
    int count = 0;
    for (auto& entry : fs::directory_iterator(pluginsDir)) {
        if (entry.path().extension() == ".dll") {
            Log("Loading: " + entry.path().filename().string());
            LoadPlugin(mono, domain, entry.path());
            count++;
        }
    }

    LogSuccess("=== Done — loaded " + std::to_string(count) + " plugin(s) ===");
    g_log.close();
}

// --- Thread entry point ---
DWORD WINAPI MainThread(LPVOID lpParam) {
    // Spin up console
    AllocConsole();
    g_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    freopen_s(&f, "CONIN$", "r", stdin);

    SetConsoleTitleA("VoidLoader");

    // Print banner
    SetColor(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY); // cyan
    printf("╔═══════════════════════════════╗\n");
    printf("║        VoidLoader v1.0        ║\n");
    printf("╚═══════════════════════════════╝\n");
    ResetColor();

    LoadAllPlugins();
    return 0;
}

// --- DLL entry point ---
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        // Named mutex is system-wide — blocks second process from running too
        HANDLE hMutex = CreateMutexA(nullptr, TRUE, "VoidLoader_SingleInstance");
        if (hMutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
            // Another process already has the loader running
            if (hMutex) CloseHandle(hMutex);
            return TRUE;
        }

        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}