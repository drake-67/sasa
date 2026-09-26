#pragma once
#include <string>
#include <features/settings.h>

namespace config
{
	std::string file_path(const char* name);
	bool save(const char* name);
	bool load(const char* name);
}
