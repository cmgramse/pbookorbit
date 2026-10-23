// Density-independent sizes, as in the SDK's qml_test example (global/global.js): dp(56)
// is the same physical size on every device. dpScale comes from main.cpp.
function dp(v) {
    return Math.round(v * dpScale)
}
