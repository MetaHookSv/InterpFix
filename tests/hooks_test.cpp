#include "interpolation.h"
#include "privatehook.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

metahook_api_t* g_pMetaHookAPI = nullptr;

namespace
{
using LookupFn                              = decltype(&interpfix::FindInterpolationUpdates);
int                           failures      = 0;
int                           installs      = 0;
int                           resolves      = 0;
int                           failInstall   = 0;
const char*                   missingSymbol = nullptr;
char                          error[1024]   = {};
std::vector<uintptr_t>        removed;
LookupFn                      hookedLookup = nullptr;
interpfix::InterpolateModelFn hookedModel  = nullptr;

void Check(const char* label, int expected, int actual)
{
    if (expected != actual)
    {
        std::printf("FAIL %s: expected %d, got %d\n", label, expected, actual);
        ++failures;
    }
}

qboolean __cdecl OriginalLookup(cl_entity_t*, float, position_history_t**, position_history_t**, int*)
{
    return true;
}
int __cdecl OriginalModel(cl_entity_t* ent)
{
    position_history_t* newer;
    position_history_t* older;
    hookedLookup(ent, 9, &newer, &older, nullptr);
    return 9 - older->animtime < 0.0f ? 0 : 1;
}
PVOID                  EngineBase() { return reinterpret_cast<void*>(0x10000000); }
DWORD                  EngineBuild() { return 10257; }
mh_gamesymbol_status_t CRC64(PVOID, uint64_t* value)
{
    *value = 0x1234;
    return MH_GAMESYMBOL_OK;
}
const char* Status(mh_gamesymbol_status_t) { return "SYMBOL_NOT_FOUND"; }
void        SysError(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    std::vsnprintf(error, sizeof(error), format, args);
    va_end(args);
}
mh_gamesymbol_status_t Resolve(PVOID base, const char* symbol, mh_gamesymbol_kind_t kind, PVOID* address)
{
    ++resolves;
    Check("resolve real engine base", 1, base == EngineBase());
    Check("resolve function kind", MH_GAMESYMBOL_KIND_FUNCTION, kind);
    if (missingSymbol && std::strcmp(symbol, missingSymbol) == 0)
        return MH_GAMESYMBOL_SYMBOL_NOT_FOUND;
    *address = std::strcmp(symbol, "CL_FindInterpolationUpdates") == 0 ?
        reinterpret_cast<void*>(OriginalLookup) :
        reinterpret_cast<void*>(OriginalModel);
    return MH_GAMESYMBOL_OK;
}
hook_t* InlineHook(void* address, void* replacement, void** original)
{
    ++installs;
    Check("resolve both before any installation", 2, resolves);
    if (failInstall == installs)
        return nullptr;
    *original = address;
    if (installs == 1)
        hookedLookup = reinterpret_cast<LookupFn>(replacement);
    else
        hookedModel = reinterpret_cast<interpfix::InterpolateModelFn>(replacement);
    return reinterpret_cast<hook_t*>(static_cast<uintptr_t>(installs));
}
BOOL UnHook(hook_t* handle)
{
    removed.push_back(reinterpret_cast<uintptr_t>(handle));
    return TRUE;
}
void Reset()
{
    interpfix::UninstallHooks();
    installs = resolves = failInstall = 0;
    missingSymbol                     = nullptr;
    error[0]                          = '\0';
    removed.clear();
    hookedLookup = nullptr;
    hookedModel  = nullptr;
}
void Success()
{
    Reset();
    Check("install succeeds", 1, interpfix::InstallHooks());
    Check("install once", 2, installs);
    Check("repeated install succeeds", 1, interpfix::InstallHooks());
    Check("repeated install adds no hooks", 2, installs);
    cl_entity_t ent      = {};
    ent.current_position = 1;
    ent.ph[1].animtime   = 10;
    ent.ph[1].origin[0]  = 123;
    ent.ph[1].angles[1]  = 45;
    Check("installed model uses real adapter", 1, hookedModel(&ent));
    Check("installed lookup yields fallback origin", 123, static_cast<int>(ent.origin[0]));
    Check("installed lookup yields fallback angle", 45, static_cast<int>(ent.angles[1]));
    interpfix::UninstallHooks();
    Check("both removed", 2, static_cast<int>(removed.size()));
    if (removed.size() == 2)
    {
        Check("model removed first", 2, static_cast<int>(removed[0]));
        Check("lookup removed second", 1, static_cast<int>(removed[1]));
    }
    interpfix::UninstallHooks();
    Check("uninstall is idempotent", 2, static_cast<int>(removed.size()));
}
void Failures()
{
    for (const auto symbol : {"CL_FindInterpolationUpdates", "CL_InterpolateModel"})
    {
        Reset();
        missingSymbol = symbol;
        Check("missing symbol fails", 0, interpfix::InstallHooks());
        Check("missing symbol installs nothing", 0, installs);
        Check("error names symbol", 1, std::strstr(error, symbol) != nullptr);
        Check("error contains CRC64", 1, std::strstr(error, "0000000000001234") != nullptr);
        Check("error contains build", 1, std::strstr(error, "10257") != nullptr);
        Check("error contains status", 1, std::strstr(error, "SYMBOL_NOT_FOUND") != nullptr);
    }
    for (int failed = 1; failed <= 2; ++failed)
    {
        Reset();
        failInstall = failed;
        Check("hook failure fails installation", 0, interpfix::InstallHooks());
        Check("hook failure rolls back prior hooks", failed - 1, static_cast<int>(removed.size()));
        interpfix::UninstallHooks();
        Check("rollback leaves no hooks", failed - 1, static_cast<int>(removed.size()));
        Check("hook failure diagnostic", 1, std::strstr(error, "InlineHook failed") != nullptr);
    }
    Reset();
    Check("install can retry after failure", 1, interpfix::InstallHooks());
    interpfix::UninstallHooks();
}
} // namespace

int main()
{
    metahook_api_t api            = {};
    api.GetEngineBase             = EngineBase;
    api.GetEngineBuildnum         = EngineBuild;
    api.GetModuleCRC64            = CRC64;
    api.GetGameSymbolStatusString = Status;
    api.SysError                  = SysError;
    api.ResolveGameSymbol         = Resolve;
    api.InlineHook                = InlineHook;
    api.UnHook                    = UnHook;
    g_pMetaHookAPI                = &api;
    Success();
    Failures();
    std::printf("InterpFix hooks: %d failure(s)\n", failures);
    return failures ? 1 : 0;
}
