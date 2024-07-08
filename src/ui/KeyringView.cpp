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
#include "KeyringView.h"
#include "AddKeyDialogBox.h"
#include "DataViewerDialogBox.h"
#include "KeysDefs.h"

BBitmap* icon_loader(const char* name, int x_size, int y_size) {
    size_t length = 0;
    BResources *res = BApplication::AppResources();
    const void* data = res->LoadResource(B_VECTOR_ICON_TYPE, name, &length);
    BBitmap* bitmap = new BBitmap(BRect(0, 0, x_size, y_size), B_RGBA32);
    BIconUtils::GetVectorIcon((uint8*)(data), length, bitmap);
    return bitmap;
}

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Keyring view"

KeyringView::KeyringView(BRect frame, const char* name, KeystoreImp* _ks)
: BView(frame, name, 0, 0), ks(_ks), keyringname(name)
{
    keyIcon = new BBitmap(BRect(0, 0, 24, 24), B_RGBA32);
    appIcon = new BBitmap(BRect(0, 0, 24, 24), B_RGBA32);

    BMimeType keymime("application/x-vnd.Haiku-keystore_server");
    BMimeType appmime("application/x-vnd.Be-elfexecutable");

    keymime.GetIcon(keyIcon, B_MINI_ICON);
    appmime.GetIcon(appIcon, B_MINI_ICON);

    unsigned int isize = 32;

    keylistview = new BColumnListView("clv_keylist", 0, B_FANCY_BORDER);
    keylistview->SetSelectionMode(B_SINGLE_SELECTION_LIST);
    keylistview->SetSelectionMessage(new BMessage(KRV_KEYS_SEL));
    keylistview->SetInvocationMessage(new BMessage(KRV_KEYS_INVOKE));
    keylistview->ResizeAllColumnsToPreferred();
    keylistview->AddColumn(new BStringColumn(B_TRANSLATE("Identifier"),
        200, 50, 250, 0, B_ALIGN_LEFT), 0);
    keylistview->AddColumn(new BStringColumn(B_TRANSLATE("Extra info"),
        100, 50, 200, 0, B_ALIGN_LEFT), 1);
    keylistview->AddColumn(new BStringColumn(B_TRANSLATE("Type"),
        100, 50, 200, 0, B_ALIGN_LEFT), 2);
    keylistview->AddColumn(new BStringColumn(B_TRANSLATE("Purpose"),
        100, 50, 200, 0, B_ALIGN_LEFT), 3);

    keylsttoolbar = new BToolBar(B_VERTICAL);
    keylsttoolbar->AddAction(KRV_KEYS_CREATE, this,
        icon_loader("i_key_create", isize, isize),
        B_TRANSLATE("Add key"), "", false);
    keylsttoolbar->AddAction(KRV_KEYS_REMOVE, this,
        icon_loader("i_key_remove", isize, isize),
        B_TRANSLATE("Remove key"), "", false);
    keylsttoolbar->AddSeparator();
    keylsttoolbar->AddAction(KRV_KEYS_COPY, this,
        icon_loader("i_key_cpydata", isize, isize),
        B_TRANSLATE("Copy key secret"), "", false);
    keylsttoolbar->AddAction(KRV_KEYS_VWDATA, this,
        icon_loader("i_key_viewdata", isize, isize),
        B_TRANSLATE("View key data"), "", false);
    keylsttoolbar->AddGlue();
    keylsttoolbar->FindButton(KRV_KEYS_REMOVE)->SetEnabled(false);
    keylsttoolbar->FindButton(KRV_KEYS_COPY)->SetEnabled(false);
    keylsttoolbar->FindButton(KRV_KEYS_VWDATA)->SetEnabled(false);

    BView *keysView = new BView(B_TRANSLATE("Keys"), B_SUPPORTS_LAYOUT, NULL);
    BLayoutBuilder::Group<>(keysView, B_HORIZONTAL, 0)
        .SetInsets(0)
        .Add(keylistview)
        .Add(keylsttoolbar)
    .End();

    applistview = new BColumnListView("clv_authapps", 0, B_FANCY_BORDER);
    applistview->SetSelectionMode(B_SINGLE_SELECTION_LIST);
    applistview->SetSelectionMessage(new BMessage(KRV_APPS_SEL));
    applistview->ResizeAllColumnsToPreferred();
    applistview->AddColumn(new BStringColumn(B_TRANSLATE("Application"),
        200, 100, 300, 0, B_ALIGN_LEFT), 0);
    applistview->AddColumn(new BStringColumn(B_TRANSLATE("Signature"),
        325, 250, 400, 0, B_ALIGN_LEFT), 1);

    applsttoolbar = new BToolBar(B_VERTICAL);
    applsttoolbar->AddAction(KRV_APPS_REMOVE, this,
        icon_loader("i_app_remove", isize, isize),
        B_TRANSLATE("Remove application"), "", false);
    applsttoolbar->AddSeparator();
    applsttoolbar->AddAction(KRV_APPS_COPY, this,
        icon_loader("i_app_cpysgn", isize, isize),
        B_TRANSLATE("Copy signature"), "", false);
    applsttoolbar->AddGlue();
    applsttoolbar->FindButton(KRV_APPS_REMOVE)->SetEnabled(false);
    applsttoolbar->FindButton(KRV_APPS_COPY)->SetEnabled(false);

    BView *authorizedAppsView = new BView(B_TRANSLATE("Authorized applications"),
        B_SUPPORTS_LAYOUT, NULL);
    BLayoutBuilder::Group<>(authorizedAppsView, B_HORIZONTAL, 0)
        // .SetInsets(0)
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

    _InitAppData(ks);
}

