/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <AppKit.h>
#include <InterfaceKit.h>
#include <StorageKit.h>
#include <Catalog.h>
#include <IconUtils.h>
#include <KeyStore.h>
#include <private/interface/ColumnTypes.h>
#include <private/shared/ToolBar.h>
#include <cstdio>
#include "KeysWindow.h"
#include "KeyringView.h"
#include "../KeysDefs.h"
#include "../dialogs/AddKeyDialogBox.h"
#include "../dialogs/DataViewerDialogBox.h"

BBitmap* addKeyIcon;
BBitmap* removeKeyIcon;
BBitmap* exportKeyIcon;
BBitmap* copySecretIcon;
BBitmap* viewKeyDataIcon;
BBitmap* removeAppIcon;
BBitmap* copySignIcon;
BBitmap* trackerIcon;

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Keyring view"

KeyringView::KeyringView(BRect frame, const char* name, KeystoreImp* _ks)
: BView(frame, name, 0, 0), ks(_ks), keyringname(name)
{
    keylistview = new BColumnListView("clv_keylist", 0, B_FANCY_BORDER);
    keylistview->SetSelectionMode(B_SINGLE_SELECTION_LIST);
    keylistview->SetSelectionMessage(new BMessage(KRV_KEYS_SEL));
    keylistview->SetInvocationMessage(new BMessage(KRV_KEYS_INVOKE));
    keylistview->AddColumn(new BStringColumn(B_TRANSLATE("Identifier"),
        200, 50, _MaxLength(keylistview, 0, 350, ks, true), 0, B_ALIGN_LEFT), 0);
    keylistview->AddColumn(new BStringColumn(B_TRANSLATE("Extra info"),
        200, 50, _MaxLength(keylistview, 1, 350, ks, true), 0, B_ALIGN_LEFT), 1);
    keylistview->AddColumn(new BStringColumn(B_TRANSLATE("Type"),
        100, 50, _MaxLength(keylistview, 2, 125, ks, true), 0, B_ALIGN_LEFT), 2);
    keylistview->AddColumn(new BStringColumn(B_TRANSLATE("Purpose"),
        100, 50, _MaxLength(keylistview, 3, 125, ks, true), 0, B_ALIGN_LEFT), 3);

    keylsttoolbar = new BToolBar(B_VERTICAL);
    keylsttoolbar->AddAction(KRV_KEYS_CREATE, this,
        addKeyIcon,
        B_TRANSLATE("Add key"), "", false);
    keylsttoolbar->AddAction(KRV_KEYS_REMOVE, this,
        removeKeyIcon,
        B_TRANSLATE("Remove key"), "", false);
    keylsttoolbar->AddAction(KRV_KEYS_EXPORT, this,
        exportKeyIcon,
        B_TRANSLATE("Export key as flattened BMessage"), "", false);
    keylsttoolbar->AddSeparator();
    keylsttoolbar->AddAction(KRV_KEYS_COPY, this,
        copySecretIcon,
        B_TRANSLATE("Copy key secret"), "", false);
    keylsttoolbar->AddAction(KRV_KEYS_VWDATA, this,
        viewKeyDataIcon,
        B_TRANSLATE("View key data"), "", false);
    keylsttoolbar->AddGlue();
    for(const auto& it : { KRV_KEYS_REMOVE, KRV_KEYS_COPY, KRV_KEYS_VWDATA, KRV_KEYS_EXPORT })
        keylsttoolbar->FindButton(it)->SetEnabled(false);

    BView *keysView = new BView(B_TRANSLATE("Keys"), B_SUPPORTS_LAYOUT, NULL);
    BLayoutBuilder::Group<>(keysView, B_HORIZONTAL, 0)
        .SetInsets(0)
        .Add(keylistview)
        .Add(keylsttoolbar)
    .End();

    applistview = new BColumnListView("clv_authapps", 0, B_FANCY_BORDER);
    applistview->SetSelectionMode(B_SINGLE_SELECTION_LIST);
    applistview->SetSelectionMessage(new BMessage(KRV_APPS_SEL));
    applistview->SetInvocationMessage(new BMessage(KRV_APPS_INVOKE));
    applistview->AddColumn(new BStringColumn(B_TRANSLATE("Application"),
        200, 100, _MaxLength(applistview, 0, 300, ks, false), 0, B_ALIGN_LEFT), 0);
    applistview->AddColumn(new BStringColumn(B_TRANSLATE("Signature"),
        325, 250, _MaxLength(applistview, 0, 400, ks, false), 0, B_ALIGN_LEFT), 1);

    applsttoolbar = new BToolBar(B_VERTICAL);
    applsttoolbar->AddAction(KRV_APPS_REMOVE, this,
        removeAppIcon,
        B_TRANSLATE("Remove application"), "", false);
    applsttoolbar->AddSeparator();
    applsttoolbar->AddAction(KRV_APPS_COPY, this,
        copySignIcon,
        B_TRANSLATE("Copy signature"), "", false);
    applsttoolbar->AddAction(KRV_APPS_INVOKE, this,
        trackerIcon,
        B_TRANSLATE("Open information view"), "", false);
    applsttoolbar->AddGlue();
    for(const auto& it : { KRV_APPS_REMOVE, KRV_APPS_COPY, KRV_APPS_INVOKE })
        applsttoolbar->FindButton(it)->SetEnabled(false);

    BView *authorizedAppsView = new BView(B_TRANSLATE("Authorized applications"),
        B_SUPPORTS_LAYOUT, NULL);
    BLayoutBuilder::Group<>(authorizedAppsView, B_HORIZONTAL, 0)
        .Add(applistview)
        .Add(applsttoolbar)
    .End();

    tabView = new BTabView("t_keys", B_WIDTH_FROM_WIDEST);
    tabView->AddTab(keysView);
    tabView->AddTab(authorizedAppsView);

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .SetInsets(0)
        .Add(tabView)
    .End();

    _InitAppData(/*ks*/);

    keylistview->ResizeAllColumnsToPreferred();
    applistview->ResizeAllColumnsToPreferred();
}

