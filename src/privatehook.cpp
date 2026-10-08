#include "interpolation.h"
#include "privatehook.h"

static_assert(METAHOOK_API_VERSION >= interpfix::RequiredMetaHookAPIVersion,
              "InterpFix requires gamedata FUNCTION resolution (MetaHook API 109)");

namespace interpfix
{
namespace
{
hook_t*            lookupHook     = nullptr;
hook_t*            modelHook      = nullptr;
void*              originalLookup = nullptr;
InterpolateModelFn originalModel  = nullptr;

void ReportFailure(const char* symbol, const char* reason)
{
    uint64_t   crc64  = 0;
    const auto status = g_pMetaHookAPI->GetModuleCRC64(g_pMetaHookAPI->GetEngineBase(), &crc64);
    if (status == MH_GAMESYMBOL_OK)
        g_pMetaHookAPI->SysError("[InterpFix] %s (module engine)\nEngine buildnum: %d\nCRC64: %016llx\nReason: %s",
                                 symbol, g_pMetaHookAPI->GetEngineBuildnum(), static_cast<unsigned long long>(crc64), reason);
    else
        g_pMetaHookAPI->SysError("[InterpFix] %s (module engine)\nEngine buildnum: %d\nReason: %s\nCRC64 unavailable: %s",
                                 symbol, g_pMetaHookAPI->GetEngineBuildnum(), reason, g_pMetaHookAPI->GetGameSymbolStatusString(status));
}

bool Resolve(const char* symbol, void** address)
{
    const auto status = g_pMetaHookAPI->ResolveGameSymbol(g_pMetaHookAPI->GetEngineBase(),
                                                          symbol, MH_GAMESYMBOL_KIND_FUNCTION, address);
    if (status == MH_GAMESYMBOL_OK)
        return true;
    ReportFailure(symbol, g_pMetaHookAPI->GetGameSymbolStatusString(status));
    return false;
}

int __cdecl ModelHook(cl_entity_t* ent)
{
    return InterpolateModel(ent, originalModel);
}
} // namespace

bool InstallHooks()
{
    if (lookupHook && modelHook)
        return true;
    void* lookupAddress = nullptr;
    void* modelAddress  = nullptr;
    if (!Resolve("CL_FindInterpolationUpdates", &lookupAddress) ||
        !Resolve("CL_InterpolateModel", &modelAddress))
        return false;
    lookupHook = g_pMetaHookAPI->InlineHook(lookupAddress,
                                            reinterpret_cast<void*>(FindInterpolationUpdates), &originalLookup);
    if (!lookupHook)
    {
        ReportFailure("CL_FindInterpolationUpdates", "InlineHook failed");
        return false;
    }
    modelHook = g_pMetaHookAPI->InlineHook(modelAddress,
                                           reinterpret_cast<void*>(ModelHook), reinterpret_cast<void**>(&originalModel));
    if (!modelHook)
    {
        UninstallHooks();
        ReportFailure("CL_InterpolateModel", "InlineHook failed");
        return false;
    }
    return true;
}

void UninstallHooks()
{
    if (modelHook)
    {
        g_pMetaHookAPI->UnHook(modelHook);
        modelHook = nullptr;
    }
    if (lookupHook)
    {
        g_pMetaHookAPI->UnHook(lookupHook);
        lookupHook = nullptr;
    }
    originalModel  = nullptr;
    originalLookup = nullptr;
}
} // namespace interpfix
