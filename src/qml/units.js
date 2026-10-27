// Density-independent sizes, as in the SDK's qml_test (global/global.js): dp(56) is the
// same physical size on every device. dpScale is set in main.cpp.
//
// To convert a pixel measurement from an Era Color screenshot to dp, divide by 2.175 (300
// dpi x scale factor 1.16 / 160) and round. Native sizes are whole dp, so dp(56) is 122px
// and dp(4) is 9px.
function dp(v) {
    return Math.round(v * dpScale)
}
