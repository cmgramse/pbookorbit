#pragma once

#include <functional>

// The SDK's e-ink platform, which the host tests don't have
namespace pocketbook::utilities {
struct EinkPlatform {
    void addHandlerForInkvewEvents(std::function<void(int, int, int)>) {}
};
inline EinkPlatform *einkPlatform() { return nullptr; }
}
