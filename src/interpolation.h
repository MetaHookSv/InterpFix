#pragma once

#include <metahook.h>
#include <cl_entity.h>

namespace interpfix
{
using InterpolateModelFn = int(__cdecl*)(cl_entity_t*);

qboolean __cdecl FindInterpolationUpdates(cl_entity_t* ent, float targetTime, position_history_t** newer, position_history_t** older, int* newerIndex);
int              InterpolateModel(cl_entity_t* ent, InterpolateModelFn original);
} // namespace interpfix
