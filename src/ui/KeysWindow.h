/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __KEYS_WIN_H_
#define __KEYS_WIN_H_

#include <FilePanel.h>
#include <InterfaceKit.h>
#include <KeyStore.h>
#include <list>
#include "../KeysDefs.h"
#include "../data/KeystoreImp.h"
#include "../dialogs/AddKeyDialogBox.h"
#include "KeyringView.h"
#include "ListViewEx.h"

#define I_QUIT             'iqt '
#define I_ABOUT            'iabt'
#define I_SERVER_RESTART   'isvr'
#define I_SERVER_STOP      'stop'
#define I_DATA_REFRESH     'irfs'
#define I_SELECTED         'isel'
#define I_KEYSTORE_BACKUP  'iksb'
#define I_KEYSTORE_RESTORE 'iksr'
#define I_KEYSTORE_INFO    'iksi'
#define I_KEYSTORE_CLEAR   'iksw'
#define I_KEYRING_ADD      'ikra'
#define I_KEYRING_REMOVE   'ikrr'
#define I_KEYRING_INFO     'ikri'
#define I_KEYRING_LOCK     'lock'
#define I_KEYRING_UNLOCKKEY_ADD 'ikrl'
#define I_KEYRING_UNLOCKKEY_REM 'ikru'
#define I_KEYRING_CLEAR 'ikrw'

#define I_KEY_ADD          'iaka'
#define I_KEY_ADD_GENERIC  'iakg'
#define I_KEY_ADD_PASSWORD 'iakp'
#define I_KEY_ADD_CERT     'iakc'
#define I_KEY_GENERATE_PWD 'igen'
#define I_KEY_IMPORT       'iiky'
#define I_KEY_EXPORT       'kexp'
#define I_KEY_COPY_DATA    'ikcp'
#define I_KEY_REMOVE       'irem'
#define I_APP_REMOVE       'iarm'

extern BBitmap* addKeyIcon;
extern BBitmap* removeKeyIcon;
extern BBitmap* exportKeyIcon;
extern BBitmap* copySecretIcon;
extern BBitmap* viewKeyDataIcon;
extern BBitmap* removeAppIcon;
extern BBitmap* copySignIcon;
extern BBitmap* trackerIcon;

void init_shared_icons();

class SimpleFilter : public BRefFilter {
public:
    virtual bool Filter(const entry_ref* ref, BNode* node,
    struct stat_beos* stat, const char* mimeType) {
        return node->IsDirectory() ||
               node->IsFile();
    }
};

class KeyMsgRefFilter : public BRefFilter
{
public:
    virtual bool Filter(const entry_ref* ref, BNode* node,
    struct stat_beos* stat, const char* mimeType) {
        return node->IsDirectory() ||
               IsValidFile(*ref);
    }
private:
    bool IsValidFile(entry_ref ref) {
        BFile file(&ref, B_READ_ONLY);
        if(file.InitCheck() != B_OK)
            return false;

        BMessage data;
        if(data.Unflatten(&file) != B_OK)
            return false;

        BKey key;
        BPasswordKey pwdkey;
        if(key.Unflatten(data) != B_OK && pwdkey.Unflatten(data) != B_OK)
            return false;

        return true;
    }
};

class KeysWindow : public BWindow
{
public:
                            KeysWindow(BRect frame, KeystoreImp* ks, BKeyStore* _keystore);
                           ~KeysWindow();
    virtual void            MessageReceived(BMessage* msg);

    void                    SetUIStatus(ui_status status, const char* focus = nullptr);
    ui_status               GetUIStatus();
    void                    Update(const void* data = NULL);
    void                    UpdateAsEmpty();
private:
    void                    _InitAppData(KeystoreImp* ks);
    void                    _FullReload(KeystoreImp* ks);

    void                    _KeystoreInfo();
    status_t                _AddKeyring();
    status_t                _RemoveKeyring(const char* target = nullptr);
    void                    _LockKeyring();
    void                    _SetKeyringLockKey();
    void                    _RemoveKeyringLockKey();
    void                    _ClearKeyring();
    void                    _KeyringInfo();
    void                    _AddKey(BKeyType type, AKDlgModel model);
    void                    _ImportKey(BMessage* msg);
    void                    _ExportKey(BMessage* msg);
    void                    _RemoveKey(BMessage* msg);

    KeyringView            *_FindKeyringView(const char* target);
    void                    _NotifyKeyringView(const char* target);

    BMenuBar*               _InitMenu();
    BPopUpMenu             *_InitKeystorePopUpMenu();
    BPopUpMenu             *_InitKeyringPopUpMenu();
private:
    std::list<KeyringView*> keyringviewlist;
    BKeyStore*              keystore;
    KeystoreImp*            ks;
    const char*             currentKeyring;
    ui_status               uist;
    KeyMsgRefFilter        *fFilter;

    BListViewEx            *listView;
    BCardView              *cardView;
    BButton                *addKeyringButton,
                           *removeKeyringButton;
    BFilePanel             *openPanel,
                           *savePanel;
    BMenuBar               *mbMain;
    BMenuItem              *fRemKeyring,
                           *fIsLockedKeyring,
                           *fMenuKeystore,
                           *fMenuKeyring;
    BPopUpMenu             *fKeystorePopMenu,
                           *fKeyringItemMenu;
};

#endif /* __KEYS_WIN_H_ */
