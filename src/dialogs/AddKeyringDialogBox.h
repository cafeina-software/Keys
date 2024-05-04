/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __ADDKEYRING_DLG_H_
#define __ADDKEYRING_DLG_H_

#include <InterfaceKit.h>
#include "KeystoreImp.h"

#define AKRDLG_SAVE         'akrs'
#define AKRDLG_CANCEL       'akrc'
#define AKRDLG_NAME_CHANGED 'akrn'

class AddKeyringDialogBox : public BWindow
{
public:
                  AddKeyringDialogBox(BRect frame, KeystoreImp& _ks);
    virtual void  MessageReceived(BMessage* msg);
private:
    status_t      _IsValid(KeystoreImp ks, BString name);
    void          _UpdateTextControlUI(bool tcinvalid, bool saveenabled,
                    BString erroricon, BString errortooltip, rgb_color errorcolor);
    void          _CallAddKeyring(KeystoreImp& ks, BString name);
private:
    KeystoreImp*  ks;
    BString       currentKeyringName;

    BFont         errorfont;

    BTextControl* tcKeyringName;
    BStringView*  svErrorDesc;
    BButton*      saveButton;
    BButton*      cancelButton;
};

#endif /* __ADDKEYRING_DLG_H_ */
