/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <Application.h>
#include <Catalog.h>
#include <FindDirectory.h>
#include <Path.h>
#include <cstdio>
#include "../dialogs/AddKeyDialogBox.h"
#include "../dialogs/AddUnlockKeyDialogBox.h"
#include "../dialogs/AddKeyringDialogBox.h"
#include "../dialogs/KeyringViewerDialogBox.h"
#include "../dialogs/BackUpDBDialogBox.h"
#include "../KeysDefs.h"
#include "KeysWindow.h"
#include "ListViewEx.h"

BStringItem* find_item(BListView* view, const char* text) {
    BStringItem* item = nullptr;
    bool found = false;
    for(int i = 0; i < view->CountItems() && !found; i++) {
        if(strcmp(((BStringItem*)view->ItemAt(i))->Text(), text) == 0) {
            item = (BStringItem*)view->ItemAt(i);
            found = true;
        }
    }
    return item;
}

void make_string_filename_friendly(const char* instr, size_t length, char*& outstr) {
    for(int i = 0; i < static_cast<int>(length); i++) {
        if(instr[i] == '/' || instr[i] == ':' || instr[i] == '\\' ||
        instr[i] == '\'' || instr[i] == '\"')
            outstr[i] = '-';

        else if(instr[i] == '\0')
            break;
        else
            outstr[i] = instr[i];
    }
}

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Main window"

