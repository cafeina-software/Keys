/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <Application.h>
#include <Catalog.h>
#include <cstdio>
#include "AddKeyDialogBox.h"
#include "AddKeyringDialogBox.h"
#include "KeysDefs.h"
#include "KeysWindow.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Main window"

KeysWindow::KeysWindow(BRect frame, KeystoreImp* ks, BKeyStore* _keystore)
: BWindow(frame, B_TRANSLATE_SYSTEM_NAME(kAppName), B_TITLED_WINDOW, B_QUIT_ON_WINDOW_CLOSE),
  _ks(ks),
  keystore(_keystore),
  uist(S_UI_NO_KEYRING_IN_FOCUS)
{
    mbMain = _InitMenu();

    listView = new BListView("lv_main", B_SINGLE_SELECTION_LIST);
    listView->SetSelectionMessage(new BMessage(I_SELECTED));
    listView->SetExplicitMaxSize(BSize(listView->StringWidth("Master") * 3, B_SIZE_UNSET));
    BScrollView *listScroll = new BScrollView("sc_main", listView, 0, false, true);

    cardView = new BCardView("cv_main");

    _InitAppData(ks);

    addKeyringButton = new BButton(NULL, "+", new BMessage(I_KEYRING_ADD));
    addKeyringButton->SetToolTip(B_TRANSLATE("Add keyring"));
    addKeyringButton->SetExplicitMinSize(BSize(listScroll->Bounds().Width() / 2, 32));
    removeKeyringButton = new BButton(NULL, "-", new BMessage(I_KEYRING_REMOVE));
    removeKeyringButton->SetToolTip(B_TRANSLATE("Remove keyring"));
    removeKeyringButton->SetEnabled(false);
    removeKeyringButton->SetExplicitMinSize(BSize(listScroll->Bounds().Width() / 2, 32));

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .SetInsets(0)
        .Add(mbMain)
        .AddGroup(B_VERTICAL)
            .SetInsets(B_USE_WINDOW_INSETS)
            .AddSplit(B_HORIZONTAL, 4.0f)
                .AddGroup(B_VERTICAL)
                    .Add(listScroll)
                    .AddGrid(1.0f)
                        .Add(addKeyringButton, 0, 0)
                        .Add(removeKeyringButton, 1, 0)
                    .End()
                .End()
                .Add(cardView)
            .End()
        .End()
    .End();
}

