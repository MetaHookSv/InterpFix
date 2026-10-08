#pragma once

namespace interpfix
{
constexpr int RequiredMetaHookAPIVersion = 109;
bool InstallHooks();
void UninstallHooks();
}
