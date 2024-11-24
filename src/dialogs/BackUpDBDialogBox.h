/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef _BACKUP_DB_DLG_H_
#define _BACKUP_DB_DLG_H_

#include <MenuField.h>
#include <PopUpMenu.h>
#include <StringView.h>
#include <TextControl.h>
#include <Window.h>
#include <SupportDefs.h>
#include "../KeysDefs.h"

#if defined(USE_OPENSSL)
#define BKP_MODE_SSL    'ssl '
#endif
#define BKP_MODE_COPY   'copy'
#define BKP_PASSWORD    'pass'
#define BKP_MODIFIED    'modf'
#define BKP_CANCEL      'cncl'
#define BKP_SAVE        'save'

class BackUpDBDialogBox : public BWindow
{
public:
                    BackUpDBDialogBox(BWindow* parent, BRect frame);
    virtual void    MessageReceived(BMessage* msg);
    virtual void    FrameResized(float, float);
private:
    BButton        *fBtSave,
                   *fBtCancel;
    BMenuField     *fMfKind;
    BPopUpMenu     *fPumKind;
    BStringView    *fSvMthdDescription;
    BTextControl   *fTcPassword;
    BWindow        *fParent;

    uint32          fKind;
};

#endif /* _BACKUP_DB_DLG_H_ */
