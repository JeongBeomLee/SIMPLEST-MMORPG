#pragma once
#include <unordered_set>
#include <shared_mutex>
#include "Types.h"

struct Sector
{
	std::unordered_set<ObjectID> objects;
	mutable std::shared_mutex mutex;
};

