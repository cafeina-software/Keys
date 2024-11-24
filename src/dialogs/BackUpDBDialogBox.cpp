/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <Application.h>
#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <PopUpMenu.h>
#include <SeparatorView.h>
#include "BackUpDBDialogBox.h"
#include "../KeysDefs.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Backupper dialog"

static struct BackUpMethod {
    uint32 what;
    const char* title;
    const char* description;
} methods [] = {
    {
        BKP_MODE_COPY, B_TRANSLATE("Simple copy"),
        B_TRANSLATE("Creates a simple copy of the keystore database.\nIt is not "
        "encrypted and anyone with access to the file could read its contents.")
    }
// #if defined(USE_OPENSSL)
    ,
    {
        'ssl ', B_TRANSLATE("Encrypted copy"),
        B_TRANSLATE("Creates an encrypted copy of the keystore database.\nOnce "
        "created, it can only be read by decrypting it with the proper passphrase.")
    }
// #endif
};

BackUpDBDialogBox::BackUpDBDialogBox(BWindow* parent, BRect frame)
: BWindow(frame, B_TRANSLATE("Back-up keystore database"), B_FLOATING_WINDOW,
    B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
  fParent(parent),
  fKind(BKP_MODE_COPY)
{
    fPumKind = new BPopUpMenu("pum_kind");
    for(uint32 i = 0; i < sizeof(methods) / sizeof(methods[0]); i++)
        fPumKind->AddItem(new BMenuItem(methods[i].title, new BMessage(methods[i].what)));
    fMfKind = new BMenuField("mf_kind", B_TRANSLATE("Kind"), fPumKind);
    fSvMthdDescription = new BStringView("sv_mthddesc", "");
    fTcPassword = new BTextControl("tc_pass", B_TRANSLATE("Password"), "", new BMessage(BKP_PASSWORD));
    fTcPassword->TextView()->HideTyping(true);
    fTcPassword->SetModificationMessage(new BMessage(BKP_MODIFIED));
    fBtSave = new BButton("bt_save", B_TRANSLATE("Back up"), new BMessage(BKP_SAVE));
    fBtCancel = new BButton("bt_cancel", B_TRANSLATE("Cancel"), new BMessage(BKP_CANCEL));
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .AddGroup(B_VERTICAL)
            .SetInsets(B_USE_WINDOW_INSETS, B_USE_WINDOW_INSETS, B_USE_WINDOW_INSETS, B_USE_SMALL_INSETS)
            .Add(fMfKind)
            .Add(fSvMthdDescription)
            .Add(fTcPassword)
        .End()
        .Add(new BSeparatorView())
        .AddGroup(B_HORIZONTAL)
            .SetInsets(B_USE_WINDOW_INSETS, B_USE_SMALL_INSETS, B_USE_WINDOW_INSETS, B_USE_WINDOW_INSETS)
            .AddGlue()
            .Add(fBtSave)
            .Add(fBtCancel)
        .End()
    .End();

    fMfKind->Menu()->FindItem(BKP_MODE_COPY)->SetMarked(true);
    fTcPassword->SetEnabled(!fMfKind->Menu()->FindItem(BKP_MODE_COPY)->IsMarked());
    fSvMthdDescription->SetText(B_TRANSLATE("Creates a simple copy of the keystore database.\n"
        "It is not encrypted and anyone with access to the file could read its contents."));
    CenterIn(fParent->Frame());
}

void BackUpDBDialogBox::FrameResized(float, float)
{
    CenterIn(fParent->Frame());
}

void BackUpDBDialogBox::MessageReceived(BMessage* msg)
{
    switch(msg->what)
    {
// #if defined(USE_OPENSSL)
        case BKP_MODE_SSL:
// #endif
        case BKP_MODE_COPY:
        {
            int i = 0;
            while(methods[i].what != msg->what)
                i++;
            if(methods[i].what == msg->what) {
                fKind = msg->what;
                fSvMthdDescription->SetText(methods[i].description);
                if(msg->what == BKP_MODE_COPY) {
                    fTcPassword->SetText("");
                }
                fTcPassword->SetEnabled(!fMfKind->Menu()->FindItem(BKP_MODE_COPY)->IsMarked());
                fBtSave->SetEnabled(fMfKind->Menu()->FindItem(BKP_MODE_COPY)->IsMarked() ||
                    (!fMfKind->Menu()->FindItem(BKP_MODE_COPY)->IsMarked() && fTcPassword->TextLength() > 0));
            }
            break;
        }
        case BKP_PASSWORD:
            if(!fMfKind->Menu()->FindItem(BKP_MODE_COPY)->IsMarked())
                fTcPassword->MarkAsInvalid(fTcPassword->TextLength() == 0);
            break;
        case BKP_MODIFIED:
            fBtSave->SetEnabled(fMfKind->Menu()->FindItem(BKP_MODE_COPY)->IsMarked() ||
                (!fMfKind->Menu()->FindItem(BKP_MODE_COPY)->IsMarked() && fTcPassword->TextLength() > 0));
            break;
        case BKP_SAVE:
        {
            BMessage request(M_KEYSTORE_BACKUP);
            request.AddUInt32("method", fKind);
            request.AddString("password", fTcPassword->Text());
            be_app->PostMessage(&request);
            Quit();
            break;
        }
        case BKP_CANCEL:
            Quit();
            break;
    }
}
