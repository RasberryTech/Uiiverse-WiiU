#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <whb/gfx.h>
#include <whb/log.h>
#include <whb/proc.h>

int main(int argc, char **argv)
{
    WHBProcInit();
    WHBGfxInit();

    while (WHBProcIsRunning())
    {
        WHBGfxBeginRender();
        WHBGfxClearColor(0.92f, 0.92f, 0.92f, 1.0f);
        WHBGfxFinishRender();

        OSSleepTicks(OSMillisecondsToTicks(16));
    }

    WHBGfxShutdown();
    WHBProcShutdown();
    return 0;
}
