#pragma once
#include <string>

namespace offsets_auto
{
	// returns true if fresh offsets were loaded (or cache was valid).
	// never fatal: on any failure returns false and compiled offsets stay in use.
	bool ensure_latest();
	const std::string& status();
	const std::string& live_version();
}
