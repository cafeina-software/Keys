/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <Application.h>
#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <SeparatorView.h>
#include <StatusBar.h>
#include <StringView.h>
#include "AddUnlockKeyDialogBox.h"
#include "../data/PasswordStrength.h"
#include "../KeysDefs.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Add unlock key"

AddUnlockKeyDialogBox::AddUnlockKeyDialogBox(BWindow* parent, BRect frame,
    KeystoreImp* imp, const char* keyring)
: BWindow(frame, "", B_FLOATING_WINDOW,
    B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
  fDatabase(imp),
  fKeyringName(keyring)
{
    BString title(B_TRANSLATE("%name%: set unlock key"));
    title.ReplaceAll("%name%", fKeyringName);
    SetTitle(title.String());

    BString desc(B_TRANSLATE("Adding key to \"%desc%\" keyring."));
    desc.ReplaceAll("%desc%", keyring);
    BStringView* intro = new BStringView("sv_intro", desc);

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .SetInsets(0)
        .AddGroup(B_HORIZONTAL)
            .SetInsets(B_USE_WINDOW_INSETS)
            .Add(intro)
        .End()
        .AddGrid()
            .SetInsets(B_USE_SMALL_INSETS)
            .AddTextControl(fTcData = new BTextControl("tc_key", B_TRANSLATE("Key"),
                "", new BMessage(UNL_SET_DATA)), 0, 0)
            .Add(fSbPwdStrength = new BStatusBar("sb_ent"), 1, 1)
        .End()
        .Add(new BSeparatorView())
        .AddGroup(B_HORIZONTAL)
            .SetInsets(B_USE_WINDOW_INSETS)
            .AddGlue()
            .Add(fBtCancel = new BButton("bt_cancel", B_TRANSLATE("Cancel"), new BMessage(UNL_CANCEL)))
            .Add(fBtSave = new BButton("bt_save", B_TRANSLATE("Save"), new BMessage(UNL_SAVE)))
        .End()
    .End();

    fTcData->SetModificationMessage(new BMessage(UNL_MODIFIED));
    fTcData->TextView()->HideTyping(true);
    fSbPwdStrength->SetBarHeight(fTcData->Bounds().Height() / 2.0f);
    fSbPwdStrength->SetMaxValue(1.0f);
    fBtSave->SetEnabled(false);

    CenterIn(parent->Frame());
}

void AddUnlockKeyDialogBox::MessageReceived(BMessage* msg)
{
    switch(msg->what)
    {
        case UNL_SET_DATA:
            fTcData->MarkAsInvalid(fTcData->TextLength() == 0);
            break;
        case UNL_MODIFIED:
            _UpdateStatusBar(fSbPwdStrength, fTcData->Text());
            fBtSave->SetEnabled(fTcData->TextLength() > 0);
            break;
        case UNL_CANCEL:
            Quit();
            break;
        case UNL_SAVE:
            if(fTcData->TextLength() > 0) {
                _SaveKey();
                Quit();
            }
            break;
        default:
            BWindow::MessageReceived(msg);
            break;
    }
}

void AddUnlockKeyDialogBox::_UpdateStatusBar(BStatusBar* bar, const char* pwd)
{
    float value = PasswordStrength(pwd);
    bar->SetTo(value
#if 0
    , std::to_string(value).c_str()
#endif
    );
    if(value < 0.5f)
        bar->SetBarColor(ui_color(B_FAILURE_COLOR));
    else if(value > 0.5f && value < 0.75f)
        bar->SetBarColor({255, 255, 0});
    else
        bar->SetBarColor(ui_color(B_SUCCESS_COLOR));
}

void AddUnlockKeyDialogBox::_SaveKey()
{
    BPasswordKey pass(fTcData->Text(), B_KEY_PURPOSE_KEYRING, "");
    BMessage keydata;
    pass.Flatten(keydata);
    BString name(fKeyringName);

    BMessenger msgr(be_app_messenger);
    BMessage savemsg(M_KEYRING_SET_LOCKKEY);
    savemsg.AddString(kConfigKeyring, name.String());
    savemsg.AddMessage(kConfigKey, &keydata);
    msgr.SendMessage(&savemsg, msgr);
}