void KeyringView::AttachedToWindow()
{
    keylistview->SetTarget(this);

    for(const auto& command : { KRV_KEYS_CREATE, KRV_KEYS_REMOVE, KRV_KEYS_COPY,
    KRV_KEYS_VWDATA, KRV_KEYS_EXPORT})
        keylsttoolbar->FindButton(command)->SetTarget(this);

    applistview->SetTarget(this);

    for(const auto& command : { KRV_APPS_REMOVE, KRV_APPS_COPY, KRV_APPS_INVOKE })
        applsttoolbar->FindButton(command)->SetTarget(this);
}

void KeyringView::MessageReceived(BMessage* msg)
{
    switch(msg->what)
    {
        case KRV_KEYS_SEL:
            if(keylistview->CountRows() > 0) {
                for(const auto& it : { KRV_KEYS_REMOVE, KRV_KEYS_COPY, KRV_KEYS_VWDATA, KRV_KEYS_EXPORT })
                    keylsttoolbar->FindButton(it)->SetEnabled(true);
            }
            break;
        case KRV_KEYS_CREATE:
        {
            AddKeyDialogBox* dlg = new AddKeyDialogBox(Window(), BRect(20, 20, 200, 200), //ks,
                keyringname, B_KEY_TYPE_GENERIC, this);
            dlg->Show();
            break;
        }
        case KRV_KEYS_REMOVE:
        {
            if(keylistview->CurrentSelection()) {
                const char* sel = ((BStringField*)keylistview->CurrentSelection()->GetField(0))->String();
                const char* sec = ((BStringField*)keylistview->CurrentSelection()->GetField(1))->String();

                BString desc(B_TRANSLATE("Do you want to remove the key \"%key%\" from this keyring?"));
                desc.ReplaceAll("%key%", sel);
                BAlert* alert = new BAlert(B_TRANSLATE("Remove key"), desc,
                    B_TRANSLATE("Remove"), B_TRANSLATE("Cancel"), NULL,
                    B_WIDTH_AS_USUAL, B_WARNING_ALERT);
                alert->SetShortcut(1, B_ESCAPE);
                int32 result = alert->Go();
                if(result == 0) {
                    _RemoveKey(ks, sel, sec);
                    for(const auto& it : { KRV_KEYS_REMOVE, KRV_KEYS_COPY, KRV_KEYS_VWDATA, KRV_KEYS_EXPORT })
                        keylsttoolbar->FindButton(it)->SetEnabled(false);
                    // Update(NULL);
                }
            }
            else {
                BAlert* alert = new BAlert(B_TRANSLATE("Error"),
                    B_TRANSLATE("An entry must be selected first."),
                    B_TRANSLATE("OK"), NULL, NULL,
                    B_WIDTH_AS_USUAL, B_STOP_ALERT);
                alert->Go();
            }
            break;
        }
        case KRV_KEYS_INVOKE:
        case KRV_KEYS_VWDATA:
        {
            if(keylistview->CurrentSelection()) {
                const char* selection = ((BStringField*)keylistview->CurrentSelection()->GetField(0))->String();
                const char* secondary = ((BStringField*)keylistview->CurrentSelection()->GetField(1))->String();
                DataViewerDialogBox* dlg = new DataViewerDialogBox(Window(),
                    BRect(10, 10, 400, 300), ks, keyringname, selection, secondary);
                dlg->Show();
            }
            break;
        }
        case KRV_KEYS_COPY:
            _CopyKey(ks);
            break;
        case KRV_KEYS_EXPORT:
        {
            if(keylistview->CurrentSelection()) {
                const char* selection = ((BStringField*)keylistview->CurrentSelection()->GetField(0))->String();
                const char* secondary = ((BStringField*)keylistview->CurrentSelection()->GetField(1))->String();
                BMessage request(msg->what);
                request.AddString(kConfigKeyring, keyringname);
                request.AddString(kConfigKeyName, selection);
                request.AddString(kConfigKeyAltName, secondary);
                Window()->PostMessage(&request);
            }
            break;
        }
        case KRV_APPS_SEL:
            if(applistview->CountRows() > 0) {
                for(const auto& it : { KRV_APPS_REMOVE, KRV_APPS_COPY, KRV_APPS_INVOKE })
                    applsttoolbar->FindButton(it)->SetEnabled(true);
            }
            break;
        case KRV_APPS_REMOVE:
        {
            if(applistview->CurrentSelection()) {
                const char* sel = ((BStringField*)applistview->CurrentSelection()->GetField(1))->String();

                BAlert* alert = new BAlert(B_TRANSLATE("Remove authorized application"),
                    B_TRANSLATE("Do you want to remove this application from this keyring?"),
                    B_TRANSLATE("Remove"), B_TRANSLATE("Cancel"), NULL,
                    B_WIDTH_AS_USUAL, B_WARNING_ALERT);
                alert->SetShortcut(1, B_ESCAPE);
                int32 result = alert->Go();
                if(result == 0) {
                    _RemoveApp(ks, sel);
                    for(const auto& it : { KRV_APPS_REMOVE, KRV_APPS_COPY, KRV_APPS_INVOKE })
                        applsttoolbar->FindButton(it)->SetEnabled(false);
                    // Update(NULL);
                }
            }
            else {
                BAlert* alert = new BAlert(B_TRANSLATE("Error"),
                    B_TRANSLATE("An entry must be selected first."),
                    B_TRANSLATE("OK"), NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
                alert->Go();
            }
            break;
        }
        case KRV_APPS_COPY:
            _CopyAppSignature(ks);
            break;
        case KRV_APPS_INVOKE:
        case KRV_APPS_VWDATA:
        {
            if(applistview->CurrentSelection()) {
                const char* sel = ((BStringField*)applistview->CurrentSelection()->GetField(1))->String();
                entry_ref ref;
                if(be_roster->FindApp(sel, &ref) == B_OK) {
                    BMessenger msgr("application/x-vnd.Be-TRAK"); // Tracker signature
                    BMessage imsg('Tinf'); // kGetInfo
                    imsg.AddRef("refs", &ref); // ref required by Info Window
                    msgr.SendMessage(&imsg);
                }
            }
            break;
        }
        case M_KEY_CREATE:
            Update();
            break;
        default:
            BView::MessageReceived(msg);
            break;
    }
}

void KeyringView::Update(const void* data)
{
    fprintf(stderr, "[%s] Called update\n", Name());

    LockLooper();

    keylistview->Clear();
    applistview->Clear();

    // Ask application class to update the data model instance for the keyring
    // BMessage request(M_ASK_FOR_REFRESH), reply;
    // request.AddString(kConfigKeyring, keyringname);
    // be_app->PostMessage(&request);

    _InitAppData(/*ks*/);

    UnlockLooper();
}

void KeyringView::_InitAppData(/*KeystoreImp* ks*/)
{
    BRow* row = NULL;

    for(int i = 0; i < ks->KeyringByName(keyringname)->KeyCount(); i++) {
        KeyImp* key = ks->KeyringByName(keyringname)->KeyAt(i);

        row = new BRow();

        row->SetField(new BStringField(key->Identifier()), 0);
        row->SetField(new BStringField(key->SecondaryIdentifier()), 1);

        const char* type;
        switch(key->Type())
        {
            case B_KEY_TYPE_GENERIC:
                type = B_TRANSLATE("Generic");
                break;
            case B_KEY_TYPE_PASSWORD:
                type = B_TRANSLATE("Password");
                break;
            case B_KEY_TYPE_CERTIFICATE:
                type = B_TRANSLATE("Certificate");
                break;
            case B_KEY_TYPE_ANY:
            default:
                type = "";
                break;
        }
        row->SetField(new BStringField(type), 2);

        const char* purpose;
        switch(key->Purpose())
        {
            case B_KEY_PURPOSE_GENERIC:
                purpose = B_TRANSLATE("Generic");
                break;
            case B_KEY_PURPOSE_KEYRING:
                purpose = B_TRANSLATE("Keyring");
                break;
            case B_KEY_PURPOSE_WEB:
                purpose = B_TRANSLATE("Web");
                break;
            case B_KEY_PURPOSE_NETWORK:
                purpose = B_TRANSLATE("Network");
                break;
            case B_KEY_PURPOSE_VOLUME:
                purpose = B_TRANSLATE("Volume");
                break;
            case B_KEY_PURPOSE_ANY:
            default:
                purpose = "";
                break;
        }
        row->SetField(new BStringField(purpose), 3);

        keylistview->AddRow(row);
    }

    for(int i = 0; i < ks->KeyringByName(keyringname)->ApplicationCount(); i++) {
        row = new BRow();

        const char* signature = ks->KeyringByName(keyringname)->ApplicationAt(i)->Identifier();
        const char* name;
        entry_ref ref;
        if(be_roster->FindApp(signature, &ref) == B_OK)
            name = ref.name;
        else
            name = "";

        row->SetField(new BStringField(name), 0);
        row->SetField(new BStringField(signature), 1);

        applistview->AddRow(row);
    }

    keylistview->ResizeAllColumnsToPreferred();
    applistview->ResizeAllColumnsToPreferred();
}

void KeyringView::_CopyKey(KeystoreImp* ks)
{
    BKeyStore keystore;
    const char* keyid = ((BStringField*)keylistview->CurrentSelection()->GetField(0))->String();
    const char* secid = ((BStringField*)keylistview->CurrentSelection()->GetField(1))->String();
    BKeyType type = ks->KeyringByName(keyringname)->KeyByIdentifier(keyid, secid)->Type();

    if(type == B_KEY_TYPE_PASSWORD) {
        BPasswordKey pwdkey;
        keystore.GetKey(keyringname, type, keyid, secid, false, pwdkey);
        _CopyData(ks, keylistview, (const uint8*)pwdkey.Password());
    }
    else {
        BKey key;
        keystore.GetKey(keyringname, type, keyid, secid, false, key);
        _CopyData(ks, keylistview, key.Data());
    }
}

void KeyringView::_RemoveKey(KeystoreImp* ks, const char* id, const char* sec)
{
    KeyImp* key = ks->KeyringByName(keyringname)->KeyByIdentifier(id, sec);
    if(!key) {
        fprintf(stderr, "Error: a non-existing key was targeted.\n");
        return;
    }

    BMessage request(I_KEY_REMOVE);
    request.AddString(kConfigKeyring, keyringname);
    request.AddString(kConfigKeyName, key->Identifier());
    request.AddString(kConfigKeyAltName, key->SecondaryIdentifier());
    Window()->PostMessage(&request);
}

void KeyringView::_RemoveApp(KeystoreImp* ks, const char* signature)
{
    KeyringImp* keyring = ks->KeyringByName(keyringname);
    if(!keyring)
        return;

    BMessage request(I_APP_REMOVE);
    request.AddString(kConfigKeyring, keyringname);
    request.AddString(kConfigSignature, signature);
    Window()->PostMessage(&request);
}

void KeyringView::_CopyAppSignature(KeystoreImp* ks)
{
    const char* app = ((BStringField*)applistview->CurrentSelection()->GetField(1))->String();
    _CopyData(ks, applistview, (const uint8*)app);
}

void KeyringView::_CopyData(KeystoreImp* ks, BColumnListView* owner,
    const uint8* data)
{
    if(be_clipboard->Lock() && owner->CurrentSelection() != NULL) {
        be_clipboard->Clear();

        BMessage *clipmsg = be_clipboard->Data();
        clipmsg->AddData("text/plain", B_MIME_TYPE,
            reinterpret_cast<const char*>(data), strlen((const char*)data));
        status_t status = be_clipboard->Commit();
        if(status != B_OK)
            fprintf(stderr, "Error saving data to clipboard.\n");

        be_clipboard->Unlock();
    }
}

int KeyringView::_MaxLength(BColumnListView* view, int fieldid, int defValue, KeystoreImp* ks, bool iskeytype)
{
    int maxValue = defValue;

    if(iskeytype) {
        for(int i = 0; i < ks->KeyringByName(keyringname)->KeyCount(); i++) {
            int length = 0;
            switch(fieldid) {
                case 0:
                    length = view->StringWidth(ks->KeyringByName(keyringname)->KeyAt(i)->Identifier()) + 20;
                    break;
                case 1:
                    length = view->StringWidth(ks->KeyringByName(keyringname)->KeyAt(i)->SecondaryIdentifier()) + 20;
                    break;
                case 2:
                {
                    switch(ks->KeyringByName(keyringname)->KeyAt(i)->Type()) {
                        case B_KEY_TYPE_GENERIC:
                            length = view->StringWidth(B_TRANSLATE("Generic")); break;
                        case B_KEY_TYPE_PASSWORD:
                            length = view->StringWidth(B_TRANSLATE("Password")); break;
                        case B_KEY_TYPE_CERTIFICATE:
                        default:
                            length = view->StringWidth(B_TRANSLATE("Certificate")); break;
                    }
                    break;
                }
                case 3:
                {
                    switch(ks->KeyringByName(keyringname)->KeyAt(i)->Purpose()) {
                        case B_KEY_PURPOSE_GENERIC:
                        default:
                            length = view->StringWidth(B_TRANSLATE("Generic")) + 20; break;
                        case B_KEY_PURPOSE_KEYRING:
                            length = view->StringWidth(B_TRANSLATE("Keyring")) + 20; break;
                        case B_KEY_PURPOSE_NETWORK:
                            length = view->StringWidth(B_TRANSLATE("Network")) + 20; break;
                        case B_KEY_PURPOSE_VOLUME:
                            length = view->StringWidth(B_TRANSLATE("Volume")) + 20; break;
                        case B_KEY_PURPOSE_WEB:
                            length = view->StringWidth(B_TRANSLATE("Web")) + 20; break;
                    }
                    break;
                }
            }

            if(maxValue < length)
                maxValue = length;
        }
    }
    else {
        for(int i = 0; i < ks->KeyringByName(keyringname)->ApplicationCount(); i++) {
            int length = 0;
            switch(fieldid) {
                case 0:
                    length = 250; // hardcoded, we can't know apps' names here
                    break;
                case 1:
                    length = view->StringWidth(ks->KeyringByName(keyringname)->ApplicationAt(i)->Identifier()) + 20;
                    break;
            }
            if(maxValue < length)
                maxValue = length;
        }
    }

    return maxValue;
}
