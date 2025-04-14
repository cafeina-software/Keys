/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <AppFileInfo.h>
#include <Application.h>
#include <Catalog.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Resources.h>
#include <Roster.h>
#include <cstdio>
#include <cassert>
#include "../data/BackUpUtils.h"
#include "../dialogs/AddKeyDialogBox.h"
#include "../dialogs/AddUnlockKeyDialogBox.h"
#include "../dialogs/AddKeyringDialogBox.h"
#include "../dialogs/KeyringViewerDialogBox.h"
#include "../dialogs/MultipleImporterDialogBox.h"
#include "../dialogs/BackUpDBDialogBox.h"
#include "../KeysDefs.h"
#include "KeysWindow.h"
#include "ListViewEx.h"

BBitmap* icon_loader(const char* name, int x_size, int y_size) {
    size_t length = 0;
    BResources *res = BApplication::AppResources();
    const void* data = res->LoadResource(B_VECTOR_ICON_TYPE, name, &length);
    BBitmap* bitmap = new BBitmap(BRect(0, 0, x_size, y_size), B_RGBA32);
    BIconUtils::GetVectorIcon((uint8*)(data), length, bitmap);
    return bitmap;
}

void init_shared_icons() {
    unsigned int isize = 32;

    addKeyIcon = icon_loader("i_key_create", isize, isize);
    removeKeyIcon = icon_loader("i_key_remove", isize, isize);
    exportKeyIcon = icon_loader("i_key_cvt2msg", isize, isize);
    copySecretIcon = icon_loader("i_key_cpydata", isize, isize);
    viewKeyDataIcon = icon_loader("i_key_viewdata", isize, isize);

    removeAppIcon = icon_loader("i_app_remove", isize, isize);
    copySignIcon = icon_loader("i_app_cpysgn", isize, isize);
    entry_ref trackerref;
    be_roster->FindApp("application/x-vnd.Be-TRAK", &trackerref);
    BFile trackerfile(&trackerref, B_READ_ONLY);
    BAppFileInfo afinfo(&trackerfile);
    trackerIcon = new BBitmap(BRect(0, 0, 31, 31), B_RGBA32);
    afinfo.GetIcon(trackerIcon, B_MINI_ICON);
}

void uninit_shared_icons() {
    delete addKeyIcon;
    delete removeKeyIcon;
    delete exportKeyIcon;
    delete copySecretIcon;
    delete viewKeyDataIcon;
    delete removeAppIcon;
    delete copySignIcon;
    delete trackerIcon;
}

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

// #pragma mark -

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Main window"

KeysWindow::KeysWindow(BRect frame, KeystoreImp* _ks, BKeyStore* _keystore)
: BWindow(frame, B_TRANSLATE_SYSTEM_NAME(kAppName), B_TITLED_WINDOW, B_QUIT_ON_WINDOW_CLOSE),
  keystore(_keystore),
  ks(_ks),
  uiStatus(S_UI_NO_KEYRING_IN_FOCUS),
  fFilter(new KeyMsgRefFilter),
  openPanel(nullptr),
  savePanel(nullptr),
  fRemKeyring(nullptr),
  fIsLockedKeyring(nullptr),
  fMenuKeyring(nullptr),
  fMenuKey(nullptr),
  fAppKey(nullptr)
{
    init_shared_icons();

    mbMain = _InitMenu();
    fKeystorePopMenu = _InitKeystorePopUpMenu();
    fKeyringItemMenu = _InitKeyringPopUpMenu();

    listView = new BListViewEx("lv_main", B_SINGLE_SELECTION_LIST,
        B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE, fKeystorePopMenu, fKeyringItemMenu);
    listView->SetSelectionMessage(new BMessage(I_SELECTED));
    listView->SetInvocationMessage(new BMessage(I_KEYRING_INFO));
    listView->SetExplicitMaxSize(BSize(listView->StringWidth("Master") * 3, B_SIZE_UNSET));
    BScrollView *listScroll = new BScrollView("sc_main", listView, 0, false, true);

    keyringView = new KeyringView(BRect(), "", ks);

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
                .Add(keyringView)
            .End()
        .End()
    .End();

    _InitAppData(ks);
}

