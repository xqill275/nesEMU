#include "src/header/EmuApp.h"

int main()
{
    EmuApp app;
    if (!app.init())
        return -1;

    int code = app.run();
    app.shutdown();
    return code;
}
