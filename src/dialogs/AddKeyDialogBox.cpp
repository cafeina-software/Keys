/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <Application.h>
#include <Catalog.h>
#include <KeyStore.h>
#include <private/interface/Spinner.h>
#include <pwd.h>
#include <unistd.h>
#include <ctime>
#include "AddKeyDialogBox.h"
#include "../KeysDefs.h"
#include "../data/KeystoreImp.h"
#include "../data/PasswordStrength.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Add key dialog box"

AddKeyDialogBox::AddKeyDialogBox(BWindow* parent, BRect frame,
    const char* keyringname, BKeyType desiredType, BView* view, AKDlgModel dialogType)
: BWindow(frame, "", B_FLOATING_WINDOW_LOOK, B_FLOATING_ALL_WINDOW_FEEL,
    B_NOT_ZOOMABLE|B_NOT_RESIZABLE|B_AUTO_UPDATE_SIZE_LIMITS),
    //ks(_ks),
    keyring(keyringname),
    currentId(""),
    currentId2(""),
    currentData(""),
    currentPurpose(B_KEY_PURPOSE_ANY),
    currentDesiredType(desiredType),
    parentWindow(parent),
    containerView(view),
    dialogModel(dialogType)
{
    fPumType = new BPopUpMenu(B_TRANSLATE("Type of key"), B_ITEMS_IN_COLUMN);
    BLayoutBuilder::Menu<>(fPumType)
        .AddItem(B_TRANSLATE("Generic key"), AKDLG_KEY_TYP_GEN)
        .AddItem(B_TRANSLATE("Password key"), AKDLG_KEY_TYP_PWD)
        .AddItem(B_TRANSLATE("Certificate key"), AKDLG_KEY_TYP_CRT)
    .End();

    fPumPurpose = new BPopUpMenu(B_TRANSLATE("Select the purpose"), B_ITEMS_IN_COLUMN);
    BLayoutBuilder::Menu<>(fPumPurpose)
        .AddItem(B_TRANSLATE("Generic"), AKDLG_KEY_PUR_GEN)
        .AddItem(B_TRANSLATE("Keyring"), AKDLG_KEY_PUR_KRN)
        .AddItem(B_TRANSLATE("Web"), AKDLG_KEY_PUR_WEB)
        .AddItem(B_TRANSLATE("Network"), AKDLG_KEY_PUR_NET)
        .AddItem(B_TRANSLATE("Volume"), AKDLG_KEY_PUR_VOL)
    .End();

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .SetInsets(0)
        .AddGroup(B_HORIZONTAL)
            .SetInsets(B_USE_WINDOW_INSETS)
            .Add(fSvIntro = new BStringView("sv_intro", ""))
        .End()
        .AddGrid()
            .SetInsets(B_USE_WINDOW_INSETS)
            .AddMenuField(fMfType = new BMenuField(B_TRANSLATE("Type"), fPumType), 0, 0)
            .AddMenuField(fMfPurpose = new BMenuField(B_TRANSLATE("Purpose"), fPumPurpose), 0, 1)
            .AddTextControl(fTcIdentifier = new BTextControl("tc_id", B_TRANSLATE("Identifier"), "", new BMessage(AKDLG_KEY_ID)), 0, 2)
            .AddTextControl(fTcSecIdentifier = new BTextControl("tc_id2", B_TRANSLATE("Secondary identifier (optional)"), "", new BMessage(AKDLG_KEY_ID2)), 0, 3)
            .AddTextControl(fTcData = new BTextControl("tc_key", B_TRANSLATE("Key"), "", new BMessage(AKDLG_KEY_DATA)), 0, 4)
            .AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING, 0, 5)
                .AddGlue()
                .Add(fBtKeyGen = new BButton("bt_keygen", B_TRANSLATE("Generate"), new BMessage(AKDLG_KEY_DATA_GEN)))
            .End()
            .Add(fSbPwdStrength = new BStatusBar("sb_ent"), 1, 5)
            .Add(fSvSpnInfo = new BStringView("sv_spninfo", B_TRANSLATE("Length")), 0, 6)
            .Add(fSpnLength = new BSpinner("sp_len", NULL, new BMessage(AKDLG_KEY_LENGTH)), 1, 6)
        .End()
        .Add(new BSeparatorView())
        .AddGroup(B_HORIZONTAL)
            .SetInsets(B_USE_WINDOW_INSETS)
            .AddGlue()
            .Add(fBtSave = new BButton("bt_save", B_TRANSLATE("Save"), new BMessage(AKDLG_KEY_SAVE)))
            .Add(fBtCancel = new BButton("bt_cancel", B_TRANSLATE("Cancel"), new BMessage(AKDLG_KEY_CANCEL)))
        .End()
    .End();

    fMfType->Menu()->FindItem(AKDLG_KEY_TYP_CRT)->SetEnabled(false);
    if(currentDesiredType == B_KEY_TYPE_PASSWORD)
        fMfType->Menu()->FindItem(AKDLG_KEY_TYP_PWD)->SetMarked(true);
    else if(currentDesiredType == B_KEY_TYPE_GENERIC)
        fMfType->Menu()->FindItem(AKDLG_KEY_TYP_GEN)->SetMarked(true);
    fTcIdentifier->SetModificationMessage(new BMessage(AKDLG_KEY_MODIFIED));
    fTcSecIdentifier->SetModificationMessage(new BMessage(AKDLG_KEY_MODIFIED));
    fTcData->SetModificationMessage(new BMessage(AKDLG_KEY_MODIFIED));
    fTcData->TextView()->HideTyping(true);
    float sz = fTcData->Frame().Height();
    fBtKeyGen->SetExplicitMaxSize(BSize(sz * 3, sz));
    fSbPwdStrength->SetBarHeight(fTcData->Bounds().Height() / 2.0f);
    fSbPwdStrength->SetMaxValue(1.0f);
    fBtSave->SetEnabled(_IsAbleToSave());

    switch(dialogModel) {
        case AKDM_STANDARD:
        {
            SetTitle(B_TRANSLATE("Add new key"));

            BString desc(B_TRANSLATE("Adding key to \"%desc%\" keyring."));
            desc.ReplaceAll("%desc%", keyring);
            fSvIntro->SetText(desc.String());

            fSvSpnInfo->Hide();
            fSpnLength->Hide();
            break;
        }
        case AKDM_UNLOCK_KEY:
        {
            SetTitle(B_TRANSLATE("Set unlock key for keyring"));

            BString desc(B_TRANSLATE("Adding unlock key to \"%desc%\" keyring."));
            desc.ReplaceAll("%desc%", keyring);
            fSvIntro->SetText(desc.String());

            fMfType->Hide();
            fMfPurpose->Hide();
            fTcIdentifier->Hide();
            fTcSecIdentifier->Hide();
            fSvSpnInfo->Hide();
            fSpnLength->Hide();
            break;
        }
        case AKDM_KEY_GEN:
        {
            SetTitle(B_TRANSLATE("Generate new key"));

            BString desc(B_TRANSLATE("Generating key for \"%desc%\" keyring."));
            desc.ReplaceAll("%desc%", keyring);
            fSvIntro->SetText(desc.String());

            fMfType->Hide();
            fTcData->Hide();
            fSbPwdStrength->Hide();
            fBtKeyGen->Hide();
            break;
        }
    }

    CenterIn(parentWindow->Frame());
}

