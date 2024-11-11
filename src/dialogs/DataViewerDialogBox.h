/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __DATA_VIEWER_DLG_H_
#define __DATA_VIEWER_DLG_H_

#include <InterfaceKit.h>
#include <Key.h>
#include "../data/KeystoreImp.h"

#define DV_CLOSE 'clse'

class DataViewerDialogBox : public BWindow
{
public:
                  DataViewerDialogBox(BWindow* parent, BRect frame,
                    KeystoreImp* imp, const char* keyring, const char* keyid);
    virtual void  MessageReceived(BMessage* msg);
private:
    void          _InitUIData();
    void          _ProcessHexData(const void* indata, const size_t inlength,
                    BString* outdata);
    void          _ProcessEnumData(const int type, const char* desc,
                    BString* out);
    void          _ProcessPurpose(const BKeyPurpose, BString* out);
private:
    KeystoreImp  *fImp;
    const char   *fKeyringName,
                 *fKeyId;

    BButton      *closeButton;
    BTextControl *tcIdentifier;
    BTextControl *tcSecIdentifier;
    BTextControl *tcType;
    BTextControl *tcPurpose;
    BTextControl *tcOwner;
    BTextControl *tcDCreated;
    BTextView    *tvData;
    BTextView    *tvHex;
};

#endif /* __DATA_VIEWER_DLG_H_ */