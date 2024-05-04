/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __KEYS_WIN_H_
#define __KEYS_WIN_H_

#include <InterfaceKit.h>
#include <KeyStore.h>
#include <list>
#include "KeysDefs.h"
#include "KeystoreImp.h"
#include "KeyringView.h"

#define I_QUIT             'iqt '
#define I_ABOUT            'iabt'
#define I_SERVER_RESTART   'isvr'
#define I_SELECTED         'isel'
#define I_KEYSTORE_INFO    'iksi'
#define I_KEYRING_ADD      'ikra'
#define I_KEYRING_REMOVE   'ikrr'
#define I_KEYRING_INFO     'ikri'
#define I_KEY_ADD_GENERIC  'iakg'
#define I_KEY_ADD_PASSWORD 'iakp'
#define I_KEY_ADD_CERT     'iakc'
#define I_KEY_COPY_DATA    'ikcp'
#define I_KEY_REMOVE       'irem'
#define I_APP_REMOVE       'iarm'

class KeysWindow : public BWindow
{
public:
                 KeysWindow(BRect frame, KeystoreImp* ks, BKeyStore* _keystore);
    virtual void MessageReceived(BMessage* msg);
    void         SetUIStatus(ui_status status);
    ui_status    GetUIStatus();
    void         Update(const void* data = NULL);
private:
    void         _InitAppData(KeystoreImp* ks);
    void         _FullReload(KeystoreImp* ks);
    status_t     _AddKeyring(KeystoreImp* _ks);
    status_t     _RemoveKeyring(KeystoreImp* ks, const char* target);
    BMenuBar*       _InitMenu();
private:
    std::list<KeyringView*> keyringviewlist;
    BKeyStore*              keystore;
    KeystoreImp*            _ks;
    const char*             currentKeyring;

    BListView              *listView;
    BCardView              *cardView;
    BButton                *addKeyringButton,
                           *removeKeyringButton;
    BMenuBar               *mbMain;
    BMenuItem              *fRemKeyring,
                           *fMenuKeyring;
    ui_status               uist;
};

#endif /* __KEYS_WIN_H_ */
