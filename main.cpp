#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/proc.h>

int main(int argc, char **argv)
{
WHBProcInit();
WHBLogConsoleInit();


WHBLogPrintf("UiiverseTest started!");

while (WHBProcIsRunning())
{
    WHBLogConsoleDraw();
    OSSleepTicks(OSMillisecondsToTicks(100));
}

WHBLogConsoleFree();
WHBProcShutdown();

return 0;


}
