#pragma once
#include <string_view>
#include "wcgl/core/wcgl-types.h"

namespace wcgl {

void setDebugCallback(DebugCallback callback);
void debugPrint(DebugSeverity severity, std::string_view message);

}