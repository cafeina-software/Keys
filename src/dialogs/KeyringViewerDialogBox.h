/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __KEYRING_VIEWER_H_
#define __KEYRING_VIEWER_H_

#include <CheckBox.h>
#include <StringView.h>
#include <SupportDefs.h>
#include <TextControl.h>
#include <Window.h>
#include "../data/KeystoreImp.h"

class KeyringViewerDialogBox : public BWindow
{
public:
                    KeyringViewerDialogBox(BWindow* parent, BRect frame, KeystoreImp* imp, const char* keyring);
    virtual void    MessageReceived(BMessage* msg);
    void            InitUIData();
private:
    KeystoreImp    *fImp;
    const char     *fKeyringName;

    BButton        *fBtClose;
    BCheckBox      *fCbLocked;
    BStringView    *fSvNameLabel,
                   *fSvLockedLabel,
                   *fSvKeysLabel,
                   *fSvGenKeysLabel,
                   *fSvPwdKeysLabel,
                   *fSvAppsLabel;
    BTextControl   *fTcName,
                   *fTcKeyCount,
                   *fTcGenKeyCount,
                   *fTcPwdKeyCount,
                   *fTcAppCount;
};

#endif /* __KEYRING_VIEWER_H_ */
