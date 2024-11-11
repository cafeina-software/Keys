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
                printf(B_TRANSLATE("%s: %s.\nUsage: %s [option]\n"
                    "[option] includes\n"
                    "\t--help      This message\n"
                    "\t--version   Shows app version\n"),
                    kAppName, kAppDesc, kAppName);
                return 0;
            case 2:
                printf(B_TRANSLATE("%s: version %s\n"), kAppName, kAppVersionStr);
                return 0;
            default:
                break;
        }
    }

    KeysApplication* app = new KeysApplication();
    app->Run();
    delete app;
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