KeysWindow::KeysWindow(BRect frame, KeystoreImp* _ks, BKeyStore* _keystore)
: BWindow(frame, B_TRANSLATE_SYSTEM_NAME(kAppName), B_TITLED_WINDOW, B_QUIT_ON_WINDOW_CLOSE),
  keystore(_keystore),
  ks(_ks),
  uist(S_UI_NO_KEYRING_IN_FOCUS),
  fFilter(new KeyMsgRefFilter),
  openPanel(nullptr),
  savePanel(nullptr),
  fRemKeyring(nullptr),
  fIsLockedKeyring(nullptr),
  fMenuKeyring(nullptr)
{
    mbMain = _InitMenu();
    fKeystorePopMenu = _InitKeystorePopUpMenu();
    fKeyringItemMenu = _InitKeyringPopUpMenu();

    listView = new BListViewEx("lv_main", B_SINGLE_SELECTION_LIST,
        B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE, fKeystorePopMenu, fKeyringItemMenu);
    listView->SetSelectionMessage(new BMessage(I_SELECTED));
    listView->SetInvocationMessage(new BMessage(I_KEYRING_INFO));
    listView->SetExplicitMaxSize(BSize(listView->StringWidth("Master") * 3, B_SIZE_UNSET));
    BScrollView *listScroll = new BScrollView("sc_main", listView, 0, false, true);

    cardView = new BCardView("cv_main");

    _InitAppData(ks);

    addKeyringButton = new BButton("bt_addkr", "+", new BMessage(I_KEYRING_ADD));
    addKeyringButton->SetToolTip(B_TRANSLATE("Add keyring"));
    addKeyringButton->SetExplicitMinSize(BSize(listScroll->Bounds().Width() / 2, 32));
    removeKeyringButton = new BButton("bt_rmkr", "-", new BMessage(I_KEYRING_REMOVE));
    removeKeyringButton->SetToolTip(B_TRANSLATE("Remove keyring"));
    removeKeyringButton->SetEnabled(false);
    removeKeyringButton->SetExplicitMinSize(BSize(listScroll->Bounds().Width() / 2, 32));

    /* Fields for file panels */
    BMessenger msgr(this);
    BPath path;
    find_directory(B_USER_DIRECTORY, &path);
    BEntry entry(path.Path());
    entry_ref ref;
    entry.GetRef(&ref);

    openPanel = new BFilePanel(B_OPEN_PANEL, &msgr, &ref, B_FILE_NODE,
        false, NULL, fFilter, false, true);
    openPanel->SetButtonLabel(B_DEFAULT_BUTTON, B_TRANSLATE("Import key"));
    openPanel->SetButtonLabel(B_CANCEL_BUTTON, B_TRANSLATE("Cancel"));

    savePanel = new BFilePanel(B_SAVE_PANEL, &msgr, &ref, B_FILE_NODE,
        false, NULL, fFilter, false, true);
    savePanel->SetButtonLabel(B_DEFAULT_BUTTON, B_TRANSLATE("Export key"));
    savePanel->SetButtonLabel(B_CANCEL_BUTTON, B_TRANSLATE("Cancel"));

    /* Layout kit */
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

KeysWindow::~KeysWindow()
{
    if(savePanel)
        delete savePanel;
    if(openPanel)
        delete openPanel;
    if(fFilter)
        delete fFilter;
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
            // Basic approach (requires items not being reordered to not show the wrong view)
            int32 index = msg->GetInt32("index", 0);
            if(index >= 0 && index < cardView->CountChildren())
                cardView->CardLayout()->SetVisibleItem(index);

            /* // Alternative approach (safer, allows separated insertion of
               //   list items and child views, but is more expensive):
            const char* sel = ((BStringItem*)listView->ItemAt(listView->CurrentSelection()))->Text();
            int items = cardView->CardLayout()->CountItems();
            int i = 0;
            while(i < items && strcmp(cardView->CardLayout()->ItemAt(i)->View()->Name(), sel) != 0)
                i++;

            BLayoutItem* li = cardView->CardLayout()->ItemAt(i);
            if(li) {
                int ix = cardView->CardLayout()->IndexOfItem(li);
                cardView->CardLayout()->SetVisibleItem(ix);
            }
            */

            SetUIStatus(S_UI_HAS_KEYRING_IN_FOCUS);
            break;
        }
        case I_KEYSTORE_BACKUP:
        {
            BackUpDBDialogBox* dlg = new BackUpDBDialogBox(this, BRect());
            dlg->Show();
            break;
        }
        case I_KEYSTORE_CLEAR:
        {
            if((new BAlert(B_TRANSLATE("Wipe keystore"),
            B_TRANSLATE("Do you want to clean the keystore database?\n\nThis will "
            "delete all the keyrings and all of their keys, as well as deleting "
            "all the keys of \"Master\" keyring (however this keyring will not "
            "be deleted).\n\nThis operation cannot be undone."),
            B_TRANSLATE("Delete all"), B_TRANSLATE("Cancel"), NULL,
            B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go() == 0) {
                BMessage request(M_KEYSTORE_WIPE_CONTENTS);
                be_app->PostMessage(&request);
            }
            break;
        }
        case I_KEYSTORE_INFO:
            _KeystoreInfo();
            break;
        case I_KEYRING_ADD:
            _AddKeyring();
            break;
        case I_KEYRING_REMOVE:
            _RemoveKeyring();
            break;
        case I_KEYRING_LOCK:
            _LockKeyring();
            break;
        case I_KEYRING_UNLOCKKEY_ADD:
            _SetKeyringLockKey();
            break;
        case I_KEYRING_UNLOCKKEY_REM:
            _RemoveKeyringLockKey();
            break;
        case I_KEYRING_CLEAR:
            _ClearKeyring();
            break;
        case I_KEYRING_INFO:
            _KeyringInfo();
            break;
        case I_KEY_ADD:
        {
            BMessage request(*msg);
            request.what = M_KEY_CREATE;
            be_app->PostMessage(&request);
            break;
        }
        case I_KEY_ADD_GENERIC:
            _AddKey(B_KEY_TYPE_GENERIC);
            break;
        case I_KEY_ADD_PASSWORD:
            _AddKey(B_KEY_TYPE_PASSWORD);
            break;
        case I_KEY_ADD_CERT:
            _AddKey(B_KEY_TYPE_CERTIFICATE);
            break;
        case I_KEY_IMPORT:
        {
            if(!currentKeyring || strcmp(currentKeyring, "") == 0)
                break;

            BMessage* request = new BMessage(B_REFS_RECEIVED);
            request->AddUInt32(kConfigWhat, msg->what);
            request->AddString(kConfigKeyring, currentKeyring);

            openPanel->SetMessage(request);
            openPanel->Show();

            delete request;
            break;
        }
        case B_REFS_RECEIVED:
        {
            uint32 what;
            if(msg->FindUInt32(kConfigWhat, &what) == B_OK && what == I_KEY_IMPORT)
                _ImportKey(msg);
            break;
        }
        case I_KEY_EXPORT:
        {
            if(!currentKeyring || strcmp(currentKeyring, "") == 0)
                break;

            /* Fields from a keyring view */
            BString keyring, id;
            if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
            msg->FindString(kConfigKeyName, &id) != B_OK) {
                fprintf(stderr, "Error: invalid message.\n");
                break;
            }

            BMessage* request = new BMessage(B_SAVE_REQUESTED);
            request->AddUInt32(kConfigWhat, msg->what);
            request->AddString(kConfigKeyring, keyring.String());
            request->AddString(kConfigKeyName, id.String());

            size_t length = strlen(id.String());
            char* filename = new char[static_cast<int>(length) + 1];
            filename[static_cast<int>(length)] = '\0';
            make_string_filename_friendly(id.String(), length, filename);
            savePanel->SetSaveText(filename);
            savePanel->SetMessage(request);
            savePanel->Show();

            delete[] filename;
            delete request;
            break;
        }
        case B_SAVE_REQUESTED:
        {
            uint32 what;
            if(msg->FindUInt32(kConfigWhat, &what) == B_OK && what == I_KEY_EXPORT)
                _ExportKey(msg);
            break;
        }
        case I_KEY_REMOVE:
            _RemoveKey(msg);
            break;

        /* Notifications: update views */
        case M_KEYRING_CREATE:
            Update();
            break;
        case M_KEY_CREATE:
            if(msg->GetUInt32("result", B_OK) == B_NAME_IN_USE) {
                (new BAlert(B_TRANSLATE("Import key: error"),
                    B_TRANSLATE("Error: there is already a key with the same "
                    "identifier as the identifier in the key file."),
                    B_TRANSLATE("Close"), NULL, NULL, B_WIDTH_AS_USUAL,
                    B_STOP_ALERT))->Go();
                break;
            }
            [[fallthrough]];
        case M_KEY_IMPORT:
        case M_KEY_EXPORT:
        case M_KEY_DELETE:
        case M_APPAUTH_DELETE:
        {
            const void* ptr = msg->GetPointer(kConfigWho);
            if(ptr)
                ((KeyringView*)ptr)->Update();
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
            currentKeyring = NULL;

            fRemKeyring->SetEnabled(false);
            fMenuKeyring->SetEnabled(false);
            removeKeyringButton->SetEnabled(false);
            fIsLockedKeyring->SetMarked(false);
            break;
        case S_UI_HAS_KEYRING_IN_FOCUS:
            currentKeyring = ((BStringItem*)listView->ItemAt(listView->CurrentSelection()))->Text();

            fRemKeyring->SetEnabled(true);
            fMenuKeyring->SetEnabled(true);
            removeKeyringButton->SetEnabled(true);
            fIsLockedKeyring->SetMarked(!(ks->KeyringByName(currentKeyring)->IsUnlocked()));
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
    _FullReload(ks);
    SetUIStatus(S_UI_NO_KEYRING_IN_FOCUS);
    if(data != nullptr)
        if(ks->KeyringByName(reinterpret_cast<const char*>(data))) {
            BStringItem* item = find_item(listView, reinterpret_cast<const char*>(data));
            if(item)
                listView->Select(listView->IndexOf(item));
        }
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

KeyringView* KeysWindow::_FindKeyringView(const char* target)
{
    if(!target)
        return nullptr;

    bool found = false;
    auto it = keyringviewlist.begin();
    while(it != keyringviewlist.end() && !found) {
        if(strcmp((*it)->Name(), target) == 0)
            found = true;
        else
            ++it;
    }
    if(*it)
        return *it;
    else
        return nullptr;
}

void KeysWindow::_NotifyKeyringView(const char* target)
{
    KeyringView* view = _FindKeyringView(target);
    if(view) {
        fprintf(stderr, "Updating view: %s\n", view->Name());
        view->Update();
    }
}

// #pragma mark - Keystore management calls

void KeysWindow::_KeystoreInfo()
{
    int keyringc = ks->KeyringCount();

    BString desc;
    desc.SetToFormat(B_TRANSLATE("Keystore.\n\n%d keyring(s).\n"), keyringc);

    BAlert *alert = new BAlert();
    alert->SetType(B_INFO_ALERT);
    alert->SetTitle(B_TRANSLATE("Keystore information"));
    alert->SetText(desc);
    alert->AddButton(B_TRANSLATE("OK"), B_ENTER | B_ESCAPE);
    alert->Go();
}

status_t KeysWindow::_AddKeyring()
{
    status_t status = B_OK;

    AddKeyringDialogBox *dlg = new AddKeyringDialogBox(this, BRect(0, 0, 100, 100), *ks);
    if(!dlg)
        return B_ERROR;
    dlg->Show();

    return status;
}

status_t KeysWindow::_RemoveKeyring()
{
    if(!currentKeyring || strcmp(currentKeyring, "") == 0 || strcmp(currentKeyring, "Master") == 0) {
        BAlert *alert = new BAlert(B_TRANSLATE("Remove keyring: not allowed"),
            B_TRANSLATE_COMMENT("The Master keyring cannot be removed.", "\"Master\" is a name"),
            B_TRANSLATE("Close"), NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
        alert->SetShortcut(0, B_ENTER | B_ESCAPE);
        alert->Go();
        return B_ERROR;
    }

    BAlert *alert = new BAlert(B_TRANSLATE("Remove keyring"),
        B_TRANSLATE("Do you want to remove this keyring and all of its contents? "
        "This action cannot be undone."), B_TRANSLATE("Remove"),
        B_TRANSLATE("Cancel"), NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
    alert->SetShortcut(1, B_ESCAPE);
    int32 result = alert->Go();

    if(result == 1) // Nothing to be done if user cancels
        return B_OK;

    BMessage request(M_KEYRING_DELETE), reply;
    request.AddPointer(kConfigWho, this);
    request.AddString(kConfigKeyring, currentKeyring);
    be_app_messenger.SendMessage(&request, &reply);

    status_t status = B_ERROR;
    if(reply.FindInt32(kConfigResult, (int32*)&status) == B_OK && status == B_OK) {
        Update();
    }

    return status;
}

void KeysWindow::_LockKeyring()
{
    if(!currentKeyring || strcmp(currentKeyring, "") == 0)
        return;

    BMessage request(M_KEYRING_LOCK), reply;
    request.AddString(kConfigKeyring, currentKeyring);
    be_app_messenger.SendMessage(&request, &reply);

    status_t status = B_ERROR;
    if(reply.FindInt32(kConfigResult, (int32*)&status) == B_OK && status == B_OK)
        SetUIStatus(S_UI_HAS_KEYRING_IN_FOCUS);
}

void KeysWindow::_SetKeyringLockKey()
{
    if(!currentKeyring || strcmp(currentKeyring, "") == 0)
        return;

    AddUnlockKeyDialogBox* dlg = new AddUnlockKeyDialogBox(this, BRect(), ks, currentKeyring);
    if(dlg) dlg->Show();
}

void KeysWindow::_RemoveKeyringLockKey()
{
    if(!currentKeyring || strcmp(currentKeyring, "") == 0)
        return;

    BAlert *alert = new BAlert(B_TRANSLATE("Unlock key"),
        B_TRANSLATE("Do you want to remove this keyring's unlock key?"),
        B_TRANSLATE("Remove"), B_TRANSLATE("Cancel"), NULL, B_WIDTH_AS_USUAL,
        B_WARNING_ALERT);
    alert->SetShortcut(1, B_ESCAPE);
    int32 result = alert->Go();

    if(result == 1) // Nothing to be done if user cancels
        return /*B_OK*/;

    BMessage request(M_KEYRING_UNSET_LOCKKEY), reply;
    request.AddString(kConfigKeyring, currentKeyring);
    be_app_messenger.SendMessage(&request, &reply);

    status_t status = B_ERROR;
    if(reply.FindInt32(kConfigResult, (int32*)&status) == B_OK && status == B_OK)
        SetUIStatus(S_UI_HAS_KEYRING_IN_FOCUS);
}

void KeysWindow::_ClearKeyring()
{
    if(!currentKeyring || strcmp(currentKeyring, "") == 0)
        return;

    BAlert *alert = new BAlert(B_TRANSLATE("Clear keyring"),
        B_TRANSLATE("Do you want to delete all the keys "
        //"and application accesses "
        "from this keyring? This action cannot be undone.\n\n"
        "The keyring will not be deleted, neither the applications access "
        "list."),
        B_TRANSLATE("Clear all"), B_TRANSLATE("Cancel"), NULL, B_WIDTH_AS_USUAL,
        B_WARNING_ALERT);
    alert->SetShortcut(1, B_ESCAPE);
    int32 result = alert->Go();

    if(result == 1)
        return;

    BMessage request(M_KEYRING_WIPE_CONTENTS), reply;
    request.AddString(kConfigKeyring, currentKeyring);
    be_app_messenger.SendMessage(&request, &reply);

    status_t status = B_ERROR;
    if(reply.FindInt32(kConfigResult, (int32*)&status) == B_OK && status == B_OK)
        Update((const void*)currentKeyring);
}

void KeysWindow::_KeyringInfo()
{
    if(!currentKeyring || strcmp(currentKeyring, "") == 0)
        return;

    SetUIStatus(S_UI_HAS_KEYRING_IN_FOCUS);

    KeyringViewerDialogBox* dlg = new KeyringViewerDialogBox(this, BRect(), ks, currentKeyring);
    if(dlg) dlg->Show();
}

void KeysWindow::_AddKey(BKeyType t)
{
    if(!currentKeyring || strcmp(currentKeyring, "") == 0)
        return;

    if(t == B_KEY_TYPE_CERTIFICATE) {
        BAlert* alert = new BAlert(B_TRANSLATE("New key: error"),
            B_TRANSLATE("The Key Storage API currently does not support "
            "creating keys of type \"Certificate\"."),
            B_TRANSLATE("Close"), NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
        alert->SetShortcut(0, B_ENTER | B_ESCAPE);
        alert->Go();
        return;
    }

    AddKeyDialogBox* dlg = new AddKeyDialogBox(this, BRect(20, 20, 200, 200), ks,
        currentKeyring, t, _FindKeyringView(currentKeyring));
    if(dlg) dlg->Show();
}

void KeysWindow::_ImportKey(BMessage* msg)
{
    if(!currentKeyring || strcmp(currentKeyring, "") == 0)
        return;

    BMessage request(M_KEY_IMPORT), reply;
    request.AddString(kConfigKeyring, currentKeyring);
    entry_ref ref;
    (void)msg->FindRef("refs", &ref);
    request.AddRef("refs", &ref);
    request.AddPointer(kConfigWho, _FindKeyringView(currentKeyring));
    be_app_messenger.SendMessage(&request, &reply);

    status_t status = B_ERROR;
    if(reply.FindInt32(kConfigResult, (int32*)&status) == B_OK) {
        if(status == B_OK) {
            _NotifyKeyringView(currentKeyring);
            SetUIStatus(S_UI_HAS_KEYRING_IN_FOCUS);
        }
        else if(status == B_NAME_IN_USE) {
            (new BAlert(B_TRANSLATE("Import key: error"),
                B_TRANSLATE("Error: there is already a key with the same "
                "identifier as the identifier in the key file."),
                B_TRANSLATE("Close"), NULL, NULL, B_WIDTH_AS_USUAL,
                B_STOP_ALERT))->Go();
        }
    }
    else
        fprintf(stderr, "not found status\n");
}

void KeysWindow::_ExportKey(BMessage* msg)
{
    BString keyring(msg->GetString(kConfigKeyring));
    BString key(msg->GetString(kConfigKeyName));
    if(keyring.IsEmpty() || key.IsEmpty())
        return;

    BMessage request(M_KEY_EXPORT), reply;
    entry_ref ref;
    (void)msg->FindRef("directory", &ref);
    request.AddRef("directory", &ref);
    request.AddString("name", msg->FindString("name")); // It does not have to be equal to id
    request.AddString(kConfigKeyring, keyring.String());
    request.AddString(kConfigKeyName, key.String());
    be_app_messenger.SendMessage(&request, &reply);

    status_t status = B_ERROR;
    if(reply.FindInt32(kConfigResult, (int32*)&status) == B_OK && status == B_OK) {
        _NotifyKeyringView(currentKeyring);
        SetUIStatus(S_UI_HAS_KEYRING_IN_FOCUS);
    } else {
        (new BAlert(B_TRANSLATE("Error"),
        B_TRANSLATE("An error has occurred. The key could not be successfully exported."),
        B_TRANSLATE("Close"), NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go();
    }
}

void KeysWindow::_RemoveKey(BMessage* msg)
{
    BString keyring(msg->GetString(kConfigKeyring));
    BString key(msg->GetString(kConfigKeyName));
    if(keyring.IsEmpty() || key.IsEmpty())
        return;

    BMessage request(M_KEY_DELETE), reply;
    request.AddString(kConfigKeyring, keyring.String());
    request.AddString(kConfigKeyName, key.String());
    request.AddPointer(kConfigWho, _FindKeyringView(keyring.String()));
    be_app_messenger.SendMessage(&request, &reply);

    status_t status = B_ERROR;
    if(reply.FindInt32(kConfigResult, (int32*)&status) == B_OK && status == B_OK)
        _NotifyKeyringView(keyring.String());
}

// #pragma mark - UI creation

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
            .AddItem(B_TRANSLATE("Remove keyring" B_UTF8_ELLIPSIS), I_KEYRING_REMOVE, '-')
            .AddSeparator()
            .AddItem(B_TRANSLATE("Backup keystore database" B_UTF8_ELLIPSIS), I_KEYSTORE_BACKUP)
            .AddItem(B_TRANSLATE("Wipe keystore database" B_UTF8_ELLIPSIS), I_KEYSTORE_CLEAR)
            .AddSeparator()
            .AddItem(B_TRANSLATE("Keystore statistics" B_UTF8_ELLIPSIS), I_KEYSTORE_INFO)
        .End()
        .AddMenu(B_TRANSLATE("Keyring"))
            .AddMenu(B_TRANSLATE("Create key"))
                .AddItem(B_TRANSLATE("Generic key"), I_KEY_ADD_GENERIC, 'G')
                .AddItem(B_TRANSLATE("Password key"), I_KEY_ADD_PASSWORD, 'P')
                .AddItem(B_TRANSLATE("Certificate key"), I_KEY_ADD_CERT)
            .End()
            .AddItem(B_TRANSLATE("Import key" B_UTF8_ELLIPSIS), I_KEY_IMPORT, 'M')
            .AddSeparator()
            .AddMenu(B_TRANSLATE("Keyring lockdown"))
                .AddItem(B_TRANSLATE("Lock keyring"), I_KEYRING_LOCK)
                .AddSeparator()
                .AddItem(B_TRANSLATE("Set unlock key" B_UTF8_ELLIPSIS), I_KEYRING_UNLOCKKEY_ADD)
                .AddItem(B_TRANSLATE("Remove unlock key" B_UTF8_ELLIPSIS), I_KEYRING_UNLOCKKEY_REM)
            .End()
            .AddSeparator()
            .AddItem(B_TRANSLATE("Wipe keyring" B_UTF8_ELLIPSIS), I_KEYRING_CLEAR)
            .AddItem(B_TRANSLATE("Keyring statistics" B_UTF8_ELLIPSIS), I_KEYRING_INFO, 'I')
        .End()
    .End();

    fRemKeyring = menu->FindItem(I_KEYRING_REMOVE);
    fIsLockedKeyring = menu->FindItem(I_KEYRING_LOCK);
    fMenuKeyring = menu->FindItem(B_TRANSLATE("Keyring"));

    fRemKeyring->SetEnabled(false);
    fMenuKeyring->SetEnabled(false);

    return menu;
}

BPopUpMenu* KeysWindow::_InitKeystorePopUpMenu()
{
    BPopUpMenu* menu = new BPopUpMenu("pum_kr", false, false);
    BLayoutBuilder::Menu<>(menu)
        .AddItem(B_TRANSLATE("Create keyring" B_UTF8_ELLIPSIS), I_KEYRING_ADD, '+')
        .AddSeparator()
        .AddItem(B_TRANSLATE("Keystore statistics" B_UTF8_ELLIPSIS), I_KEYSTORE_INFO)
    .End();
    return menu;
}

BPopUpMenu* KeysWindow::_InitKeyringPopUpMenu()
{
    BPopUpMenu* menu = new BPopUpMenu("pum_kr", false, false);
    BLayoutBuilder::Menu<>(menu)
        .AddItem(B_TRANSLATE("Add generic key" B_UTF8_ELLIPSIS), I_KEY_ADD_GENERIC, 'G')
        .AddItem(B_TRANSLATE("Add password key" B_UTF8_ELLIPSIS), I_KEY_ADD_PASSWORD, 'P')
        .AddItem(B_TRANSLATE("Import key" B_UTF8_ELLIPSIS), I_KEY_IMPORT, 'M')
        .AddSeparator()
        .AddItem(B_TRANSLATE("Lock keyring"), I_KEYRING_LOCK)
        .AddItem(B_TRANSLATE("Set unlock key" B_UTF8_ELLIPSIS), I_KEYRING_UNLOCKKEY_ADD)
        .AddItem(B_TRANSLATE("Remove unlock key"), I_KEYRING_UNLOCKKEY_REM)
        .AddSeparator()
        .AddItem(B_TRANSLATE("Wipe keyring" B_UTF8_ELLIPSIS), I_KEYRING_CLEAR)
        .AddItem(B_TRANSLATE("Remove keyring" B_UTF8_ELLIPSIS), I_KEYRING_REMOVE, '-')
        .AddSeparator()
        .AddItem(B_TRANSLATE("Statistics" B_UTF8_ELLIPSIS), I_KEYRING_INFO, 'I')
    .End();
    return menu;
}
