#include "interpolation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace
{
int failures = 0;
thread_local float targetTime = 0.0f;
void Check(const char* label, double expected, double actual)
{
	if (!(std::fabs(expected - actual) <= 0.0001))
	{
		std::printf("FAIL %s: expected %.6f, got %.6f\n", label, expected, actual);
		++failures;
	}
}

void Sample(cl_entity_t& ent, float time, float x, float yaw = 30.0f)
{
	ent.current_position = (ent.current_position + 1) & HISTORY_MASK;
	auto& pose = ent.ph[ent.current_position];
	pose.animtime = time;
	pose.origin[0] = x;
	pose.angles[1] = yaw;
}

// Model the original engine's rejection order and ordinary interpolation.
// This callback calls the production lookup, just as the hooked engine does.
int __cdecl OriginalModel(cl_entity_t* ent)
{
	position_history_t* newer = nullptr;
	position_history_t* older = nullptr;
	interpfix::FindInterpolationUpdates(ent, targetTime, &newer, &older, nullptr);
	if (targetTime - older->animtime < 0.0f || older->animtime == 0.0f)
		return 0;
	if (older->origin[0] == 0.0f && newer->origin[0] != 0.0f)
		return 0;
	float fraction = newer->animtime == older->animtime ? 1.0f :
		(targetTime - older->animtime) / (newer->animtime - older->animtime);
	if (fraction > 1.0f)
		fraction = 1.0f;
	for (int axis = 0; axis < 3; ++axis)
	{
		ent->origin[axis] = older->origin[axis] + fraction * (newer->origin[axis] - older->origin[axis]);
		float delta = newer->angles[axis] - older->angles[axis];
		if (delta > 180.0f)
			delta -= 360.0f;
		else if (delta < -180.0f)
			delta += 360.0f;
		ent->angles[axis] = older->angles[axis] + fraction * delta;
		while (ent->angles[axis] > 180.0f)
			ent->angles[axis] -= 360.0f;
		while (ent->angles[axis] < -180.0f)
			ent->angles[axis] += 360.0f;
	}
	return 1;
}

void Pose(const char* label, cl_entity_t& ent, float time, float x, float yaw)
{
	targetTime = time;
	Check(label, 1, interpfix::InterpolateModel(&ent, OriginalModel));
	Check(label, x, ent.origin[0]);
	Check(label, yaw, ent.angles[1]);
}

void PartialHistory()
{
	cl_entity_t ent = {};
	Sample(ent, 10, 100, 170);
	Sample(ent, 11, 200, -170);
	Pose("normal interpolation", ent, 10.5f, 150, 180);
	Pose("newer than latest", ent, 12, 200, -170);
	Pose("older than partial history", ent, 9, 100, 170);
	Pose("exact oldest timestamp", ent, 10, 100, 170);
	interpfix::FindInterpolationUpdates(&ent, 9, nullptr, nullptr, nullptr);
}

void WrappedHistory(int extra)
{
	cl_entity_t ent = {};
	for (int i = 0; i < HISTORY_MAX * 2 + extra; ++i)
		Sample(ent, 10.0f + i, 1.0f + i);
	const int oldest = (ent.current_position + 1) & HISTORY_MASK;
	const auto sample = ent.ph[oldest];
	Pose("older than wrapped history", ent, sample.animtime - 1, sample.origin[0], 30);
	Pose("wrapped oldest equality", ent, sample.animtime, sample.origin[0], 30);
	Pose("oldest interval", ent, sample.animtime + 0.5f, sample.origin[0] + 0.5f, 30);
	position_history_t* newer = nullptr;
	position_history_t* older = nullptr;
	int index = -1;
	Check("clamp is not extrapolation", 0,
		interpfix::FindInterpolationUpdates(&ent, sample.animtime - 1, &newer, &older, &index));
	Check("oldest ring index", oldest, index);
	Check("clamped endpoints", 1, newer == older && older == &ent.ph[oldest]);
	Check("history not modified", 0, std::memcmp(&sample, &ent.ph[oldest], sizeof(sample)));
}

void SparseAndReset()
{
	cl_entity_t ent = {};
	targetTime = 9;
	Check("empty history preserves failure", 0, interpfix::InterpolateModel(&ent, OriginalModel));
	Sample(ent, 10, 0, 30);
	Pose("single sample before", ent, 9, 0, 30);
	Pose("single sample after", ent, 11, 0, 30);
	Sample(ent, 11, 100, 40);
	Pose("valid world origin", ent, 9, 0, 30);
	const auto latest = ent.ph[ent.current_position];
	std::memset(ent.ph, 0, sizeof(ent.ph));
	ent.current_position = 1;
	ent.ph[0] = ent.ph[1] = latest;
	Pose("reset excludes prior lifetime", ent, 9, 100, 40);
}

void HighRate(int rate, int snapshotRate, float interpolation)
{
	cl_entity_t ent = {};
	int accepted = 0;
	for (int frame = 0; frame < rate * 2; ++frame)
	{
		const double now = 10.0 + static_cast<double>(frame) / rate;
		if (frame == 0 || frame * snapshotRate / rate != (frame - 1) * snapshotRate / rate)
			Sample(ent, static_cast<float>(now), 100);
		targetTime = static_cast<float>(now - interpolation);
		const int result = interpfix::InterpolateModel(&ent, OriginalModel);
		if (frame >= rate)
			accepted += result != 0;
	}
	char label[100];
	std::snprintf(label, sizeof(label), "%d FPS / %d snapshots / %.2f interp", rate, snapshotRate, interpolation);
	Check(label, rate, accepted);
}

int __cdecl SkipLookup(cl_entity_t* ent)
{
	ent->origin[0] = 321;
	return 1;
}
int __cdecl FailWithoutLookup(cl_entity_t*) { return 0; }

void OriginalResults()
{
	cl_entity_t ent = {};
	Sample(ent, 10, 100);
	Check("early engine success", 1, interpfix::InterpolateModel(&ent, SkipLookup));
	Check("early engine pose", 321, ent.origin[0]);
	Check("unrelated failure preserved", 0, interpfix::InterpolateModel(&ent, FailWithoutLookup));
	Sample(ent, 11, 200);
	targetTime = 10.5f;
	Pose("successful engine result", ent, targetTime, 150, 30);
	cl_entity_t invalid = {};
	Sample(invalid, 10, 0);
	Sample(invalid, 11, 100);
	targetTime = 10.5f;
	Check("unclamped engine failure preserved", 0, interpfix::InterpolateModel(&invalid, OriginalModel));
}

cl_entity_t nestedEntity = {};
int __cdecl NestedOriginal(cl_entity_t* ent)
{
	interpfix::FindInterpolationUpdates(ent, 9, nullptr, nullptr, nullptr);
	Check("nested unrelated failure", 0, interpfix::InterpolateModel(&nestedEntity, FailWithoutLookup));
	return 0;
}
int __cdecl OtherEntityLookup(cl_entity_t*)
{
	interpfix::FindInterpolationUpdates(&nestedEntity, 9, nullptr, nullptr, nullptr);
	return 0;
}
void NestedContexts()
{
	cl_entity_t ent = {};
	Sample(ent, 10, 100, 40);
	nestedEntity = {};
	Sample(nestedEntity, 10, 999, 50);
	Check("outer context survives nested call", 1, interpfix::InterpolateModel(&ent, NestedOriginal));
	Check("outer fallback pose", 100, ent.origin[0]);
	Check("other entity cannot mark context", 0, interpfix::InterpolateModel(&ent, OtherEntityLookup));
	Check("context cleared after call", 0, interpfix::InterpolateModel(&ent, FailWithoutLookup));
}
}

int main()
{
	PartialHistory();
	WrappedHistory(0);
	WrappedHistory(HISTORY_MAX - 1);
	SparseAndReset();
	for (const int rate : {600, 650, 2000, 2500})
		HighRate(rate, rate, 0.1f);
	HighRate(2000, 2000, 0.01f);
	HighRate(2500, 20, 0.1f);
	OriginalResults();
	NestedContexts();
	std::printf("InterpFix interpolation: %d failure(s)\n", failures);
	return failures ? 1 : 0;
}