void KeysWindow::MessageReceived(BMessage* msg)
{
    switch(msg->what)
    {
        case I_SERVER_RESTART:
            be_app->PostMessage(msg);
            break;
        case I_ABOUT:
            be_app->PostMessage(B_ABOUT_REQUESTED);
            break;
        case I_QUIT:
            be_app->PostMessage(B_QUIT_REQUESTED);
            break;
        case I_SELECTED:
        {
            int32 index = msg->GetInt32("index", 0);
            if(index >= 0)
                cardView->CardLayout()->SetVisibleItem(index);

            SetUIStatus(S_UI_HAS_KEYRING_IN_FOCUS);
            break;
        }
        case I_KEYSTORE_INFO:
        {
            int keyringc = _ks->KeyringCount();

            BString desc;
            desc.SetToFormat(
                B_TRANSLATE("Keystore.\n\n%d keyring(s).\n"),
                keyringc);

            BAlert *alert = new BAlert();
            alert->SetType(B_INFO_ALERT);
            alert->SetTitle(B_TRANSLATE("Keystore information"));
            alert->SetText(desc);
            alert->AddButton(B_TRANSLATE("OK"), B_ENTER | B_ESCAPE);
            alert->Go();
            break;
        }

        case I_KEYRING_ADD:
            _AddKeyring(_ks);
            break;
        case I_KEYRING_REMOVE:
        {
            BAlert *alert = new BAlert();

            if(strcmp(currentKeyring, "Master") == 0) {
                alert->SetType(B_STOP_ALERT);
                alert->SetTitle(B_TRANSLATE("Remove keyring: not allowed"));
                alert->SetText(B_TRANSLATE("The Master keyring cannot be removed."));
                alert->AddButton(B_TRANSLATE("OK"), B_ENTER | B_ESCAPE);
                alert->Go();
            }
            else {
                alert->SetType(B_WARNING_ALERT);
                alert->SetTitle(B_TRANSLATE("Remove keyring"));
                alert->SetText(B_TRANSLATE("Do you want to remove this keyring "
                               "and all of its keys? This action "
                               "cannot be undone."));
                alert->AddButton(B_TRANSLATE("Remove"));
                alert->AddButton(B_TRANSLATE("Cancel"), B_ESCAPE);

                int32 result = alert->Go();
                if(result == 0) {
                    if(_RemoveKeyring(_ks, currentKeyring) != B_OK)
                        fprintf(stderr, "error trying to remove keyring \"%s\"\n",
                            currentKeyring);
                }
            }

            break;
        }
        case I_KEYRING_INFO:
        {
            int gkeyc = _ks->KeyringByName(currentKeyring)->KeyCount(B_KEY_TYPE_GENERIC);
            int pkeyc = _ks->KeyringByName(currentKeyring)->KeyCount(B_KEY_TYPE_PASSWORD);
            int keyc = gkeyc + pkeyc;
            int appc = _ks->KeyringByName(currentKeyring)->ApplicationCount();

            BString desc;
            desc.SetToFormat(B_TRANSLATE("Keyring: %s.\n\n"
                "%d key(s): %d generic-type, %d password-type.\n"
                "%d application(s)."),
                currentKeyring, keyc, gkeyc, pkeyc, appc);

            BAlert *alert = new BAlert();
            alert->SetType(B_INFO_ALERT);
            alert->SetTitle(B_TRANSLATE("Keyring information"));
            alert->SetText(desc);
            alert->AddButton(B_TRANSLATE("OK"), B_ENTER | B_ESCAPE);
            alert->Go();
            break;
        }
        case I_KEY_ADD_GENERIC:
        {
            const char* keyringname = ((BStringItem*)listView->ItemAt(listView->CurrentSelection()))->Text();
            AddKeyDialogBox* dlg = new AddKeyDialogBox(BRect(20, 20, 200, 200), _ks,
                currentKeyring, B_KEY_TYPE_GENERIC);
            dlg->Show();
            break;
        }
        case I_KEY_ADD_PASSWORD:
        {
            const char* keyringname = ((BStringItem*)listView->ItemAt(listView->CurrentSelection()))->Text();
            AddKeyDialogBox* dlg = new AddKeyDialogBox(BRect(20, 20, 200, 200), _ks,
                currentKeyring, B_KEY_TYPE_PASSWORD);
            dlg->Show();
            break;
        }
        case I_KEY_ADD_CERT:
        {
            BAlert* alert = new BAlert(B_TRANSLATE("New key: error"),
                B_TRANSLATE("The Key Storage API currently does not support "
                "creating keys of type \"Certificate\"."),
                B_TRANSLATE("OK"));
            alert->SetShortcut(0, B_ENTER | B_ESCAPE);
            alert->Go();
            break;
        }
        case M_USER_PROVIDES_KEY:
        case M_USER_REMOVES_KEY:
        case M_USER_REMOVES_APPAUTH:
        {
            BString owner;
            if(msg->FindString("owner", &owner) == B_OK) {
                // fprintf(stderr, "Received task for %s\n", owner.String());
                bool found = false;
                auto it = keyringviewlist.begin();
                while(it != keyringviewlist.end() && !found) {
                    // fprintf(stderr, "Current iteration: %s\n", (*it)->Name());
                    // fprintf(stderr, "Pre-test: %s <=> %s\n", (*it)->Name(), owner.String());
                    if(strcmp((*it)->Name(), owner.String()) == 0)
                        found = true;
                    else
                        ++it;
                }
                if(*it) {
                    fprintf(stderr, "Updating view: %s\n", (*it)->Name());
                    (*it)->Update();
                }
            }
            break;
        }
        default:
            BWindow::MessageReceived(msg);
            break;
    }
}

void KeysWindow::SetUIStatus(ui_status status)
{
    LockLooper();

    switch(status)
    {
        case S_UI_NO_KEYRING_IN_FOCUS:
            fRemKeyring->SetEnabled(false);
            fMenuKeyring->SetEnabled(false);
            removeKeyringButton->SetEnabled(false);
            currentKeyring = NULL;
            break;
        case S_UI_HAS_KEYRING_IN_FOCUS:
            fRemKeyring->SetEnabled(true);
            fMenuKeyring->SetEnabled(true);
            removeKeyringButton->SetEnabled(true);
            currentKeyring = ((BStringItem*)listView->ItemAt(listView->CurrentSelection()))->Text();
            break;
        default:
            break;
    }

    UnlockLooper();
}

