/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __ADDKEY_DLG_H_
#define __ADDKEY_DLG_H_

#include <InterfaceKit.h>
#include <Key.h>
#include "KeystoreImp.h"

#define AKDLG_KEY_ID      'k1id'
#define AKDLG_KEY_ID2     'k2id'
#define AKDLG_KEY_DATA    'kdat'
#define AKDLG_KEY_TYP_GEN 'tgen'
#define AKDLG_KEY_TYP_PWD 'tpwd'
#define AKDLG_KEY_TYP_CRT 'tcrt'
#define AKDLG_KEY_PUR_GEN 'pgen'
#define AKDLG_KEY_PUR_KRN 'pkrn'
#define AKDLG_KEY_PUR_WEB 'pweb'
#define AKDLG_KEY_PUR_NET 'pnet'
#define AKDLG_KEY_PUR_VOL 'pvol'
#define AKDLG_KEY_SAVE    'save'
#define AKDLG_KEY_CANCEL  'cncl'

class AddKeyDialogBox : public BWindow
{
public:
                  AddKeyDialogBox(BRect frame, KeystoreImp* _ks,
        const char* keyringname, BKeyType desiredType);
    virtual void  MessageReceived(BMessage* msg);
    virtual bool  QuitRequested();
private:
    status_t      _IsValid(KeystoreImp* _ks, BString info);
    void          _SaveKey();
    bool          _IsAbleToSave();
private:
    BTextControl *tcIdentifier,
                 *tcSecIdentifier,
                 *tcData;
    BMenuField   *mfType,
                 *mfPurpose;
    BMenu        *pumType,
                 *pumPurpose;
    BButton      *saveKeyButton,
                 *cancelButton;

    KeystoreImp  *ks;
    const char   *keyring;
    BString       currentId,
                  currentId2,
                  currentData;
    BKeyPurpose   currentPurpose;
    BKeyType      currentDesiredType;
};

#endif /* __ADDKEY_DLG_H_ */
