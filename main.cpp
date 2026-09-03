#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/memdefaultheap.h>
#include <whb/file.h>
#include <whb/gfx.h>
#include <whb/log.h>
#include <whb/proc.h>
#include <vpad/input.h>

#include <stdint.h>
#include <string.h>

// Stage 2: load the original MemoTop layout from the WUHB content bundle,
// read its pane names, and provide GamePad touch/button interaction.
// The BFLYT/BFLIM data remains untouched in content/memo.

static const char *kMemoLayout =
    "/vol/content/memo/layout/Body/Common-extracted/blyt/MemoTop.bflyt";

struct Stage2State {
    bool running;
    bool pressed;
    uint32_t selected;
    float touchX;
    float touchY;
};

static bool hasName(const uint8_t *data, uint32_t size, const char *name)
{
    const size_t n = strlen(name);
    if (n == 0 || size < n)
        return false;

    for (uint32_t i = 0; i + n <= size; ++i) {
        if (memcmp(data + i, name, n) == 0)
            return true;
    }
    return false;
}

static void scanMemoLayout()
{
    uint32_t size = 0;
    void *raw = WHBReadWholeFile(kMemoLayout, &size);
    if (!raw) {
        WHBLogPrintf("Stage2: could not load %s", kMemoLayout);
        return;
    }

    const uint8_t *data = static_cast<const uint8_t *>(raw);
    const bool valid = size >= 4 && memcmp(data, "FLYT", 4) == 0;

    WHBLogPrintf("Stage2: MemoTop %s (%u bytes)", valid ? "loaded" : "invalid", size);
    WHBLogPrintf("Stage2: BtnPen=%s BtnErs=%s BtnUndo=%s BtnWrite=%s",
                 hasName(data, size, "BtnPen") ? "yes" : "no",
                 hasName(data, size, "BtnErs") ? "yes" : "no",
                 hasName(data, size, "BtnUndo") ? "yes" : "no",
                 hasName(data, size, "BtnWriteIcon") ? "yes" : "no");

    WHBFreeWholeFile(raw);
}

static void readGamePad(Stage2State &state)
{
    VPADStatus status;
    VPADReadError error;
    memset(&status, 0, sizeof(status));

    if (VPADRead(VPAD_CHAN_0, &status, 1, &error) <= 0)
        return;

    state.pressed = false;

    if (status.trigger & VPAD_BUTTON_A) {
        state.selected = (state.selected + 1) & 3;
        state.pressed = true;
    }

    if (status.trigger & VPAD_BUTTON_B) {
        state.selected = 3;
        state.pressed = true;
    }

    if (status.touch.touched && status.touch.validity == VPAD_TOUCH_VALID) {
        state.touchX = static_cast<float>(status.touch.x);
        state.touchY = static_cast<float>(status.touch.y);

        // MemoTop's four primary controls occupy the right-hand toolbar.
        // Touching the corresponding horizontal bands selects a control.
        if (state.touchX >= 650.0f && state.touchX <= 854.0f) {
            if (state.touchY < 120.0f)
                state.selected = 0; // Pen
            else if (state.touchY < 240.0f)
                state.selected = 1; // Eraser
            else if (state.touchY < 360.0f)
                state.selected = 2; // Write
            else
                state.selected = 3; // Undo
            state.pressed = true;
        }
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    WHBProcInit();
    WHBGfxInit();
    WHBLogConsoleInit();

    Stage2State state = {true, false, 0, 0.0f, 0.0f};
    scanMemoLayout();

    WHBLogPrintf("Stage 2 interactive UI started");
    WHBLogPrintf("A = next control, B = Undo, touch = select toolbar");

    while (WHBProcIsRunning() && state.running)
    {
        readGamePad(state);

        WHBGfxBeginRender();

        // Keep the Wii U render path active while Stage 2 assets are loaded.
        // The selected control gets a visible color state so interaction can
        // be verified on real hardware before the BFLIM/GX2 material renderer
        // is enabled.
        switch (state.selected) {
            case 0:
                WHBGfxClearColor(0.88f, 0.93f, 1.0f, 1.0f);
                break;
            case 1:
                WHBGfxClearColor(0.93f, 0.88f, 1.0f, 1.0f);
                break;
            case 2:
                WHBGfxClearColor(0.88f, 1.0f, 0.92f, 1.0f);
                break;
            default:
                WHBGfxClearColor(1.0f, 0.93f, 0.88f, 1.0f);
                break;
        }

        WHBGfxFinishRender();
        WHBLogConsoleDraw();

        if (state.pressed) {
            WHBLogPrintf("Stage2: selected control %u (touch %.0f, %.0f)",
                         state.selected, state.touchX, state.touchY);
        }

        OSSleepTicks(OSMillisecondsToTicks(16));
    }

    WHBLogConsoleFree();
    WHBGfxShutdown();
    WHBProcShutdown();
    return 0;
}
