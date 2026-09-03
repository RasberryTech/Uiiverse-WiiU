```cpp
#include <coreinit/time.h>
#include <whb/proc.h>
#include <whb/gfx.h>

int main(int argc, char **argv)
{
    WHBProcInit();

    if (!WHBGfxInit()) {
        WHBProcShutdown();
        return 1;
    }

    while (WHBProcIsRunning())
    {
        // Begin TV rendering.
        WHBGfxBeginRenderTV();

        // Clear TV to a light gray UI background.
        WHBGfxClearColor(0.92f, 0.92f, 0.92f, 1.0f);

        WHBGfxFinishRenderTV();

        // Begin GamePad/DRC rendering.
        WHBGfxBeginRenderDRC();

        // Clear DRC to the same background.
        WHBGfxClearColor(0.92f, 0.92f, 0.92f, 1.0f);

        WHBGfxFinishRenderDRC();

        OSSleepTicks(OSMillisecondsToTicks(16));
    }

    WHBGfxShutdown();
    WHBProcShutdown();

    return 0;
}
```
