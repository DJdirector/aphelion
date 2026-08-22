#pragma once

#include <memory>

#include "provider.h"
#include "settings.h"

namespace ai {

// Constructs the right AIProvider for settings.ai.provider. Throws
// std::runtime_error if the provider name isn't recognized.
std::unique_ptr<AIProvider> createProvider(const AISettings& settings);

}  // namespace ai
