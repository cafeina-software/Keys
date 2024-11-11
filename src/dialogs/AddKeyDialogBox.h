/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __ADDKEY_DLG_H_
#define __ADDKEY_DLG_H_

#include <InterfaceKit.h>
#include <Key.h>
#include "../data/KeystoreImp.h"

#define AKDLG_KEY_ID        'k1id'
#define AKDLG_KEY_ID2       'k2id'
#define AKDLG_KEY_DATA      'kdat'
#define AKDLG_KEY_TYP_GEN   'tgen'
#define AKDLG_KEY_TYP_PWD   'tpwd'
#define AKDLG_KEY_TYP_CRT   'tcrt'
#define AKDLG_KEY_PUR_GEN   'pgen'
#define AKDLG_KEY_PUR_KRN   'pkrn'
#define AKDLG_KEY_PUR_WEB   'pweb'
#define AKDLG_KEY_PUR_NET   'pnet'
#define AKDLG_KEY_PUR_VOL   'pvol'
#define AKDLG_KEY_MODIFIED  'modi'
#define AKDLG_KEY_SAVE      'save'
#define AKDLG_KEY_CANCEL    'cncl'
#define I_KEY_ADD           'iaka'

class AddKeyDialogBox : public BWindow
{
public:
                  AddKeyDialogBox(BWindow* parent, BRect frame, KeystoreImp* _ks,
                    const char* keyringname, BKeyType desiredType, BView* view);
    virtual void  MessageReceived(BMessage* msg);
    virtual bool  QuitRequested();
private:
    status_t      _IsValid(BString info);
    void          _SaveKey();
    bool          _IsAbleToSave();
    void          _UpdateStatusBar(BStatusBar* bar, const char* pwd);
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
    BStatusBar   *sbPwdStrength;

    KeystoreImp  *ks;
    const char   *keyring;
    BString       currentId,
                  currentId2,
                  currentData;
    BKeyPurpose   currentPurpose;
    BKeyType      currentDesiredType;

    BWindow      *parentWindow;
    BView        *containerView;
};

#endif /* __ADDKEY_DLG_H_ */
