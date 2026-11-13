// StudioHub — v0.1 skeleton test.
//
// Goal of this first version: prove the whole pipeline works on-device
// before any real feature (replay, camera, posing) is built on top of it.
// It draws:
//   - a thin icon rail on the left edge (4 buttons: Replay, Camera, Pose,
//     Settings — none of them do anything real yet)
//   - tapping a rail icon opens a panel next to it, with a header, a
//     close (X) button, and one placeholder button that just logs a tap
//   - the panel can be dragged by its header
//
// This uses the documented LeviAbi interface (submitDrawCommands +
// registerTouchCallback), which the launcher exposes itself and does not
// depend on libminecraftpe.so at all — so it should keep working across
// Minecraft updates as long as the launcher's own ABI doesn't change.
//
// Coordinates below are tuned for a 1612x720 landscape screen (this
// device's own reported display size). If the panel looks off on a
// different screen, the ref width/height constants are the first thing
// to adjust.

#include "LeviAbi.hpp"

#define TAG "StudioHub"
#define LOGI(...) __android_log_print(LOG_INFO_VALUE, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(LOG_ERROR_VALUE, TAG, __VA_ARGS__)

namespace {

constexpr const char* kModuleId = "studiohub";
constexpr float kRefW = 1612.0f;
constexpr float kRefH = 720.0f;

leviabi::Api gApi{};
bool gAvailable = false;

// ---------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------
constexpr float kRailX = 0.0f;
constexpr float kRailW = 64.0f;
constexpr int kRailButtons = 4;
constexpr float kRailButtonH = 64.0f;
constexpr float kRailButtonGap = 8.0f;
constexpr float kRailTopY = 120.0f;  // leave room above for the game's own HUD

constexpr float kPanelW = 420.0f;
constexpr float kPanelHeaderH = 56.0f;
constexpr float kPanelBodyH = 300.0f;
constexpr float kCloseBtnSize = 40.0f;

enum class Section { None = -1, Replay = 0, Camera = 1, Pose = 2, Settings = 3 };

const char* sectionLabel(Section s) {
    switch (s) {
        case Section::Replay: return "Replay";
        case Section::Camera: return "Camera";
        case Section::Pose: return "Pose";
        case Section::Settings: return "Settings";
        default: return "";
    }
}

struct State {
    bool open = false;
    Section active = Section::None;
    float panelX = kRailW + 12.0f;
    float panelY = kRailTopY;
    bool dragging = false;
    float dragGrabOffsetX = 0.0f;
    float dragGrabOffsetY = 0.0f;
    int tapCount = 0;
} gState;

// ---------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------
constexpr int kMaxCommands = 64;
leviabi::DrawCommand gCmds[kMaxCommands];
int gCmdCount = 0;

void resetCmds() { gCmdCount = 0; }

void pushRect(float x, float y, float w, float h, unsigned color, bool filled) {
    if (gCmdCount >= kMaxCommands) return;
    leviabi::DrawCommand& c = gCmds[gCmdCount++];
    c = {};
    c.type = filled ? leviabi::DrawCommandType::RectFilled : leviabi::DrawCommandType::Rect;
    c.x = x;
    c.y = y;
    c.w = w;
    c.h = h;
    c.color = color;
}

void pushText(float x, float y, const char* text, unsigned color, float size) {
    if (gCmdCount >= kMaxCommands) return;
    leviabi::DrawCommand& c = gCmds[gCmdCount++];
    c = {};
    c.type = leviabi::DrawCommandType::Text;
    c.x = x;
    c.y = y;
    c.color = color;
    c.size = size;
    leviabi::setShortString(c.text, text);
}

bool hit(float px, float py, float x, float y, float w, float h) {
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

// ---------------------------------------------------------------------
// Frame build
// ---------------------------------------------------------------------
void buildFrame() {
    resetCmds();

    // --- icon rail ---
    pushRect(kRailX, kRailTopY, kRailW, (kRailButtonH + kRailButtonGap) * kRailButtons, 0xAA1A1A1Au, true);
    for (int i = 0; i < kRailButtons; ++i) {
        float by = kRailTopY + i * (kRailButtonH + kRailButtonGap);
        bool activeHere = gState.open && static_cast<int>(gState.active) == i;
        unsigned bg = activeHere ? 0xFF3A7BD5u : 0xFF2A2A2Au;
        pushRect(kRailX + 4.0f, by + 4.0f, kRailW - 8.0f, kRailButtonH - 8.0f, bg, true);
        pushText(kRailX + 10.0f, by + kRailButtonH / 2.0f - 6.0f, sectionLabel(static_cast<Section>(i)), 0xFFFFFFFFu, 12.0f);
    }

    if (!gState.open) return;

    // --- panel ---
    pushRect(gState.panelX, gState.panelY, kPanelW, kPanelHeaderH + kPanelBodyH, 0xE6101010u, true);
    // header (draggable)
    pushRect(gState.panelX, gState.panelY, kPanelW, kPanelHeaderH, 0xFF202020u, true);
    pushText(gState.panelX + 16.0f, gState.panelY + 18.0f, sectionLabel(gState.active), 0xFFFFFFFFu, 18.0f);
    // close button
    float closeX = gState.panelX + kPanelW - kCloseBtnSize - 8.0f;
    float closeY = gState.panelY + 8.0f;
    pushRect(closeX, closeY, kCloseBtnSize, kCloseBtnSize, 0xFFB03030u, true);
    pushText(closeX + 13.0f, closeY + 9.0f, "X", 0xFFFFFFFFu, 18.0f);

    // body: one placeholder button, proves the touch path works end to end
    float btnX = gState.panelX + 24.0f;
    float btnY = gState.panelY + kPanelHeaderH + 24.0f;
    float btnW = kPanelW - 48.0f;
    float btnH = 56.0f;
    pushRect(btnX, btnY, btnW, btnH, 0xFF2E7D32u, true);
    char label[32];
    label[0] = 0;
    appendString(label, sizeof(label), "Test tap: ");
    char numBuf[12];
    snprintf(numBuf, sizeof(numBuf), "%d", gState.tapCount);
    appendString(label, sizeof(label), numBuf);
    pushText(btnX + 16.0f, btnY + 18.0f, label, 0xFFFFFFFFu, 16.0f);

    gApi.submitDrawCommands(kModuleId, strlen(kModuleId), gCmds, static_cast<usize>(gCmdCount));
}

// ---------------------------------------------------------------------
// Touch handling
// ---------------------------------------------------------------------
// action codes follow Android's MotionEvent: 0 = down, 1 = up, 2 = move.
constexpr int kActionDown = 0;
constexpr int kActionUp = 1;
constexpr int kActionMove = 2;

bool touchThunk(int action, int /*pointerId*/, float x, float y) {
    if (!gAvailable) return false;

    // Scale from this device's reference frame — if the real screen is a
    // different size, we'd convert here. Left as 1:1 for now since this is
    // tuned directly for the device it was built for.
    const float px = x;
    const float py = y;

    bool consumed = false;

    if (action == kActionDown) {
        // rail buttons
        for (int i = 0; i < kRailButtons; ++i) {
            float by = kRailTopY + i * (kRailButtonH + kRailButtonGap);
            if (hit(px, py, kRailX, by, kRailW, kRailButtonH)) {
                Section s = static_cast<Section>(i);
                if (gState.open && gState.active == s) {
                    gState.open = false;
                } else {
                    gState.open = true;
                    gState.active = s;
                }
                consumed = true;
                break;
            }
        }

        if (!consumed && gState.open) {
            float closeX = gState.panelX + kPanelW - kCloseBtnSize - 8.0f;
            float closeY = gState.panelY + 8.0f;
            if (hit(px, py, closeX, closeY, kCloseBtnSize, kCloseBtnSize)) {
                gState.open = false;
                consumed = true;
            }
        }

        if (!consumed && gState.open) {
            float btnX = gState.panelX + 24.0f;
            float btnY = gState.panelY + kPanelHeaderH + 24.0f;
            float btnW = kPanelW - 48.0f;
            float btnH = 56.0f;
            if (hit(px, py, btnX, btnY, btnW, btnH)) {
                gState.tapCount++;
                LOGI("test button tapped, count=%d", gState.tapCount);
                consumed = true;
            }
        }

        if (!consumed && gState.open &&
            hit(px, py, gState.panelX, gState.panelY, kPanelW, kPanelHeaderH)) {
            gState.dragging = true;
            gState.dragGrabOffsetX = px - gState.panelX;
            gState.dragGrabOffsetY = py - gState.panelY;
            consumed = true;
        }
    } else if (action == kActionMove) {
        if (gState.dragging) {
            gState.panelX = px - gState.dragGrabOffsetX;
            gState.panelY = py - gState.dragGrabOffsetY;
            // keep the panel fully on-screen
            if (gState.panelX < kRailW) gState.panelX = kRailW;
            if (gState.panelY < 0.0f) gState.panelY = 0.0f;
            if (gState.panelX + kPanelW > kRefW) gState.panelX = kRefW - kPanelW;
            if (gState.panelY + kPanelHeaderH + kPanelBodyH > kRefH)
                gState.panelY = kRefH - kPanelHeaderH - kPanelBodyH;
            consumed = true;
        }
    } else if (action == kActionUp) {
        if (gState.dragging) {
            gState.dragging = false;
            consumed = true;
        }
    }

    buildFrame();
    return consumed;  // false lets the tap pass through to the game underneath
}

bool gRegistered = false;

bool loadImpl(void*) {
    LOGI("load");
    return true;
}

bool enableImpl(void*) {
    gAvailable = leviabi::resolveApi(gApi);
    if (!gAvailable) {
        LOGE("resolveApi failed — launcher API not found, StudioHub stays inactive");
        return true;  // don't fail the whole mod load over this
    }

    // A module must be registered before submitDrawCommands/touch input do
    // anything — confirmed by reading how the real OfflineMusicPlayer mod
    // uses this same API.
    leviabi::ModuleInfo info{};
    leviabi::setShortString(info.moduleId, kModuleId);
    leviabi::setShortString(info.displayName, "Studio Hub");
    leviabi::setShortString(info.description, "In-world panel for Replay/Camera/Pose (skeleton test)");
    leviabi::setShortString(info.modId, kModuleId);
    info.defaultEnabled = true;
    info.hideInHudEditor = true;
    info.configs = {};  // no configs yet
    leviabi::initializeEmptyCallbacks(info);

    gRegistered = gApi.registerModule(&info);
    if (!gRegistered) {
        LOGE("registerModule failed");
        return true;
    }

    gApi.input->registerTouchCallback(&touchThunk);
    buildFrame();
    LOGI("enabled (api resolved, module registered)");
    return true;
}

bool disableImpl(void*) {
    gState.open = false;
    if (gAvailable && gRegistered) {
        buildFrame();  // clear the panel from the screen
        gApi.unregisterModule(kModuleId, strlen(kModuleId));
        gRegistered = false;
    }
    LOGI("disabled");
    return true;
}

bool unloadImpl(void*) {
    disableImpl(nullptr);
    LOGI("unloaded");
    return true;
}

struct ModRegistration {
    void* instance;
    bool (*load)(void*, void*);
    bool (*enable)(void*, void*);
    bool (*disable)(void*, void*);
    bool (*unload)(void*, void*);
};

bool loadBridge(void*, void* ctx) { return loadImpl(ctx); }
bool enableBridge(void*, void* ctx) { return enableImpl(ctx); }
bool disableBridge(void*, void* ctx) { return disableImpl(ctx); }
bool unloadBridge(void*, void* ctx) { return unloadImpl(ctx); }

ModRegistration gRegistration{};

}  // namespace

extern "C" __attribute__((visibility("default"))) ModRegistration* PLGetModRegistration() {
    gRegistration.instance = nullptr;
    gRegistration.load = &loadBridge;
    gRegistration.enable = &enableBridge;
    gRegistration.disable = &disableBridge;
    gRegistration.unload = &unloadBridge;
    return &gRegistration;
}
