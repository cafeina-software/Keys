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
    virtual             ~KeysApplication();

    virtual bool        QuitRequested();
    virtual void        ReadyToRun();
    virtual void        MessageReceived(BMessage* msg);
    virtual void        ArgvReceived(int32 argc, char** argv);
    virtual void        RefsReceived(BMessage* msg);
    virtual void        AboutRequested();

    virtual BHandler*   ResolveSpecifier(BMessage* msg, int32 index,
                            BMessage* specifier, int32 form,
                            const char* property);
    virtual status_t    GetSupportedSuites(BMessage* data);
            void        HandleScripting(BMessage* msg);

            void        CreateSettings(BMessage* archive);
            status_t    LoadSettings();
            status_t    SaveSettings();

            status_t    StartServer(bool rebuildModel = true,
                            bool forceRestart = false);
            status_t    StopServer(bool rebuildModel = true);

            status_t    KeystoreBackup(BMessage* msg);
            status_t    KeystoreRestore(BMessage* msg);
            void        WipeKeystoreContents(BMessage* msg);
            status_t    AddKeyring(BMessage* msg);
            status_t    LockKeyring(BMessage* msg);
            status_t    SetKeyringLockKey(BMessage* msg);
            status_t    RemoveKeyringLockKey(BMessage* msg);
            void        WipeKeyringContents(BMessage* msg);
            status_t    RemoveKeyring(BMessage* msg);
            status_t    AddKey(BMessage* msg);
            status_t    GeneratePwdKey(BMessage* msg);
            status_t    ImportKey(BMessage* msg);
            status_t    ExportKey(BMessage* msg);
            status_t    RemoveKey(BMessage* msg);
            status_t    CopyKeyData(BMessage* msg);
            status_t    RemoveApp(BMessage* msg);
private:
            void        _InitAppData(const BMessage* data);
            void        _InitKeystoreData(KeystoreImp*& ks, BKeyStore* keystore);
            void        _InitKeyring(KeystoreImp*& ks, BKeyStore* keystore,
                            const char* kr);

            void        _Notify(void* ptr, BMessage* msg, status_t result);
    static  int32       _CallServerMonitor(void* data);
    static  void        _CallRebuildModel(void* data);
            void        _RebuildModel();
            void        _ClipboardJanitor();
private:
    KeysWindow     *window;
    BMessage        currentSettings;
    BRect           frame;
    BKeyStore       keystore;
    KeystoreImp    *ks;
    node_ref        databaseNRef;
    thread_id       thServerMonitor;
    const char     *inFocus;
    bool            hasDataCopied;
    BMessageRunner* clipboardCleanerRunner;
};

#endif /* __KEY_APP_H_ */