void AddKeyDialogBox::MessageReceived(BMessage* msg)
{
    switch (msg->what)
    {
        case AKDLG_KEY_ID:
            fTcIdentifier->MarkAsInvalid(fTcIdentifier->TextLength() == 0);
            break;
        case AKDLG_KEY_ID2:
            // tcSecIdentifier->MarkAsInvalid(false);
            break;
        case AKDLG_KEY_DATA:
            fTcData->MarkAsInvalid(fTcData->TextLength() == 0);
            break;
        case AKDLG_KEY_DATA_GEN:
        {
            BPasswordKey dummy("", currentPurpose, fTcData->Text());
            status_t status = GeneratePassword(dummy, 24, 0);
            if(status == B_OK) {
                fTcData->SetText(dummy.Password());
                _UpdateStatusBar(fSbPwdStrength, fTcData->Text());
                fBtSave->SetEnabled(_IsAbleToSave());
            }
            break;
        }
        case AKDLG_KEY_MODIFIED:
            // currentId = fTcIdentifier->Text();
            // currentId2 = fTcSecIdentifier->Text();
            // currentData = fTcData->Text();
            _UpdateStatusBar(fSbPwdStrength, fTcData->Text());
            fBtSave->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_TYP_GEN:
            currentDesiredType = B_KEY_TYPE_GENERIC;
            fBtSave->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_TYP_PWD:
            currentDesiredType = B_KEY_TYPE_PASSWORD;
            fBtSave->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_PUR_GEN:
            currentPurpose = B_KEY_PURPOSE_GENERIC;
            fBtSave->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_PUR_KRN:
            currentPurpose = B_KEY_PURPOSE_KEYRING;
            fBtSave->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_PUR_WEB:
            currentPurpose = B_KEY_PURPOSE_WEB;
            fBtSave->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_PUR_NET:
            currentPurpose = B_KEY_PURPOSE_NETWORK;
            fBtSave->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_PUR_VOL:
            currentPurpose = B_KEY_PURPOSE_VOLUME;
            fBtSave->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_LENGTH:
            fBtSave->SetEnabled(_IsAbleToSave());
            break;
        case AKDLG_KEY_SAVE:
            if(_IsAbleToSave()) {
                _SaveKey();
                QuitRequested();
            }
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
    switch(dialogModel) {
        case AKDM_UNLOCK_KEY:
            return fTcData->TextLength()       > 0;
        case AKDM_KEY_GEN:
            return currentPurpose              > B_KEY_PURPOSE_ANY &&
                   fTcIdentifier->TextLength() > 0                 &&
                   fSpnLength->Value()         > 1;
        case AKDM_STANDARD:
        default:
            return fTcIdentifier->TextLength() > 0                 &&
                   fTcData->TextLength()       > 0                 &&
                   currentDesiredType          > B_KEY_TYPE_ANY    &&
                   currentPurpose              > B_KEY_PURPOSE_ANY;
    }
}

void AddKeyDialogBox::_SaveKey()
{
    BMessage request(I_KEY_ADD);
    request.AddUInt32(kConfigWhat, dialogModel == AKDM_UNLOCK_KEY ? M_KEYRING_SET_LOCKKEY :
        dialogModel == AKDM_KEY_GEN ? M_KEY_GENERATE_PASSWORD : M_KEY_CREATE);
    request.AddPointer(kConfigWho, containerView);
    request.AddString(kConfigKeyring, keyring);
    request.AddString(kConfigKeyName, dialogModel == AKDM_UNLOCK_KEY ? "" : fTcIdentifier->Text());
    request.AddString(kConfigKeyAltName, dialogModel == AKDM_UNLOCK_KEY ? "" : fTcSecIdentifier->Text());
    request.AddUInt32(kConfigKeyType, dialogModel != AKDM_STANDARD ? (uint32)B_KEY_TYPE_PASSWORD : (uint32)currentDesiredType);
    request.AddUInt32(kConfigKeyPurpose,dialogModel == AKDM_UNLOCK_KEY ? (uint32)B_KEY_PURPOSE_KEYRING : (uint32)currentPurpose);
    if(dialogModel == AKDM_KEY_GEN)
        request.AddUInt32(kConfigKeyGenLength, (uint32)fSpnLength->Value());
    request.AddInt64(kConfigKeyCreated, std::time(nullptr)); // not used by the API
    BString owner;
    passwd* pwd = getpwuid(geteuid());
    if(pwd) owner.SetTo(pwd->pw_name);
    else owner.SetTo("");
    request.AddString(kConfigKeyOwner, owner.String());  // not used by the API
    /* Somehow this has to be the last entry, otherwise it will be polluted by
        bad data (eating up the entry name of the next object added). Some kind
        of bug or just bad implementation? */
    request.AddData(kConfigKeyData, B_RAW_TYPE,
        dialogModel == AKDM_KEY_GEN ? "" : reinterpret_cast<const void*>(fTcData->Text()),
        dialogModel == AKDM_KEY_GEN ? 0 : static_cast<ssize_t>(strlen(fTcData->Text())));
    parentWindow->PostMessage(&request, parentWindow);
}

void AddKeyDialogBox::_UpdateStatusBar(BStatusBar* bar, const char* pwd)
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
        bar->SetBarColor({255, 255, 0}); //(ui_color(B_TOOL_TIP_BACKGROUND_COLOR))
    else
        bar->SetBarColor(ui_color(B_SUCCESS_COLOR));
}
