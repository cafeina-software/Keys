/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __KEYS_APP_H_
#define __KEYS_APP_H_

#include <AppKit.h>
#include <KeyStore.h>
#include "KeysWindow.h"

class KeysApplication : public BApplication
{
public:
    KeysApplication();
    virtual void ReadyToRun();
    virtual void MessageReceived(BMessage* msg);
    virtual void AboutRequested();
    virtual bool QuitRequested();
private:
    void     _InitAppData(KeystoreImp* ks, BKeyStore* keystore);
    void     _InitKeyring(KeystoreImp* ks, BKeyStore* keystore, const char* kr);
    status_t _RestartServer();
    status_t _LoadSettings();
    status_t _SaveSettings();
private:
    KeysWindow* window;
    BMessage currentSettings;
    BRect frame;
    BKeyStore keystore;
    KeystoreImp ks;
};

#endif /* __KEY_APP_H_ */