ui_status KeysWindow::GetUIStatus()
{
    return uist;
}

void KeysWindow::Update(const void* data)
{
    _FullReload(_ks);
    SetUIStatus(S_UI_NO_KEYRING_IN_FOCUS);
}

// #pragma mark -

void KeysWindow::_InitAppData(KeystoreImp* ks)
{
    LockLooper();

    for(int i = 0; i < ks->KeyringCount(); i++) {
        KeyringView* view = new KeyringView(BRect(0, 0, 100, 100),
            ks->KeyringAt(i)->Identifier(), ks);
        keyringviewlist.push_back(view);
        listView->AddItem(new BStringItem(ks->KeyringAt(i)->Identifier()));
        cardView->AddChild(view);
    }

    UnlockLooper();
}

void KeysWindow::_FullReload(KeystoreImp* ks)
{
    LockLooper();

    listView->MakeEmpty();
    for(auto& view : keyringviewlist) {
        cardView->RemoveChild(view);
        delete view;
    }
    keyringviewlist.clear();

    UnlockLooper();

    _InitAppData(ks);
}

status_t KeysWindow::_AddKeyring(KeystoreImp* _ks)
{
    status_t status = B_OK;

    AddKeyringDialogBox *dlg = new AddKeyringDialogBox(BRect(0, 0, 100, 100),
        *_ks);
    if(!dlg)
        return B_ERROR;
    dlg->Show();

    return status;
}

status_t KeysWindow::_RemoveKeyring(KeystoreImp* _ks, const char* target)
{
    status_t status = _ks->RemoveKeyring(target);

    if(status == B_OK) {
        _FullReload(_ks);
        SetUIStatus(S_UI_NO_KEYRING_IN_FOCUS);
    }

    return status;
}

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Main menu"

BMenuBar* KeysWindow::_InitMenu()
{
    BMenuBar* menu = new BMenuBar("mb_main");
    BLayoutBuilder::Menu<>(menu)
        .AddMenu(B_TRANSLATE(kAppName))
            .AddItem(B_TRANSLATE("Restart keystore server"), I_SERVER_RESTART)
            .AddSeparator()
            .AddItem(B_TRANSLATE("About" B_UTF8_ELLIPSIS), I_ABOUT)
            .AddSeparator()
            .AddItem(B_TRANSLATE("Quit"), I_QUIT, 'Q')
        .End()
        .AddMenu(B_TRANSLATE("Keystore"))
            .AddItem(B_TRANSLATE("Create keyring" B_UTF8_ELLIPSIS), I_KEYRING_ADD, '+')
            .AddItem(B_TRANSLATE("Remove keyring"), I_KEYRING_REMOVE, '-')
            .AddSeparator()
            .AddItem(B_TRANSLATE("Keystore statistics" B_UTF8_ELLIPSIS), I_KEYSTORE_INFO)
        .End()
        .AddMenu(B_TRANSLATE("Keyring"))
            .AddMenu(B_TRANSLATE("Create key"))
                .AddItem(B_TRANSLATE("Generic key"), I_KEY_ADD_GENERIC)
                .AddItem(B_TRANSLATE("Password key"), I_KEY_ADD_PASSWORD)
                .AddItem(B_TRANSLATE("Certificate key"), I_KEY_ADD_CERT)
            .End()
            .AddSeparator()
            .AddItem(B_TRANSLATE("Keyring statistics" B_UTF8_ELLIPSIS), I_KEYRING_INFO, 'I')
        .End()
    .End();
    menu->FindItem(B_TRANSLATE("Remove keyring"))->SetEnabled(false);
    menu->FindItem(B_TRANSLATE("Keyring"))->SetEnabled(false);

    fRemKeyring = menu->FindItem(B_TRANSLATE("Remove keyring"));
    fMenuKeyring = menu->FindItem(B_TRANSLATE("Keyring"));

    return menu;
}
