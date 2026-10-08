#include <metahook.h>
#include "privatehook.h"

metahook_api_t* g_pMetaHookAPI = nullptr;
mh_interface_t* g_pInterface = nullptr;
mh_enginesave_t* g_pMetaSave = nullptr;
cl_enginefunc_t gEngfuncs = {};

void IPluginsV4::Init(metahook_api_t* api, mh_interface_t* interfaces, mh_enginesave_t* save)
{
	g_pMetaHookAPI = api;
	g_pInterface = interfaces;
	g_pMetaSave = save;
	if (interfaces->MetaHookAPIVersion < interpfix::RequiredMetaHookAPIVersion)
		api->SysError("[InterpFix] MetaHook API 109 or newer is required.");
}

void IPluginsV4::LoadEngine(cl_enginefunc_t* engineFuncs)
{
	gEngfuncs = *engineFuncs;
	interpfix::InstallHooks();
}

void IPluginsV4::LoadClient(cl_exportfuncs_t*) {}
void IPluginsV4::ExitGame(int) { interpfix::UninstallHooks(); }
void IPluginsV4::Shutdown() { interpfix::UninstallHooks(); }
const char* IPluginsV4::GetVersion() { return "1.0.0"; }

EXPOSE_SINGLE_INTERFACE(IPluginsV4, IPluginsV4, METAHOOK_PLUGIN_API_VERSION_V4);
