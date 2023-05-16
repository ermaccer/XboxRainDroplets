#include "xrd11.h"

void Init()
{
#ifdef DEBUG
    AllocConsole();
    freopen("conin$", "r", stdin);
    freopen("conout$", "w", stdout);
    freopen("conout$", "w", stderr);
    std::setvbuf(stdout, NULL, _IONBF, 0);
#endif

    int thread_param = 42;
    CallbackHandler::CreateThreadAutoClose(nullptr, 0, [](LPVOID data) -> DWORD { 
        DX11Hook::Init();
        return 0;
    }, &thread_param, 0, nullptr);

    WaterDrops::ReadIniSettings();

    DX11Hook::onPresentEvent += [](IDXGISwapChain* pSwapChain)
    {
        WaterDrops::Process(pSwapChain);
        WaterDrops::Render(pSwapChain);
    };

    DX11Hook::onShutdownEvent += []()
    {
        WaterDrops::Shutdown();
    };
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(Init);
    });
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        if (!IsUALPresent()) { InitializeASI(); }
    }
    return TRUE;
}
