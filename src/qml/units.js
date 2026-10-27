// Density-independent sizes, as in the SDK's qml_test example (global/global.js): dp(56)
// is the same physical size on every device. dpScale comes from main.cpp.
//
// To turn a pixel measurement from an Era Color screenshot into dp, divide by 2.175
// (300 dpi x the system's scale factor 1.16 / 160) and pick the nearest whole number:
// native sizes are whole dp, so dp(56) shows as 122px and dp(4) as 9px.
function dp(v) {
    return Math.round(v * dpScale)
}
