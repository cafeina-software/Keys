/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <Catalog.h>
#include <cstdio>
#include "ui/KeysApplication.h"
#include "KeysDefs.h"

int option(const char* op);

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "main"

int main (int argc, char** argv)
{
    if(argc > 1) {
        switch(option(argv[1]))
        {
            case 1:
                return help();
            case 2:
                return version();
            default:
                break;
        }
    }

    KeysApplication* app = new KeysApplication();
    app->Run();
    delete app;
    return 0;
}

int help()
{
    BString helpString(B_TRANSLATE(
        "%appName%: %appDesc%.\n"
        "Command line only usage: %appName% [option]\n"
        "[option] includes\n"
        "\t%helpParam%               %helpParamDesc%\n"
        "\t%versionParam%            %versionParamDesc%\n"
        "\n"
        "Graphic interface usage: %appName% [option 1] [option ...]\n"
        "[option N] includes one or several of these\n"
        "\t%keyringParam% <name>     %keyringParamDesc%\n"
        "\t%resetSetsParam%     %resetSetsParamDesc%\n"
        "\n"
        "To get information about scripting support, please run in a Terminal \n"
        "the following command while the application is running:\n"
        "\t%heyCommand%\n"
    ));
    helpString.ReplaceAll("%appName%", kAppName);
    helpString.ReplaceAll("%appDesc%", B_TRANSLATE(kAppDesc));
    helpString.ReplaceAll("%helpParam%", "--help");
    helpString.ReplaceAll("%helpParamDesc%", B_TRANSLATE("Shows the help (this message)."));
    helpString.ReplaceAll("%versionParam%", "--version");
    helpString.ReplaceAll("%versionParamDesc%", B_TRANSLATE("Shows the application version."));
    helpString.ReplaceAll("%keyringParam%", "--keyring");
    helpString.ReplaceAll("%keyringParamDesc%", B_TRANSLATE("Opens the user interface with <name> keyring in focus."));
    helpString.ReplaceAll("%resetSetsParam%", "--reset-settings");
    helpString.ReplaceAll("%resetSetsParamDesc%", B_TRANSLATE("Resets the application's settings before initialization."));
    BString heyCommand("hey %appName% getsuites");
    heyCommand.ReplaceAll("%appName%", kAppName);
    helpString.ReplaceAll("%heyCommand%", heyCommand.String());

    printf("%s", helpString.String());
    return 0;
}

int version()
{
    printf(B_TRANSLATE("%s: version %s\n"), kAppName, kAppVersionStr);
    return 0;
}

int option(const char* op)
{
    if(strcmp(op, "--help") == 0)
        return 1;
    if(strcmp(op, "--version") == 0)
        return 2;
    else
        return 0;
}
