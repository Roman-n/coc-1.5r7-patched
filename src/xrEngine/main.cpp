// Entry point is in xr_3da/entry_point.cpp
#include "stdafx.h"
#include "main.h"

#include <process.h>
#include <locale.h>

#include "IGame_Persistent.h"
#include "xr_input.h"
#include "XR_IOConsole.h"
#include "x_ray.h"
#include "std_classes.h"
#include "splash.h"
#include "LightAnimLibrary.h"
#include "xrCDB/ISpatial.h"
#include "Text_Console.h"
#include "xrSASH.h"
#include "xr_ioc_cmd.h"

#ifdef MASTER_GOLD
#define NO_MULTI_INSTANCES
#endif

// global variables
ENGINE_API CInifile* pGameIni = nullptr;
ENGINE_API bool g_bBenchmark = false;
string512 g_sBenchmarkName;
ENGINE_API string512 g_sLaunchOnExit_params;
ENGINE_API string512 g_sLaunchOnExit_app;
ENGINE_API string_path g_sLaunchWorkingFolder;

namespace
{
bool CheckBenchmark();
void RunBenchmark(pcstr name);
} // namespace

ENGINE_API void InitEngine()
{
    Engine.Initialize();
    Device.Initialize();
}

ENGINE_API void InitSettings()
{
    string_path fname;
    FS.update_path(fname, "$game_config$", "system.ltx");
#ifdef DEBUG
    Msg("Updated path to system.ltx is %s", fname);
#endif
    pSettings = new CInifile(fname, TRUE);
    CHECK_OR_EXIT(pSettings->section_count(),
        make_string("Cannot find file %s.\nReinstalling application may fix this problem.", fname));

    FS.update_path(fname, "$game_config$", "game.ltx");
    pGameIni = new CInifile(fname, TRUE);
    CHECK_OR_EXIT(pGameIni->section_count(),
        make_string("Cannot find file %s.\nReinstalling application may fix this problem.", fname));
}

ENGINE_API void InitConsole()
{
    if (GEnv.isDedicatedServer)
        Console = new CTextConsole();
    else
        Console = new CConsole();

    Console->Initialize();
    xr_strcpy(Console->ConfigFile, "user.ltx");
    if (strstr(Core.Params, "-ltx "))
    {
        string64 c_name;
        sscanf(strstr(Core.Params, "-ltx ") + strlen("-ltx "), "%[^ ] ", c_name);
        xr_strcpy(Console->ConfigFile, c_name);
    }
}

ENGINE_API void InitInput()
{
    bool captureInput = !strstr(Core.Params, "-i");
    pInput = new CInput(captureInput);
}

ENGINE_API void destroyInput() { xr_delete(pInput); }
ENGINE_API void InitSound() { ISoundManager::_create(); }
ENGINE_API void destroySound() { ISoundManager::_destroy(); }
ENGINE_API void destroySettings()
{
    auto s = const_cast<CInifile**>(&pSettings);
    xr_delete(*s);
    xr_delete(pGameIni);
}

ENGINE_API void destroyConsole()
{
    Console->Execute("cfg_save");
    Console->Destroy();
    xr_delete(Console);
}

ENGINE_API void destroyEngine()
{
    Device.Destroy();
    Engine.Destroy();
}

void execUserScript()
{
    Console->Execute("default_controls");
    Console->ExecuteScript(Console->ConfigFile);
}

void slowdownthread(void*)
{
    for (;;)
    {
        if (Device.GetStats().fFPS < 30)
            Sleep(1);
        if (Device.mt_bMustExit || !pSettings || !Console || !pInput || !pApp)
            return;
    }
}

ENGINE_API void Startup()
{
    execUserScript();
    InitSound();

    // ...command line for auto start
    pcstr startArgs = strstr(Core.Params, "-start ");
    if (startArgs)
        Console->Execute(startArgs + 1);
    pcstr loadArgs = strstr(Core.Params, "-load ");
    if (loadArgs)
        Console->Execute(loadArgs + 1);

    // Initialize APP
    Device.Create();
    LALib.OnCreate();
    pApp = new CApplication();
    g_pGamePersistent = dynamic_cast<IGame_Persistent*>(NEW_INSTANCE(CLSID_GAME_PERSISTANT));
    R_ASSERT(g_pGamePersistent);
    g_SpatialSpace = new ISpatial_DB("Spatial obj");
    g_SpatialSpacePhysic = new ISpatial_DB("Spatial phys");

    // Main cycle
    Device.Run();
    // Destroy APP
    xr_delete(g_SpatialSpacePhysic);
    xr_delete(g_SpatialSpace);
    DEL_INSTANCE(g_pGamePersistent);
    xr_delete(pApp);
    Engine.Event.Dump();
    // Destroying
    destroyInput();
    if (!g_bBenchmark && !g_SASH.IsRunning())
        destroySettings();
    LALib.OnDestroy();
    if (!g_bBenchmark && !g_SASH.IsRunning())
        destroyConsole();
    else
        Console->Destroy();
    destroyEngine();
    destroySound();
}

