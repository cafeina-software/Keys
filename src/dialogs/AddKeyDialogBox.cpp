/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <Application.h>
#include <Catalog.h>
#include <KeyStore.h>
#include <cstdio>
#include "AddKeyDialogBox.h"
#include "KeysDefs.h"
#include "KeystoreImp.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Add key dialog box"

AddKeyDialogBox::AddKeyDialogBox(BRect frame, KeystoreImp* _ks,
    const char* keyringname, BKeyType desiredType)
: BWindow(frame, B_TRANSLATE("Add new key"), B_FLOATING_WINDOW_LOOK, B_FLOATING_ALL_WINDOW_FEEL,
    B_NOT_ZOOMABLE|B_NOT_RESIZABLE|B_AUTO_UPDATE_SIZE_LIMITS),
    ks(_ks),
    keyring(keyringname),
    currentId(""),
    currentId2(""),
    currentData(""),
    currentPurpose(B_KEY_PURPOSE_ANY),
    currentDesiredType(desiredType)
{
    BString desc(B_TRANSLATE("Adding key to \"%desc%\" keyring."));
    desc.ReplaceAll("%desc%", keyring);
    BStringView* intro = new BStringView(NULL, desc);

    pumType = new BPopUpMenu(B_TRANSLATE("Type of key"), B_ITEMS_IN_COLUMN);
    BLayoutBuilder::Menu<>(pumType)
        .AddItem(B_TRANSLATE("Generic key"), AKDLG_KEY_TYP_GEN)
        .AddItem(B_TRANSLATE("Password key"), AKDLG_KEY_TYP_PWD)
        .AddItem(B_TRANSLATE("Certificate key"), AKDLG_KEY_TYP_CRT)
    .End();
    mfType = new BMenuField(B_TRANSLATE("Type"), pumType);
    mfType->Menu()->FindItem(AKDLG_KEY_TYP_CRT)->SetEnabled(false);
    if(currentDesiredType == B_KEY_TYPE_PASSWORD)
        mfType->Menu()->FindItem(AKDLG_KEY_TYP_PWD)->SetMarked(true);
    else if(currentDesiredType == B_KEY_TYPE_GENERIC)
        mfType->Menu()->FindItem(AKDLG_KEY_TYP_GEN)->SetMarked(true);

    tcIdentifier = new BTextControl("tc_id", B_TRANSLATE("Identifier"), currentId,
        new BMessage(AKDLG_KEY_ID));
    tcIdentifier->SetModificationMessage(new BMessage(AKDLG_KEY_MODIFIED));
    tcSecIdentifier = new BTextControl("tc_id2",
        B_TRANSLATE("Secondary identifier (optional)"),
        currentId2, new BMessage(AKDLG_KEY_ID2));
    tcSecIdentifier->SetModificationMessage(new BMessage(AKDLG_KEY_MODIFIED));
    tcData = new BTextControl("tc_key", B_TRANSLATE("Key"), currentData,
        new BMessage(AKDLG_KEY_DATA));
    tcData->SetModificationMessage(new BMessage(AKDLG_KEY_MODIFIED));
    tcData->TextView()->HideTyping(true);

    pumPurpose = new BPopUpMenu(B_TRANSLATE("Select the purpose"), B_ITEMS_IN_COLUMN);
    BLayoutBuilder::Menu<>(pumPurpose)
        .AddItem(B_TRANSLATE("Generic"), AKDLG_KEY_PUR_GEN)
        .AddItem(B_TRANSLATE("Keyring"), AKDLG_KEY_PUR_KRN)
        .AddItem(B_TRANSLATE("Web"), AKDLG_KEY_PUR_WEB)
        .AddItem(B_TRANSLATE("Network"), AKDLG_KEY_PUR_NET)
        .AddItem(B_TRANSLATE("Volume"), AKDLG_KEY_PUR_VOL)
    .End();
    mfPurpose = new BMenuField(B_TRANSLATE("Purpose"), pumPurpose);

    saveKeyButton = new BButton("bt_save", B_TRANSLATE("Save"), new BMessage(AKDLG_KEY_SAVE));
    saveKeyButton->SetEnabled(false);
    cancelButton = new BButton("bt_cancel", B_TRANSLATE("Cancel"), new BMessage(AKDLG_KEY_CANCEL));

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .SetInsets(0)
        .AddGroup(B_HORIZONTAL)
            .SetInsets(B_USE_WINDOW_INSETS)
            .Add(intro)
        .End()
        .AddGrid()
            .SetInsets(B_USE_WINDOW_INSETS)
            .AddMenuField(mfType, 0, 0)
            .AddTextControl(tcIdentifier, 0, 1)
            .AddTextControl(tcSecIdentifier, 0, 2)
            .AddTextControl(tcData, 0, 3)
            .AddMenuField(mfPurpose, 0, 4)
        .End()
        .Add(new BSeparatorView())
        .AddGroup(B_HORIZONTAL)
            .SetInsets(B_USE_WINDOW_INSETS)
            .AddGlue()
            .Add(cancelButton)
            .Add(saveKeyButton)
        .End()
    .End();

    CenterOnScreen();
}

