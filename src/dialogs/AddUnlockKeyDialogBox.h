/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __ADDUNLOCKKEY_DLG_H_
#define __ADDUNLOCKKEY_DLG_H_

#include <StatusBar.h>
#include <SupportDefs.h>
#include <TextControl.h>
#include <Window.h>
#include "../data/KeystoreImp.h"

#define UNL_SET_DATA    'kdat'
#define UNL_MODIFIED    'modi'
#define UNL_CANCEL      'cncl'
#define UNL_SAVE        'save'

class AddUnlockKeyDialogBox : public BWindow
{
public:
                        AddUnlockKeyDialogBox(BWindow* parent, BRect frame,
                            KeystoreImp* imp, const char* keyring);
    virtual void        MessageReceived(BMessage* msg);
    void                _UpdateStatusBar(BStatusBar* bar, const char* pwd);
    void                _SaveKey();
private:
    KeystoreImp        *fDatabase;
    const char         *fKeyringName;

    BTextControl       *fTcData;
    BStatusBar         *fSbPwdStrength;
    BButton            *fBtSave,
                       *fBtCancel;
};

#endif /* __ADDUNLOCKKEY_DLG_H_ */
