/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __ADDKEY_DLG_H_
#define __ADDKEY_DLG_H_

#include <InterfaceKit.h>
#include <Key.h>
#include <private/interface/Spinner.h>
#include "../data/KeystoreImp.h"

#define AKDLG_KEY_ID        'k1id'
#define AKDLG_KEY_ID2       'k2id'
#define AKDLG_KEY_DATA      'kdat'
#define AKDLG_KEY_DATA_GEN  'gen0'
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
#define AKDLG_KEY_LENGTH    'ilen'
#define I_KEY_ADD           'iaka'

enum AKDlgModel {
    AKDM_STANDARD = 0,
    AKDM_UNLOCK_KEY,
    AKDM_KEY_GEN
};

class AddKeyDialogBox : public BWindow
{
public:
                  AddKeyDialogBox(BWindow* parent, BRect frame,
                    const char* keyringname, BKeyType desiredType,
                    BView* view, AKDlgModel dialogType = AKDM_STANDARD);
    virtual void  MessageReceived(BMessage* msg);
    virtual bool  QuitRequested();
private:
    status_t      _IsValid(BString info);
    void          _SaveKey();
    bool          _IsAbleToSave();
    void          _UpdateStatusBar(BStatusBar* bar, const char* pwd);
private:
    BButton      *fBtSave,
                 *fBtKeyGen,
                 *fBtCancel;
    BMenuField   *fMfType,
                 *fMfPurpose;
    BPopUpMenu   *fPumType,
                 *fPumPurpose;
    BSpinner     *fSpnLength;
    BStatusBar   *fSbPwdStrength;
    BStringView  *fSvIntro,
                 *fSvSpnInfo;
    BTextControl *fTcIdentifier,
                 *fTcSecIdentifier,
                 *fTcData;

    const char   *keyring;
    BString       currentId,
                  currentId2,
                  currentData;
    BKeyPurpose   currentPurpose;
    BKeyType      currentDesiredType;

    BWindow      *parentWindow;
    BView        *containerView;
    AKDlgModel    dialogModel;
};

#endif /* __ADDKEY_DLG_H_ */