void AddKeyDialogBox::MessageReceived(BMessage* msg)
{
    switch (msg->what)
    {
        case AKDLG_KEY_ID:
            tcIdentifier->MarkAsInvalid(_IsValid(BString(tcIdentifier->Text())) != B_OK);
            break;
        case AKDLG_KEY_ID2:
            // tcSecIdentifier->MarkAsInvalid(false);
            break;
        case AKDLG_KEY_DATA:
            tcData->MarkAsInvalid(_IsValid(BString(tcData->Text())) != B_OK);
            break;
        case AKDLG_KEY_MODIFIED:
            currentId = tcIdentifier->Text();
            currentId2 = tcSecIdentifier->Text();
            currentData = tcData->Text();

            saveKeyButton->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_TYP_GEN:
            currentDesiredType = B_KEY_TYPE_GENERIC;
            saveKeyButton->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_TYP_PWD:
            currentDesiredType = B_KEY_TYPE_PASSWORD;
            saveKeyButton->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_PUR_GEN:
            currentPurpose = B_KEY_PURPOSE_GENERIC;
            saveKeyButton->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_PUR_KRN:
            currentPurpose = B_KEY_PURPOSE_KEYRING;
            saveKeyButton->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_PUR_WEB:
            currentPurpose = B_KEY_PURPOSE_WEB;
            saveKeyButton->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_PUR_NET:
            currentPurpose = B_KEY_PURPOSE_NETWORK;
            saveKeyButton->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_PUR_VOL:
            currentPurpose = B_KEY_PURPOSE_VOLUME;
            saveKeyButton->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_SAVE:
            if(_IsAbleToSave()) {
                _SaveKey();
                QuitRequested();
            }
            else
                fprintf(stderr, "Error: invalid data.\n");
            break;
        case AKDLG_KEY_CANCEL:
            QuitRequested();
            break;
        default:
            BWindow::MessageReceived(msg);
            break;
    }
}

bool AddKeyDialogBox::QuitRequested()
{
    Quit();
    return true;
}

status_t AddKeyDialogBox::_IsValid(BString info)
{
    if(info.IsEmpty())
        return B_BAD_VALUE;
    else
        return B_OK;
}

bool AddKeyDialogBox::_IsAbleToSave()
{
    return _IsValid(currentId)       == B_OK           &&
           _IsValid(currentData)     == B_OK           &&
           currentDesiredType        >  B_KEY_TYPE_ANY &&
           currentPurpose            >  B_KEY_PURPOSE_ANY;
}

void AddKeyDialogBox::_SaveKey()
{
    BKeyStore keystore;

    if(currentDesiredType == B_KEY_TYPE_PASSWORD) {
        BPasswordKey key(currentData.String(), currentPurpose,
            currentId.String(), currentId2 == "" ? NULL : currentId2.String());
        keystore.AddKey(keyring, key);
    }
    else {
        BKey key(currentPurpose, currentId.String(),
            currentId2 == "" ? NULL : currentId2.String(),
            reinterpret_cast<const uint8*>(currentData.String()),
            strlen(currentData.String()));
        keystore.AddKey(keyring, key);
    }

    BMessenger msgr(be_app_messenger);
    BMessage savemsg(M_USER_PROVIDES_KEY);
    savemsg.AddString("owner", keyring);
    msgr.SendMessage(&savemsg, msgr);
}
