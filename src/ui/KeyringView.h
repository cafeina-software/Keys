/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __KEYRING_VIEW_H_
#define __KEYRING_VIEW_H_

#include <InterfaceKit.h>
#include <private/interface/ColumnListView.h>
#include <private/shared/ToolBar.h>
#include "../data/KeystoreImp.h"

#define KRV_KEYS_SEL    'ksel'
#define KRV_KEYS_INVOKE 'kivk'
#define KRV_KEYS_CREATE 'knew'
#define KRV_KEYS_REMOVE 'krem'
#define KRV_KEYS_COPY   'kcpy'
#define KRV_KEYS_VWDATA 'kvwd'
#define KRV_KEYS_EXPORT 'kexp'
#define KRV_APPS_SEL    'asel'
#define KRV_APPS_INVOKE 'aivk'
#define KRV_APPS_REMOVE 'arem'
#define KRV_APPS_COPY   'acpy'
#define KRV_APPS_VWDATA 'avwd'

class KeyringView : public BView
{
public:
                        KeyringView(BRect frame, const char* name, KeystoreImp* _ks);
    virtual             ~KeyringView();

    virtual void        AttachedToWindow();
    virtual void        MessageReceived(BMessage* msg);

    virtual void        Update(const void* data = NULL);
private:
            void        _InitAppData(const char* target);
            void        _CopyKey(KeystoreImp* ks);
            void        _CopyAppSignature(KeystoreImp* ks);
            void        _CopyData(KeystoreImp* ks, BColumnListView* owner, const uint8* data);
            void        _RemoveKey(KeystoreImp* ks, const char* id, const char* sec);
            void        _RemoveApp(KeystoreImp* ks, const char* _app);
private:
    BTabView            *tabView;
    BColumnListView     *keylistview,
                        *applistview;
    BToolBar            *keylsttoolbar,
                        *applsttoolbar;

    KeystoreImp         *ks;
    const char          *keyringname;
};

#endif