ENGINE_API int RunApplication()
{
    R_ASSERT2(Core.Params, "Core must be initialized");

#ifdef NO_MULTI_INSTANCES
    if (!GEnv.isDedicatedServer)
    {
        CreateMutex(nullptr, TRUE, "Local\\STALKER-COP");
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            splash::hide();
            MessageBox(nullptr, "The game has already been launched!", nullptr, MB_ICONERROR | MB_OK);
            return 2;
        }
        
    }
#endif
    *g_sLaunchOnExit_app = 0;
    *g_sLaunchOnExit_params = 0;

    InitSettings();
    // Adjust player & computer name for Asian
    if (pSettings->line_exist("string_table", "no_native_input"))
    {
        xr_strcpy(Core.UserName, sizeof(Core.UserName), "Player");
        xr_strcpy(Core.CompName, sizeof(Core.CompName), "Computer");
    }

    FPU::m24r();
    InitEngine();
    InitInput();
    InitConsole();
    Engine.External.CreateRendererList();

    if (CheckBenchmark())
        return 0;

    if (strstr(Core.Params, "-r4"))
        Console->Execute("renderer renderer_r4");
    else if (strstr(Core.Params, "-r3"))
        Console->Execute("renderer renderer_r3");
    else if (strstr(Core.Params, "-r2.5"))
        Console->Execute("renderer renderer_r2.5");
    else if (strstr(Core.Params, "-r2a"))
        Console->Execute("renderer renderer_r2a");
    else if (strstr(Core.Params, "-r2"))
        Console->Execute("renderer renderer_r2");
    else if (strstr(Core.Params, "-r1"))
        Console->Execute("renderer renderer_r1");
    else
    {
        CCC_LoadCFG_custom cmd("renderer ");
        cmd.Execute(Console->ConfigFile);
    }

    Engine.External.Initialize();
    Startup();
    // check for need to execute something external
    if (/*xr_strlen(g_sLaunchOnExit_params) && */ xr_strlen(g_sLaunchOnExit_app))
    {
        // CreateProcess need to return results to next two structures
        STARTUPINFO si = {};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {};
        // We use CreateProcess to setup working folder
        pcstr tempDir = xr_strlen(g_sLaunchWorkingFolder) ? g_sLaunchWorkingFolder : nullptr;
        CreateProcess(
            g_sLaunchOnExit_app, g_sLaunchOnExit_params, nullptr, nullptr, FALSE, 0, nullptr, tempDir, &si, &pi);
    }
    return 0;
}

namespace
{
bool CheckBenchmark()
{
    pcstr benchName = "-batch_benchmark ";
    if (strstr(Core.Params, benchName))
    {
        const u32 sz = xr_strlen(benchName);
        string64 benchmarkName;
        sscanf(strstr(Core.Params, benchName) + sz, "%[^ ] ", benchmarkName);
        RunBenchmark(benchmarkName);
        return true;
    }

    pcstr sashName = "-openautomate ";
    if (strstr(Core.Params, sashName))
    {
        const u32 sz = xr_strlen(sashName);
        string512 sashArg;
        sscanf(strstr(Core.Params, sashName) + sz, "%[^ ] ", sashArg);
        g_SASH.Init(sashArg);
        g_SASH.MainLoop();
        return true;
    }

    return false;
}
void RunBenchmark(pcstr name)
{
    g_bBenchmark = true;
    string_path cfgPath;
    FS.update_path(cfgPath, "$app_data_root$", name);
    CInifile ini(cfgPath);
    const u32 benchmarkCount = ini.line_count("benchmark");
    for (u32 i = 0; i < benchmarkCount; i++)
    {
        LPCSTR benchmarkName, t;
        ini.r_line("benchmark", i, &benchmarkName, &t);
        xr_strcpy(g_sBenchmarkName, benchmarkName);
        shared_str benchmarkCommand = ini.r_string_wb("benchmark", benchmarkName);
        u32 cmdSize = benchmarkCommand.size() + 1;
        Core.Params = (char*)xr_realloc(Core.Params, cmdSize);
        xr_strcpy(Core.Params, cmdSize, benchmarkCommand.c_str());
        xr_strlwr(Core.Params);
        InitInput();
        if (i)
            InitEngine();
        Engine.External.Initialize();
        xr_strcpy(Console->ConfigFile, "user.ltx");
        if (strstr(Core.Params, "-ltx "))
        {
            string64 cfgName;
            sscanf(strstr(Core.Params, "-ltx ") + strlen("-ltx "), "%[^ ] ", cfgName);
            xr_strcpy(Console->ConfigFile, cfgName);
        }
        Startup();
    }
}
} // namespace
