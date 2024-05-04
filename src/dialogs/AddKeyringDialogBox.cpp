/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <Application.h>
#include <Catalog.h>
#include "AddKeyringDialogBox.h"
#include "KeysDefs.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Add keyring dialog box"

AddKeyringDialogBox::AddKeyringDialogBox(BRect frame, KeystoreImp& _ks)
: BWindow(frame, B_TRANSLATE("Create keyring"), B_FLOATING_WINDOW_LOOK,
    B_FLOATING_ALL_WINDOW_FEEL,
    B_NOT_ZOOMABLE|B_NOT_RESIZABLE|B_AUTO_UPDATE_SIZE_LIMITS|B_CLOSE_ON_ESCAPE),
    ks(&_ks),
    currentKeyringName("")
{
    BString description(
        B_TRANSLATE("Write the desired name for the new keyring below.\n\n"
        "If a keyring with the written name already exists, this procedure\n"
        "will fail. In addition, the name \"%inv%\" is reserved for the main \n"
        "system keyring and cannot be used."));
    description.ReplaceAll("%inv%", "Master");
    BStringView* svDescription = new BStringView("sv_desc", description);

    tcKeyringName = new BTextControl("tc_krname", B_TRANSLATE("Keyring name"),
        currentKeyringName, new BMessage(AKRDLG_NAME_CHANGED));

    errorfont.SetFace(B_BOLD_FACE);

    svErrorDesc = new BStringView("sv_error", "\xE2\x9C\x98");
    svErrorDesc->SetFont(&errorfont);
    svErrorDesc->SetHighColor(tint_color({255, 0, 0}, B_DARKEN_1_TINT));

    saveButton = new BButton("bt_save", B_TRANSLATE("Save"), new BMessage(AKRDLG_SAVE));
    saveButton->SetEnabled(false);
    cancelButton = new BButton("bt_cancel", B_TRANSLATE("Cancel"), new BMessage(AKRDLG_CANCEL));

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .SetInsets(0)
        .AddGroup(B_VERTICAL)
            .SetInsets(B_USE_WINDOW_INSETS)
            .Add(svDescription)
            .AddStrut(4.0f)
            .AddGroup(B_HORIZONTAL)
                .Add(tcKeyringName)
                .Add(svErrorDesc)
            .End()
        .End()
        .Add(new BSeparatorView())
        .AddGroup(B_HORIZONTAL)
            .SetInsets(B_USE_WINDOW_INSETS)
            .AddGlue()
            .Add(saveButton)
            .Add(cancelButton)
        .End()
    .End();

    CenterOnScreen();
}

void AddKeyringDialogBox::MessageReceived(BMessage* msg)
{
    switch(msg->what)
    {
        case AKRDLG_NAME_CHANGED:
        {
            status_t status;
            if((status = _IsValid(*ks, tcKeyringName->Text())) != B_OK) {
                _UpdateTextControlUI(true, false, "\xE2\x9C\x98",
                    status == B_NOT_ALLOWED ?
                    B_TRANSLATE("Error: the name \"Master\" cannot be used") :
                    status == B_BAD_VALUE ?
                    B_TRANSLATE("Error: the name cannot be empty") :
                    status == B_NAME_IN_USE ?
                    B_TRANSLATE("Error: the name is in use") :
                    B_TRANSLATE("Error"),
                    tint_color({255, 0, 0}, B_DARKEN_1_TINT));
            }
            else {
                currentKeyringName = tcKeyringName->Text();

                _UpdateTextControlUI(false, true, "\xE2\x9C\x94",
                    B_TRANSLATE("Valid"), tint_color({0, 192, 0}, B_DARKEN_1_TINT));
            }
            break;
        }
        case AKRDLG_SAVE:
        {
            _CallAddKeyring(*ks, currentKeyringName);
            Quit();
            break;
        }
        case AKRDLG_CANCEL:
            Quit();
            break;
        default:
            BWindow::MessageReceived(msg);
            break;
    }
}

status_t AddKeyringDialogBox::_IsValid(KeystoreImp ks, BString name)
{
    status_t status = B_OK;

    if(name == "Master")
        status = B_NOT_ALLOWED;
    else if(name == "")
        status = B_BAD_VALUE;
    else if(ks.KeyringByName(name) != NULL)
        status = B_NAME_IN_USE;

    return status;
}

void AddKeyringDialogBox::_UpdateTextControlUI(bool tcinvalid, bool saveenabled,
    BString erroricon, BString errortooltip, rgb_color errorcolor)
{
    tcKeyringName->MarkAsInvalid(tcinvalid);
    saveButton->SetEnabled(saveenabled);

    svErrorDesc->SetText(erroricon.String());
    svErrorDesc->SetHighColor(errorcolor);
    svErrorDesc->SetToolTip(errortooltip);
    svErrorDesc->ShowToolTip(svErrorDesc->ToolTip());
}

void AddKeyringDialogBox::_CallAddKeyring(KeystoreImp& ks, BString name)
{
    status_t status = ks.CreateKeyring(name);

    if(status == B_OK) {
        BMessage addkrmsg(M_USER_ADDS_KEYRING);
        addkrmsg.AddString("owner", name);
        be_app->PostMessage(&addkrmsg);
    }
}
