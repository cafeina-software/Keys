/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <KeyStore.h>
#include <LayoutBuilder.h>
#include <SeparatorView.h>
#include <StringView.h>
#include <Window.h>
#include <string>
#include "KeyringViewerDialogBox.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Keyring viewer"

KeyringViewerDialogBox::KeyringViewerDialogBox(BWindow* parent, BRect frame, KeystoreImp* imp, const char* keyring)
: BWindow(frame, B_TRANSLATE("Keyring: <no keyring>"), B_FLOATING_WINDOW,
    B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS | B_CLOSE_ON_ESCAPE),
  fImp(imp),fKeyringName(keyring)
{
    BString title(B_TRANSLATE("Keyring: %name% %status%"));
    title.ReplaceAll("%name%", fKeyringName);
    title.ReplaceAll("%status%", fImp->KeyringByName(fKeyringName)->IsUnlocked() ?
        "" /* Nothing */ : B_TRANSLATE("(locked)"));
    SetTitle(title.String());

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .AddGroup(B_VERTICAL)
            .SetInsets(B_USE_WINDOW_INSETS)
            .AddGrid(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING)
                .Add(fSvNameLabel = new BStringView("sv_name", B_TRANSLATE("Name")), 0, 0)
                .Add(fTcName = new BTextControl("tc_name", NULL, "", NULL), 1, 0)
                .Add(fSvLockedLabel = new BStringView("sv_lock", B_TRANSLATE("Status")), 0, 1)
                .Add(fCbLocked = new BCheckBox("cb_lock", B_TRANSLATE("Locked"), NULL), 1, 1)
            .End()
            .Add(new BSeparatorView())
            .AddGrid(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING)
                .Add(fSvKeysLabel = new BStringView("sv_keys", B_TRANSLATE("Keys")), 0, 0)
                .Add(fTcKeyCount = new BTextControl(NULL, NULL, "0", NULL), 1, 0)
                .AddGroup(B_HORIZONTAL, 0, 0, 1)
                    .SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_MIDDLE))
                    .AddStrut(B_USE_DEFAULT_SPACING)
                    .Add(fSvGenKeysLabel = new BStringView(NULL, B_TRANSLATE("Generic keys")))
                .End()
                .Add(fTcGenKeyCount = new BTextControl(NULL, NULL, "0", NULL), 1, 1)
                .AddGroup(B_HORIZONTAL, 0, 0, 2)
                    .SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_MIDDLE))
                    .AddStrut(B_USE_DEFAULT_SPACING)
                    .Add(fSvPwdKeysLabel = new BStringView(NULL, B_TRANSLATE("Password keys")))
                .End()
                .Add(fTcPwdKeyCount = new BTextControl(NULL, NULL, "0", NULL), 1, 2)
                .Add(fSvAppsLabel = new BStringView("sv_apps", B_TRANSLATE("Applications with access")), 0, 3)
                .Add(fTcAppCount = new BTextControl(NULL, NULL, "0", NULL), 1, 3)
            .End()
        .End()
        .Add(new BSeparatorView())
        .AddGroup(B_HORIZONTAL)
            .SetInsets(B_USE_WINDOW_INSETS)
            .AddGlue()
            .Add(fBtClose = new BButton("bt_clse", B_TRANSLATE("Close"), new BMessage('clse')))
        .End()
    .End();

    fTcName->SetExplicitMinSize(BSize(fTcName->StringWidth(fKeyringName) + 10, B_SIZE_UNSET));
    fCbLocked->SetEnabled(false);
    for(const auto& it : { fTcName, fTcKeyCount, fTcGenKeyCount, fTcPwdKeyCount, fTcAppCount }) {
        it->TextView()->MakeEditable(false);
        it->SetAlignment(B_ALIGN_LEFT, it == fTcName ? B_ALIGN_LEFT : B_ALIGN_RIGHT);
    }

    InitUIData();
    CenterIn(parent->Frame());
}

void KeyringViewerDialogBox::MessageReceived(BMessage* msg)
{
    if(msg->what == 'clse') Quit();
    else BWindow::MessageReceived(msg);
}

void KeyringViewerDialogBox::InitUIData()
{
    int gkeyc = fImp->KeyringByName(fKeyringName)->KeyCount(B_KEY_TYPE_GENERIC);
    int pkeyc = fImp->KeyringByName(fKeyringName)->KeyCount(B_KEY_TYPE_PASSWORD);
    int keyc = gkeyc + pkeyc;
    int appc = fImp->KeyringByName(fKeyringName)->ApplicationCount();
    bool unlocked = fImp->KeyringByName(fKeyringName)->IsUnlocked();

    fTcName->SetText(fKeyringName);
    fCbLocked->SetValue(unlocked ? B_CONTROL_OFF : B_CONTROL_ON);
    fTcKeyCount->SetText(std::to_string(keyc).c_str());
    fTcGenKeyCount->SetText(std::to_string(gkeyc).c_str());
    fTcPwdKeyCount->SetText(std::to_string(pkeyc).c_str());
    fTcAppCount->SetText(std::to_string(appc).c_str());
}
