#include "interpolation.h"
#include <cstring>

namespace interpfix
{
namespace
{
struct InterpolationContext
{
    cl_entity_t*       entity;
    bool               hasFallback = false;
    position_history_t fallback    = {};
};

thread_local InterpolationContext* activeContext = nullptr;

class ContextScope
{
public:
    explicit ContextScope(InterpolationContext& context) : previous(activeContext)
    {
        activeContext = &context;
    }
    ~ContextScope() { activeContext = previous; }
    ContextScope(const ContextScope&)            = delete;
    ContextScope& operator=(const ContextScope&) = delete;

private:
    InterpolationContext* previous;
};
} // namespace

qboolean __cdecl FindInterpolationUpdates(cl_entity_t* ent, float targetTime, position_history_t** newer, position_history_t** older, int* newerIndex)
{
    int       i0          = ent->current_position & HISTORY_MASK;
    int       i1          = (ent->current_position - 1) & HISTORY_MASK;
    const int newest      = i0;
    int       oldest      = newest;
    qboolean  extrapolate = true;
    for (int offset = 1; offset < HISTORY_MAX; ++offset)
    {
        const int   index = (ent->current_position - offset) & HISTORY_MASK;
        const float time  = ent->ph[index].animtime;
        if (time == 0.0f)
            break;
        oldest = index;
        if (targetTime > time)
        {
            i0          = (index + 1) & HISTORY_MASK;
            i1          = index;
            extrapolate = false;
            break;
        }
    }
    const bool clamped = extrapolate && ent->ph[oldest].animtime != 0.0f &&
        (oldest == newest || targetTime <= ent->ph[oldest].animtime);
    if (clamped)
    {
        i0 = i1     = oldest;
        extrapolate = false;
    }
    // Capture only this model invocation's lookup. Other callers keep the same ABI.
    if (activeContext && activeContext->entity == ent)
    {
        activeContext->hasFallback = clamped;
        if (clamped)
            activeContext->fallback = ent->ph[oldest];
    }
    if (newer)
        *newer = &ent->ph[i0];
    if (older)
        *older = &ent->ph[i1];
    if (newerIndex)
        *newerIndex = i0;
    return extrapolate;
}

int InterpolateModel(cl_entity_t* ent, InterpolateModelFn original)
{
    InterpolationContext context{ent};
    ContextScope         scope(context);
    const int            result = original(ent);
    // The original engine rejects a negative time delta before its equal-time
    // branch. Preserve its other decisions and repair only a valid history clamp.
    if (result == 0 && context.hasFallback)
    {
        std::memcpy(ent->origin, context.fallback.origin, sizeof(ent->origin));
        std::memcpy(ent->angles, context.fallback.angles, sizeof(ent->angles));
        return 1;
    }
    return result;
}
} // namespace interpfix