void KeyringView::AttachedToWindow()
{
    keylistview->SetTarget(this);

    for(const auto& command : { KRV_KEYS_CREATE, KRV_KEYS_REMOVE, KRV_KEYS_COPY,
    KRV_KEYS_VWDATA })
        keylsttoolbar->FindButton(command)->SetTarget(this);

    applistview->SetTarget(this);

    for(const auto& command : { KRV_APPS_REMOVE, KRV_APPS_COPY })
        applsttoolbar->FindButton(command)->SetTarget(this);
}

void KeyringView::MessageReceived(BMessage* msg)
{
    switch(msg->what)
    {
        case KRV_KEYS_SEL:
            if(keylistview->CountRows() > 0) {
                keylsttoolbar->FindButton(KRV_KEYS_REMOVE)->SetEnabled(true);
                keylsttoolbar->FindButton(KRV_KEYS_COPY)->SetEnabled(true);
                keylsttoolbar->FindButton(KRV_KEYS_VWDATA)->SetEnabled(true);
            }
            break;
        case KRV_KEYS_CREATE:
        {
            AddKeyDialogBox* dlg = new AddKeyDialogBox(BRect(20, 20, 200, 200), ks,
                keyringname, B_KEY_TYPE_GENERIC);
            dlg->Show();
            break;
        }
        case KRV_KEYS_REMOVE:
        {
            if(keylistview->CurrentSelection()) {
                const char* sel = ((BStringField*)keylistview->CurrentSelection()->GetField(0))->String();

                BString desc(B_TRANSLATE("Do you want to remove the key \"%key%\" from this keyring?"));
                desc.ReplaceAll("%key%", sel);
                BAlert* alert = new BAlert(B_TRANSLATE("Remove key"), desc,
                    B_TRANSLATE("Remove"), B_TRANSLATE("Cancel"), NULL,
                    B_WIDTH_AS_USUAL, B_WARNING_ALERT);
                alert->SetShortcut(1, B_ESCAPE);
                int32 result = alert->Go();
                if(result == 0) {
                    _RemoveKey(ks, sel);
                    keylsttoolbar->FindButton(KRV_KEYS_REMOVE)->SetEnabled(false);
                    keylsttoolbar->FindButton(KRV_KEYS_COPY)->SetEnabled(false);
                    keylsttoolbar->FindButton(KRV_KEYS_VWDATA)->SetEnabled(false);
                    // Update(NULL);
                }
            }
            else {
                BAlert* alert = new BAlert(B_TRANSLATE("Error"),
                    B_TRANSLATE("An entry must be selected first."),
                    B_TRANSLATE("OK"), NULL, NULL,
                    B_WIDTH_AS_USUAL, B_STOP_ALERT);
                int32 result = alert->Go();
            }
            break;
        }
        case KRV_KEYS_INVOKE:
        case KRV_KEYS_VWDATA:
        {
            if(keylistview->CurrentSelection()) {
                const char* selection = ((BStringField*)keylistview->CurrentSelection()->GetField(0))->String();
                DataViewerDialogBox* dlg = new DataViewerDialogBox(
                    BRect(10, 10, 400, 300), keyringname, selection,
                    ks->KeyringByName(keyringname)->KeyByIdentifier(selection)->Type());
                dlg->Show();
            }
            break;
        }
        case KRV_KEYS_COPY:
            _CopyKey(ks);
            break;
        case KRV_APPS_SEL:
            if(applistview->CountRows() > 0) {
                applsttoolbar->FindButton(KRV_APPS_REMOVE)->SetEnabled(true);
                applsttoolbar->FindButton(KRV_APPS_COPY)->SetEnabled(true);
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
                    applsttoolbar->FindButton(KRV_APPS_REMOVE)->SetEnabled(false);
                    applsttoolbar->FindButton(KRV_APPS_COPY)->SetEnabled(false);
                    // Update(NULL);
                }
            }
            else {
                BAlert* alert = new BAlert(B_TRANSLATE("Error"),
                    B_TRANSLATE("An entry must be selected first."),
                    B_TRANSLATE("OK"), NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
                int32 result = alert->Go();
            }
            break;
        }
        case KRV_APPS_COPY:
            _CopyAppSignature(ks);
            break;
        default:
            BView::MessageReceived(msg);
            break;
    }
}

