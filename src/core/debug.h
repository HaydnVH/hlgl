#pragma once
#include <string_view>
#include <hlgl/core/types.h>

namespace hlgl {

void setDebugCallback(DebugCallback callback);
void debugPrint(DebugSeverity severity, std::string_view message);

}