KeysWindow::~KeysWindow()
{
    uninit_shared_icons();

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
        case I_SERVER_STOP:
            be_app->PostMessage(msg);
            break;
        case I_DATA_REFRESH:
            // _InitAppData(ks);
            break;
        case I_ABOUT:
            be_app->PostMessage(B_ABOUT_REQUESTED);
            break;
        case I_QUIT:
            be_app->PostMessage(B_QUIT_REQUESTED);
            break;
        case I_SELECTED:
        {
            const char* sel = ((BStringItem*)listView->ItemAt(listView->CurrentSelection()))->Text();

            assert(ks->KeyringByName(sel));

            keyringView->Update(sel);
            SetUIStatus(S_UI_HAS_KEYRING_IN_FOCUS);
            break;
        }
        case I_KEYSTORE_BACKUP:
        {
            BackUpDBDialogBox* dlg = new BackUpDBDialogBox(this, BRect());
            dlg->Show();
            break;
        }
        case I_KEYSTORE_RESTORE:
        {
            int32 result;
            switch(result = (new BAlert(B_TRANSLATE("Restore backup"),
            B_TRANSLATE("Do you want to restore a backup of the keystore database?\n"
            "The keystore server will be stop temporarily during the operation.\n\n"
            "If you select to restore a simple copy, choose the file in the file panel.\n"
            "For an encrypted backup, choose the datafile where the metadata is contained.\n"),
            B_TRANSLATE("Restore simple copy"), B_TRANSLATE("Restore encrypted copy"),
            B_TRANSLATE("Cancel"), B_WIDTH_FROM_LABEL, B_IDEA_ALERT))->Go()) {
                case 0:
                    (new BAlert("Error", "Not Implemented", "OK"))->Go();
                    break;
                case 1: {
                    BPath dbpath, parent;
                    DBPath(&dbpath);
                    dbpath.GetParent(&parent);
                    BMessage data(B_REFS_RECEIVED);
                    data.AddUInt32(kConfigWhat, msg->what);
                    data.AddInt32("mode", 1);
                    openPanel->SetRefFilter(new SimpleFilter);
                    openPanel->SetPanelDirectory(parent.Path());
                    openPanel->SetButtonLabel(B_DEFAULT_BUTTON, B_TRANSLATE("Import"));
                    openPanel->SetMessage(&data);
                    openPanel->Show();
                    break;
                }
                case 2:
                default:
                    break;
            }
            break;
        }
        case I_KEYSTORE_CLEAR:
        {
            if((new BAlert(B_TRANSLATE("Wipe keystore"),
            B_TRANSLATE("Do you want to clean the keystore database?\n\nThis will "
            "delete all the keyrings and all of their keys, as well as deleting "
            "all the keys of \"Master\" keyring (however this last keyring will not "
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
        case LVX_DELETE_REQUESTED:
        {
            BListView* list = (BListView*)msg->GetPointer("origin");
            BStringItem* item = (BStringItem*)msg->GetPointer("item_to_delete");
            if(list && item && ks->KeyringByName(item->Text())) {
                list->ScrollTo(list->IndexOf(item));
                _RemoveKeyring(item->Text());
            }
            break;
        }
        case I_KEYRING_REMOVE:
            _RemoveKeyring();
            break;
        case I_KEYRING_LOCK:
            _LockKeyring();
            break;
        case I_KEYRING_UNLOCKKEY_ADD:
            _AddKey(B_KEY_TYPE_PASSWORD, AKDM_UNLOCK_KEY);
            // _SetKeyringLockKey();
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
            request.what = msg->GetUInt32(kConfigWhat, M_KEY_CREATE); // redirect request
            be_app->PostMessage(&request);
            break;
        }
        case I_KEY_ADD_GENERIC:
            _AddKey(B_KEY_TYPE_GENERIC, AKDM_STANDARD);
            break;
        case I_KEY_ADD_PASSWORD:
            _AddKey(B_KEY_TYPE_PASSWORD, AKDM_STANDARD);
            break;
        case I_KEY_ADD_CERT:
            _AddKey(B_KEY_TYPE_CERTIFICATE, AKDM_STANDARD);
            break;
        case I_KEY_GENERATE_PWD:
            _AddKey(B_KEY_TYPE_PASSWORD, AKDM_KEY_GEN);
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
            if(msg->FindUInt32(kConfigWhat, &what) != B_OK)
                break;

            if(what == I_KEY_IMPORT)
                _ImportKey(msg);
            else if(what == I_KEYSTORE_RESTORE) {
                BMessage request(*msg);
                request.what = M_KEYSTORE_RESTORE;
                entry_ref ref;
                if(msg->FindRef("refs", &ref) != B_OK)
                    fprintf(stderr, "Warning: missing refs\n");
                be_app->PostMessage(&request);
            }
            break;
        }
        case B_SIMPLE_DATA:
        {
            if(currentKeyring == NULL) {
                __trace("Error: a keyring must be selected first.\n");
                break;
            }

            BFile keyFile;
            entry_ref ref;
            int32 index = 0;
            BMessage importerData;
            BMessage droppedData;
            while(msg->FindRef("refs", index, &ref) == B_OK) {
                index++;

                keyFile.SetTo(&ref, B_READ_ONLY);
                if(keyFile.InitCheck() != B_OK) { // Drop any invalid references
                    droppedData.AddRef("refs", &ref);
                    droppedData.AddString("reason", B_TRANSLATE_COMMENT("Bad file descriptor",
                        "This is from strerror(B_FILE_ERROR)"));
                    continue;
                }
                BMessage keyFileData;
                if(keyFileData.Unflatten(&keyFile) == B_OK) {
                    // Presanitization to drop any non-exported-key flattened BMessage
                    if(IsExportedKey(&keyFileData))
                        importerData.AddRef("refs", &ref);
                    else {
                        droppedData.AddRef("refs", &ref);
                        droppedData.AddString("reason", B_TRANSLATE("Message is not an exported key"));
                    }
                }
                else { // Drop any foreign formats
                    droppedData.AddRef("refs", &ref);
                    droppedData.AddString("reason", B_TRANSLATE_COMMENT("Data is not a message",
                        "This is from strerror(B_NOT_A_MESSAGE)"));
                }
                keyFile.Unset();
            }

            MultipleImporterDialogBox* importer = new MultipleImporterDialogBox(this, currentKeyring, &importerData, &droppedData);
            importer->Show();
            break;
        }
        case I_KEY_EXPORT:
        {
            if(!currentKeyring || strcmp(currentKeyring, "") == 0)
                break;

            /* Fields from a keyring view */
            BString keyring, id, alt;
            if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
            msg->FindString(kConfigKeyName, &id) != B_OK ||
            msg->FindString(kConfigKeyAltName, &alt) != B_OK) {
                fprintf(stderr, "Error: invalid message.\n");
                break;
            }

            BMessage* request = new BMessage(B_SAVE_REQUESTED);
            request->AddUInt32(kConfigWhat, msg->what);
            request->AddString(kConfigKeyring, keyring.String());
            request->AddString(kConfigKeyName, id.String());
            request->AddString(kConfigKeyAltName, alt.String());

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
        case I_APP_REMOVE:
            _RemoveApp(msg);
            break;

        /* Notifications: update views */
        case B_REPLY:
            _HandleReplyBacks(msg);
            break;
        default:
            BWindow::MessageReceived(msg);
            break;
    }
}

void KeysWindow::SetUIStatus(ui_status status, const char* focus)
{
	uiStatus = status;

    LockLooper();

    switch(status)
    {
        case S_UI_NO_SERVER:
        {
            fMenuKeystore->SetEnabled(false);
            fMenuKeyring->SetEnabled(false);
            fKeystorePopMenu->SetEnabled(false);
            fKeyringItemMenu->SetEnabled(false);

            listView->MakeEmpty();
            listView->SetEnabled(false);
            addKeyringButton->SetEnabled(false);
            removeKeyringButton->SetEnabled(false);

            keyringView->Update(NULL);
            BMessage reply(B_REPLY);
            reply.AddBool("keyring_unavailable", true);
            BMessenger(keyringView).SendMessage(&reply);
            break;
        }
        case S_UI_HAS_SERVER:
            fMenuKeystore->SetEnabled(true);
            fKeystorePopMenu->SetEnabled(true);
            fKeyringItemMenu->SetEnabled(true);
            listView->SetEnabled(true);
            addKeyringButton->SetEnabled(true);
            [[fallthrough]];
        case S_UI_REMOVE_KEYRING_FOCUS:
            listView->DeselectAll();
            [[fallthrough]];
        case S_UI_NO_KEYRING_IN_FOCUS:
        {
            currentKeyring = NULL;

            fRemKeyring->SetEnabled(false);
            fMenuKeyring->SetEnabled(false);
            removeKeyringButton->SetEnabled(false);
            fIsLockedKeyring->SetMarked(false);
            keyringView->Update(NULL);
            BMessage reply(B_REPLY);
            reply.AddBool("keyring_unavailable", true);
            BMessenger(keyringView).SendMessage(&reply);
            break;
        }
        case S_UI_SET_KEYRING_FOCUS:
        {
            BStringItem* item = nullptr;
            if(focus)
                item = find_item(listView, focus);
            if(item && listView->HasItem(item))
                listView->Select(listView->IndexOf(item));
        }
            [[fallthrough]];
        case S_UI_HAS_KEYRING_IN_FOCUS:
        {
            currentKeyring = ((BStringItem*)listView->ItemAt(listView->CurrentSelection()))->Text();

            fRemKeyring->SetEnabled(true);
            fMenuKeyring->SetEnabled(true);
            removeKeyringButton->SetEnabled(true);
            fIsLockedKeyring->SetMarked(!(ks->KeyringByName(currentKeyring)->IsUnlocked()));
            keyringView->Update(currentKeyring);
            BMessage reply(B_REPLY);
            reply.AddBool("keyring_changed", true);
            BMessenger(keyringView).SendMessage(&reply);
            break;
        }
        default:
            break;
    }

    // mbMain->FindItem(I_KEYSTORE_BACKUP)->SetEnabled(false);
    // mbMain->FindItem(I_KEYSTORE_RESTORE)->SetEnabled(false);

    UnlockLooper();
}

ui_status KeysWindow::GetUIStatus()
{
    return uiStatus;
}

void KeysWindow::Update(const void* data)
{
	_InitAppData(ks);
    if(data != nullptr && ks->KeyringByName((const char*)data)) {
        fprintf(stderr, "Received: %s\n", (const char*)data);
        BStringItem* item = find_item(listView, reinterpret_cast<const char*>(data));
        if(item) {
            listView->Select(listView->IndexOf(item));
            SetUIStatus(S_UI_HAS_KEYRING_IN_FOCUS);
        }
    }
    else
        SetUIStatus(S_UI_NO_KEYRING_IN_FOCUS);
}

// #pragma mark -

void KeysWindow::_InitAppData(KeystoreImp* ks)
{
    __trace("CALLED.\n");
    LockLooper();
    listView->MakeEmpty();
    for(int i = 0; i < ks->KeyringCount(); i++) {
        listView->AddItem(new BStringItem(ks->KeyringAt(i)->Identifier()));
    }
    // Preselect something to avoid protection faults when selecting a key
    //  without a keyring selected in the other view
    if(listView->CountItems() > 0) {
        currentKeyring = ((BStringItem*)listView->ItemAt(0))->Text();
        listView->Select(0);

        if(listView->IsItemSelected(listView->CurrentSelection()))
            fMenuKeyring->SetEnabled(true);
    }
    UnlockLooper();
}

void KeysWindow::_HandleReplyBacks(BMessage* reply)
{
    BString alertText;
    switch(reply->GetInt32(kConfigWhat, B_REPLY))
    {
        case I_SERVER_RESTART:
            SetUIStatus(S_UI_HAS_SERVER);
            return; // No need to report errors
        case I_SERVER_STOP:
            SetUIStatus(S_UI_NO_SERVER);
            return; // No need to report errors
        case M_KEYSTORE_BACKUP:
            alertText.SetTo("Keystore backup error: ");
            break;
        case M_KEYSTORE_RESTORE:
            alertText.SetTo("Keystore restore error: ");
            break;
        case M_KEYRING_CREATE:
            alertText.SetTo("Keyring creation error: ");
            break;
        case M_KEYRING_LOCK:
            alertText.SetTo("Keyring lockdown error: ");
            break;
        case M_KEYRING_SET_LOCKKEY:
            alertText.SetTo("Keyring lock key addition error: ");
            break;
        case M_KEYRING_UNSET_LOCKKEY:
            alertText.SetTo("Keyring lock key deletion error: ");
            break;
        case M_KEYRING_DELETE:
            alertText.SetTo("Keyring deletion error: ");
            break;
        case M_KEY_CREATE:
            alertText.SetTo("Key creation error: ");
            break;
        case M_KEY_GENERATE_PASSWORD:
            alertText.SetTo("Key generation error: ");
            break;
        case M_KEY_IMPORT:
            alertText.SetTo("Key import error: ");
            break;
        case M_KEY_EXPORT:
            alertText.SetTo("Key export error: ");
            break;
        case M_KEY_DELETE:
            alertText.SetTo("Key deletion error: ");
            break;
        case M_APP_DELETE:
            alertText.SetTo("Application deletion error: ");
            break;
        default:
            alertText.SetTo("Reply back: unknown...\n");
            break;
    }

    alertText.Append(strerror(reply->GetInt32(kConfigResult, 0)));

    BAlert* alert = new BAlert;
    alert->SetText(alertText.String());
    alert->SetTitle(B_TRANSLATE_COMMENT("Error", "Title of error alerts"));
    alert->SetType(alert_type::B_STOP_ALERT);
    alert->AddButton(B_TRANSLATE("Close"));
    alert->Go();
}

// #pragma mark - Keystore management calls

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Keystore info dialog box"

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

status_t KeysWindow::_RemoveKeyring(const char* target)
{
    const char* keyring;
    if(!target)
        keyring = currentKeyring;
    else
        keyring = target;

    if(!keyring || strcmp(keyring, "") == 0) {
        (new BAlert(B_TRANSLATE("Remove keyring: error"),
        B_TRANSLATE("Invalid keyring name."), B_TRANSLATE("Close"),
        NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go();
        return B_BAD_VALUE;
    }

    if(strcmp(keyring, "Master") == 0) {
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

    BMessage request(M_KEYRING_DELETE);
    request.AddPointer(kConfigWho, this);
    request.AddString(kConfigKeyring, keyring);
    be_app_messenger.SendMessage(&request);

    return B_OK;
}

void KeysWindow::_LockKeyring()
{
    if(!currentKeyring || strcmp(currentKeyring, "") == 0)
        return;

    BMessage request(M_KEYRING_LOCK);
    request.AddString(kConfigKeyring, currentKeyring);
    be_app_messenger.SendMessage(&request);
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

    BMessage request(M_KEYRING_UNSET_LOCKKEY);
    request.AddString(kConfigKeyring, currentKeyring);
    be_app_messenger.SendMessage(&request);
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

    BMessage request(M_KEYRING_WIPE_CONTENTS);
    request.AddString(kConfigKeyring, currentKeyring);
    be_app_messenger.SendMessage(&request);
}

void KeysWindow::_KeyringInfo()
{
    if(!currentKeyring ||
    strcmp(currentKeyring, "") == 0 ||
    listView->CurrentSelection(0) == -1)
        return;

    KeyringViewerDialogBox* dlg = new KeyringViewerDialogBox(this, BRect(), ks, currentKeyring);
    if(dlg) dlg->Show();
}

void KeysWindow::_AddKey(BKeyType t, AKDlgModel model)
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

    AddKeyDialogBox* dlg = new AddKeyDialogBox(this, BRect(20, 20, 200, 200),
        currentKeyring, t, keyringView, model);
    if(dlg) dlg->Show();
}

void KeysWindow::_ImportKey(BMessage* msg)
{
    if(!currentKeyring || strcmp(currentKeyring, "") == 0)
        return;

    BMessage request(M_KEY_IMPORT);
    request.AddString(kConfigKeyring, currentKeyring);
    entry_ref ref;
    (void)msg->FindRef("refs", &ref);
    request.AddRef("refs", &ref);
    be_app_messenger.SendMessage(&request);
}

void KeysWindow::_ExportKey(BMessage* msg)
{
    BString keyring(msg->GetString(kConfigKeyring));
    BString key(msg->GetString(kConfigKeyName));
    BString secondary(msg->GetString(kConfigKeyAltName));
    if(keyring.IsEmpty() || key.IsEmpty())
        return;

    BMessage request(M_KEY_EXPORT), reply;
    entry_ref ref;
    (void)msg->FindRef("directory", &ref);
    request.AddRef("directory", &ref);
    request.AddString("name", msg->FindString("name")); // It does not have to be equal to id
    request.AddString(kConfigKeyring, keyring.String());
    request.AddString(kConfigKeyName, key.String());
    request.AddString(kConfigKeyAltName, secondary.String());
    be_app_messenger.SendMessage(&request, &reply);
}

void KeysWindow::_RemoveKey(BMessage* msg)
{
    BString keyring(msg->GetString(kConfigKeyring));
    BString key(msg->GetString(kConfigKeyName));
    BString secondary(msg->GetString(kConfigKeyAltName));
    if(keyring.IsEmpty() || key.IsEmpty())
        return;

    BMessage request(M_KEY_DELETE);
    request.AddString(kConfigKeyring, keyring.String());
    request.AddString(kConfigKeyName, key.String());
    request.AddString(kConfigKeyAltName, secondary.String());
    be_app_messenger.SendMessage(&request);
}

void KeysWindow::_RemoveApp(BMessage* msg)
{
    __trace("Request: delete app %s from %s keyring.\n",
        msg->GetString(kConfigSignature), msg->GetString(kConfigKeyring));
    BMessage request(M_APP_DELETE);
    request.AddString(kConfigKeyring, msg->GetString(kConfigKeyring));
    request.AddString(kConfigSignature, msg->GetString(kConfigSignature));
    be_app->PostMessage(&request);
}

// #pragma mark - UI creation

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Main menu"

BMenuBar* KeysWindow::_InitMenu()
{
    BMenuItem* backupDBItem = new BMenuItem(B_TRANSLATE("Backup keystore database" B_UTF8_ELLIPSIS), new BMessage(I_KEYSTORE_BACKUP));
    BMenuItem* restoreDBItem = new BMenuItem(B_TRANSLATE("Restore keystore database snapshot" B_UTF8_ELLIPSIS), new BMessage(I_KEYSTORE_RESTORE));

    BMenuBar* menu = new BMenuBar("mb_main");
    BLayoutBuilder::Menu<>(menu)
        .AddMenu(B_TRANSLATE(kAppName))
            .AddItem(B_TRANSLATE("Restart keystore server"), I_SERVER_RESTART)
            .AddItem(B_TRANSLATE("Stop server"), I_SERVER_STOP)
            .AddSeparator()
            .AddItem(B_TRANSLATE("About" B_UTF8_ELLIPSIS), I_ABOUT)
            .AddSeparator()
            .AddItem(B_TRANSLATE("Quit"), I_QUIT, 'Q')
        .End()
        .AddMenu(B_TRANSLATE("Keystore"))
            .AddItem(B_TRANSLATE("Create keyring" B_UTF8_ELLIPSIS), I_KEYRING_ADD, '+')
            .AddItem(B_TRANSLATE("Remove keyring" B_UTF8_ELLIPSIS), I_KEYRING_REMOVE, '-')
            .AddSeparator()
            // .AddItem(backupDBItem)
            // .AddItem(restoreDBItem)
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
            .AddItem(B_TRANSLATE("Generate password key" B_UTF8_ELLIPSIS), I_KEY_GENERATE_PWD, 'K')
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
    backupDBItem->SetEnabled(false);
    restoreDBItem->SetEnabled(false);
    fMenuKeystore = menu->FindItem(B_TRANSLATE("Keystore"));
    fMenuKeyring = menu->FindItem(B_TRANSLATE("Keyring"));

    fMenuKey = new BMenu(B_TRANSLATE("Key"));
    BLayoutBuilder::Menu<>(fMenuKey)
        .AddItem(B_TRANSLATE("Export" B_UTF8_ELLIPSIS), KRV_KEYS_EXPORT)
        .AddSeparator()
        .AddItem(B_TRANSLATE("View data" B_UTF8_ELLIPSIS), KRV_KEYS_VWDATA)
        .AddItem(B_TRANSLATE("Copy secret"), KRV_KEYS_COPY)
    .End();
    // menu->AddItem(fMenuKey);

    fAppKey = new BMenu(B_TRANSLATE("Application"));
    BLayoutBuilder::Menu<>(fAppKey)
        .AddItem(B_TRANSLATE("View information" B_UTF8_ELLIPSIS), KRV_APPS_VWDATA)
        .AddItem(B_TRANSLATE("Copy signature"), KRV_APPS_COPY)
    .End();
    // menu->AddItem(fAppKey);

    fRemKeyring->SetEnabled(false);
    fMenuKeyring->SetEnabled(false);

    return menu;
}

BPopUpMenu* KeysWindow::_InitKeystorePopUpMenu()
{
    BMenuItem* backupDBItem = new BMenuItem(B_TRANSLATE("Backup keystore database" B_UTF8_ELLIPSIS), new BMessage(I_KEYSTORE_BACKUP));
    BMenuItem* restoreDBItem = new BMenuItem(B_TRANSLATE("Restore keystore database snapshot" B_UTF8_ELLIPSIS), new BMessage(I_KEYSTORE_RESTORE));

    BPopUpMenu* menu = new BPopUpMenu("pum_ks", false, false);
    BLayoutBuilder::Menu<>(menu)
        .AddItem(B_TRANSLATE("Create keyring" B_UTF8_ELLIPSIS), I_KEYRING_ADD, '+')
        .AddSeparator()
        // .AddItem(backupDBItem)
        // .AddItem(restoreDBItem)
        .AddItem(B_TRANSLATE("Wipe keystore database" B_UTF8_ELLIPSIS), I_KEYSTORE_CLEAR)
        .AddSeparator()
        .AddItem(B_TRANSLATE("Keystore statistics" B_UTF8_ELLIPSIS), I_KEYSTORE_INFO)
    .End();

    backupDBItem->SetEnabled(false);
    restoreDBItem->SetEnabled(false);

    return menu;
}

BPopUpMenu* KeysWindow::_InitKeyringPopUpMenu()
{
    BPopUpMenu* menu = new BPopUpMenu("pum_kr", false, false);
    BLayoutBuilder::Menu<>(menu)
        .AddItem(B_TRANSLATE("Add generic key" B_UTF8_ELLIPSIS), I_KEY_ADD_GENERIC, 'G')
        .AddItem(B_TRANSLATE("Add password key" B_UTF8_ELLIPSIS), I_KEY_ADD_PASSWORD, 'P')
        .AddItem(B_TRANSLATE("Generate password key" B_UTF8_ELLIPSIS), I_KEY_GENERATE_PWD, 'K')
        .AddItem(B_TRANSLATE("Import key" B_UTF8_ELLIPSIS), I_KEY_IMPORT, 'M')
        .AddSeparator()
        .AddItem(B_TRANSLATE("Lock keyring"), I_KEYRING_LOCK)
        .AddItem(B_TRANSLATE("Set unlock key" B_UTF8_ELLIPSIS), I_KEYRING_UNLOCKKEY_ADD)
        .AddItem(B_TRANSLATE("Remove unlock key" B_UTF8_ELLIPSIS), I_KEYRING_UNLOCKKEY_REM)
        .AddSeparator()
        .AddItem(B_TRANSLATE("Wipe keyring" B_UTF8_ELLIPSIS), I_KEYRING_CLEAR)
        .AddItem(B_TRANSLATE("Remove keyring" B_UTF8_ELLIPSIS), I_KEYRING_REMOVE, '-')
        .AddSeparator()
        .AddItem(B_TRANSLATE("Statistics" B_UTF8_ELLIPSIS), I_KEYRING_INFO, 'I')
    .End();
    return menu;
}