void KeyringView::Update(const void* data)
{
    fprintf(stderr, "[%s] Called update\n", Name());

    keylistview->Clear();
    applistview->Clear();

    // ks->Reset();

    _InitAppData(ks);
}

void KeyringView::_InitAppData(KeystoreImp* ks)
{
    BRow* row = NULL;

    for(int i = 0; i < ks->KeyringByName(keyringname)->KeyCount(); i++) {
        row = new BRow();

        row->SetField(new BStringField(ks->KeyringByName(keyringname)->KeyAt(i)->Identifier()), 0);
        // row->SetField(new CfIconStringField(ks->KeyringByName(keyringname)->KeyAt(i)->Identifier(), keyIcon), 0);

        row->SetField(new BStringField(ks->KeyringByName(keyringname)->KeyAt(i)->SecondaryIdentifier()), 1);

        const char* type;
        switch(ks->KeyringByName(keyringname)->KeyAt(i)->Type())
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
        switch(ks->KeyringByName(keyringname)->KeyAt(i)->Purpose())
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
        BRoster roster;
        app_info info;
        status_t status;
        const char* name;
        if((status = roster.GetAppInfo(signature, &info)) == B_OK)
            name = info.ref.name;
        else
            name = "";

        row->SetField(new BStringField(name), 0);
        row->SetField(new BStringField(signature), 1);

        applistview->AddRow(row);
    }
}

void KeyringView::_CopyKey(KeystoreImp* ks)
{
    BKeyStore keystore;
    const char* keyid = ((BStringField*)keylistview->CurrentSelection()->GetField(0))->String();
    BKeyType type = ks->KeyringByName(keyringname)->KeyByIdentifier(keyid)->Type();

    if(type == B_KEY_TYPE_PASSWORD) {
        BPasswordKey pwdkey;
        keystore.GetKey(keyringname, type, keyid, pwdkey);
        _CopyData(ks, keylistview, (const uint8*)pwdkey.Password());
    }
    else {
        BKey key;
        keystore.GetKey(keyringname, type, keyid, key);
        _CopyData(ks, keylistview, key.Data());
    }
}

void KeyringView::_RemoveKey(KeystoreImp* ks, const char* _id)
{
    if(ks->KeyringByName(keyringname)->DeleteKey(_id) != B_OK)
        fprintf(stderr, "Key %s in %s could not be removed.\n",
            _id, keyringname);
    else
        fprintf(stderr, "Key %s in %s seems to be removed successfully.\n",
            _id, keyringname);

    BMessenger msgr(be_app_messenger);
    BMessage rmmsg(M_USER_REMOVES_KEY);
    rmmsg.AddString("owner", keyringname);
    msgr.SendMessage(&rmmsg);
}

void KeyringView::_RemoveApp(KeystoreImp* ks, const char* _app)
{
    if(ks->KeyringByName(keyringname)->RemoveApplication(_app) != B_OK)
        fprintf(stderr, "Application %s in %s could not be removed.\n",
        _app, keyringname);
    else
        fprintf(stderr, "Application %s in %s seems to be removed successfully.\n",
        _app, keyringname);

    BMessenger msgr(be_app_messenger);
    BMessage rmmsg(M_USER_REMOVES_APPAUTH);
    rmmsg.AddString("owner", keyringname);
    msgr.SendMessage(&rmmsg);
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
