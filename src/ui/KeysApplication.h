/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __KEYS_APP_H_
#define __KEYS_APP_H_

#include <AppKit.h>
#include <KeyStore.h>
#include "KeysWindow.h"
#include "../KeysDefs.h"
#include "../data/KeystoreImp.h"

class KeysApplication : public BApplication
{
public:
                    KeysApplication();
    virtual        ~KeysApplication();
    virtual void    ReadyToRun();
    virtual void    MessageReceived(BMessage* msg);
    virtual void    AboutRequested();
    virtual bool    QuitRequested();

    void            KeystoreBackup(BMessage* msg);
    void            KeystoreRestore(BMessage* msg);
    void            WipeKeystoreContents(BMessage* msg);
    void            AddKeyring(BMessage* msg);
    void            LockKeyring(BMessage* msg);
    void            SetKeyringLockKey(BMessage* msg);
    void            RemoveKeyringLockKey(BMessage* msg);
    void            WipeKeyringContents(BMessage* msg);
    void            RemoveKeyring(BMessage* msg);
    void            AddKey(BMessage* msg);
    void            GeneratePwdKey(BMessage* msg);
    void            ImportKey(BMessage* msg);
    void            ExportKey(BMessage* msg);
    void            RemoveKey(BMessage* msg);
    void            RemoveApp(BMessage* msg);
private:
    void            _InitAppData(KeystoreImp*& ks, BKeyStore* keystore);
    void            _InitKeyring(KeystoreImp*& ks, BKeyStore* keystore, const char* kr);
    void            _Notify(void* ptr, BMessage* msg, status_t result);
    status_t        _StopServer(bool rebuildModel = true);
    status_t        _RestartServer(bool rebuildModel = true, bool closeIfRunning = false);
    static int32    _CallServerMonitor(void* data);
    static void     _CallRebuildModel(void* data);
    void            _RebuildModel();
    status_t        _LoadSettings();
    status_t        _SaveSettings();
private:
    KeysWindow     *window;
    BMessage        currentSettings;
    BRect           frame;
    BKeyStore       keystore;
    KeystoreImp    *ks;
    node_ref        databaseNRef;
    thread_id       thServerMonitor;
};

#endif /* __KEY_APP_H_ */
