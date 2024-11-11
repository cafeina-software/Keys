/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <StorageKit.h>
#include <Catalog.h>
#include <DateTime.h>
#include <KeyStore.h>
#include <private/interface/AboutWindow.h>
#include <cstdio>
#include "KeysApplication.h"
#include "KeysWindow.h"
#include "../KeysDefs.h"
#include "../data/BackUpUtils.h"
#include "../data/KeystoreImp.h"

KeysApplication::KeysApplication()
: BApplication(kAppSignature), frame(BRect(50, 50, 720, 480))
{
    /* Start the server if not yet started */
    _RestartServer();

    _InitAppData(&ks, &keystore);
    _LoadSettings();
    window = new KeysWindow(frame, &ks, &keystore);
}

void KeysApplication::ReadyToRun()
{
    window->Show();
}

void KeysApplication::MessageReceived(BMessage* msg)
{
    switch(msg->what)
    {
        case B_QUIT_REQUESTED:
            QuitRequested();
            break;
        case B_ABOUT_REQUESTED:
            AboutRequested();
            break;
        case I_SERVER_RESTART:
            _RestartServer();
            break;

        case M_KEYSTORE_BACKUP:
            KeystoreBackup(msg);
            break;
        case M_KEYSTORE_WIPE_CONTENTS:
            WipeKeystoreContents(msg);
            break;

        case M_KEYRING_CREATE:
            AddKeyring(msg);
            break;
        case M_KEYRING_DELETE:
            RemoveKeyring(msg);
            break;
        case M_KEYRING_WIPE_CONTENTS:
            WipeKeyringContents(msg);
            break;
        case M_KEYRING_LOCK:
            LockKeyring(msg);
            break;
        case M_KEYRING_SET_LOCKKEY:
            SetKeyringLockKey(msg);
            break;
        case M_KEYRING_UNSET_LOCKKEY:
            RemoveKeyringLockKey(msg);
            break;

        case M_KEY_CREATE:
            AddKey(msg);
            break;
        case M_KEY_IMPORT:
            ImportKey(msg);
            break;
        case M_KEY_EXPORT:
            ExportKey(msg);
            break;
        case M_KEY_DELETE:
            RemoveKey(msg);
            break;

        case M_APPAUTH_DELETE:
            RemoveApp(msg);
            break;
        default:
            BApplication::MessageReceived(msg);
            break;
    }
}

bool KeysApplication::QuitRequested()
{
    _SaveSettings();
    return BApplication::QuitRequested();
}

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "About box"

void KeysApplication::AboutRequested()
{
	const char* extraCopyrights[] = {
		// If you helped me with this program, add yourself:
		// "<Year span> <Your name>",
		NULL
	};
	const char* authors[] = {
		B_TRANSLATE("Cafeina (original author)"),
		// If you helped me with this program, add yourself:
		// B_TRANSLATE("<Your name> (<your role>)"),
		NULL
	};
	const char* website[] = {
        kAppHomePage,
		NULL
	};
	const char* thanks[] = {
		B_TRANSLATE("The BeOS and Haiku community members."),
		NULL
	};
	const char* history[] = {
		B_TRANSLATE("0.1\tInitial version."),
		NULL
	};
    const char* extrainfo = B_TRANSLATE(
#if defined(USE_OPENSSL)
        "This software makes use of the OpenSSL library.\n"
#endif
        "Check the README for more information."
    );

	BAboutWindow *about = new BAboutWindow(B_TRANSLATE_SYSTEM_NAME(kAppName),
		kAppSignature);
	about->AddDescription(B_TRANSLATE("Haiku key store manager."));
	about->AddCopyright(2024, "Cafeina", extraCopyrights);
	about->AddAuthors(authors);
	about->AddSpecialThanks(thanks);
	about->AddVersionHistory(history);
	about->AddExtraInfo(extrainfo);
	about->AddText(B_TRANSLATE("Website:"), website);
	about->SetVersion(kAppVersionStr);
	about->Show();
}

// #pragma mark -

void KeysApplication::KeystoreBackup(BMessage* msg)
{
    uint32 method;
    BString password;
    if(msg->FindUInt32("method", &method) != B_OK ||
    msg->FindString("password", &password) != B_OK)
        return;

    switch(method) {
        case 'copy': {
            DoPlainKeystoreBackup();
            return;
        }
#if defined(USE_OPENSSL)
        case 'ssl ':
            // DoEncryptedKeystoreBackup(password.String());
            return;
#endif
        default:
            return;
    }
}

void KeysApplication::WipeKeystoreContents(BMessage* msg)
{
    int32 count = ks.KeyringCount();
    for(int i = count - 1; i >= 0; i--) {
        // Master is protected, we can only clean it up
        if(strcmp(ks.KeyringAt(i)->Identifier(), "Master") == 0) {
            BMessage data;
            data.AddString(kConfigKeyring, "Master");
            WipeKeyringContents(&data);
        }
        // For the others, we can straight up delete them
        else ks.RemoveKeyring(ks.KeyringAt(i)->Identifier(), true);
    }
    window->Update();
}

void KeysApplication::AddKeyring(BMessage* msg)
{
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK)
        return;

    if(ks.KeyringByName(keyring.String()))
        return;

    (void)ks.AddKeyring(keyring.String(), true);
    // we could here refill the database to retrieve entries under the keyring,
    //  but at this point, the keyring is empty anyways, so let's save cycles

    void* ptr = nullptr;
    if(msg->FindPointer(kConfigWho, &ptr) == B_OK)
        ((KeysWindow*)ptr)->Update();
}

void KeysApplication::LockKeyring(BMessage* msg)
{
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK)
        return;

    if(ks.KeyringByName(keyring.String()) == nullptr)
        return;

    status_t status = ks.KeyringByName(keyring.String())->Lock();

    _Notify(nullptr, msg, status);
}

void KeysApplication::SetKeyringLockKey(BMessage* msg)
{
    BString keyring;
    BMessage key;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    msg->FindMessage(kConfigKey, &key) != B_OK)
        return;

    if(ks.KeyringByName(keyring.String()) == nullptr)
        return;

    (void)ks.KeyringByName(keyring.String())->SetUnlockKey(&key);

    void* ptr = nullptr;
    if(msg->FindPointer(kConfigWho, &ptr) == B_OK)
        ((KeysWindow*)ptr)->Update();
}

void KeysApplication::RemoveKeyringLockKey(BMessage* msg)
{
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK)
        return;

    if(ks.KeyringByName(keyring.String()) == nullptr)
        return;

    status_t status = ks.KeyringByName(keyring.String())->RemoveUnlockKey();

    _Notify(nullptr, msg, status);
}

void KeysApplication::WipeKeyringContents(BMessage* msg)
{
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK) {
        __trace("Error: data member \"kConfigKeyring\" not found.\n");
        return;
    }

    if(ks.KeyringByName(keyring.String()) == nullptr)
        return;

    KeyringImp* target = ks.KeyringByName(keyring.String());
    int count = target->KeyCount();
    for(int i = count - 1 ; i >= 0; i--)
        target->RemoveKey(target->KeyAt(i)->Identifier(), true);

    _Notify((void*)msg->GetPointer(kConfigWho, nullptr), msg, B_OK);
}

void KeysApplication::RemoveKeyring(BMessage* msg)
{
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK)
        return;

    if(ks.KeyringByName(keyring.String()) == nullptr)
        return;

    status_t status = ks.RemoveKeyring(keyring.String(), true);

    _Notify(nullptr, msg, status);
}

void KeysApplication::AddKey(BMessage* msg)
{
    BString keyring = msg->GetString(kConfigKeyring);
    if(ks.KeyringByName(keyring.String()) == nullptr) {
        return;
    }

    BString id, sec;
    BKeyType t;
    BKeyPurpose p;
    const void* data = nullptr;
    ssize_t length = 0;
    if(msg->FindString(kConfigKeyName, &id) != B_OK ||
    msg->FindString(kConfigKeyAltName, &sec) != B_OK ||
    msg->FindUInt32(kConfigKeyPurpose, (uint32*)&p) != B_OK ||
    msg->FindUInt32(kConfigKeyType, (uint32*)&t) != B_OK ||
    msg->FindData(kConfigKeyData, B_RAW_TYPE, &data, &length) != B_OK)
        return;

    /* Check for duplicates. The API seems to allow partial duplicates
        as long as they are either of different type or have different
        secondary identifier.

       This app is currently unable to deal with such partial duplicates.
    */
    KeyImp* key = ks.KeyringByName(keyring.String())->KeyByIdentifier(id.String());
    if(key /*&& key->Type() == t &&
    strcmp(key->Identifier(), id.String()) == 0 &&
    strcmp(key->SecondaryIdentifier(), sec.String()) == 0*/) {
        BMessage answer(msg->what);
        answer.AddUInt32("result", B_NAME_IN_USE);
        window->PostMessage(&answer);
        return;
    }

    ks.KeyringByName(keyring.String())->AddKey(p, t,
        id.String(), sec.String(), (const uint8*)data, length, true);

    void* ptr = (void*)msg->GetPointer(kConfigWho, nullptr);
    if(ptr)
        ((KeyringView*)ptr)->Update();
}

void KeysApplication::ImportKey(BMessage* msg)
{
    entry_ref ref;
    BString keyring;
    if(msg->FindRef("refs", &ref) != B_OK ||
    msg->FindString(kConfigKeyring, &keyring) != B_OK) {
        fprintf(stderr, "Error: bad data. No reference or keyring name received.\n");
        return;
    }

    BFile file(&ref, B_READ_ONLY);
    if(file.InitCheck() != B_OK) {
        fprintf(stderr, "Error: no file from where import.\n");
        return;
    }

    BMessage* archive = new BMessage;
    if(archive->Unflatten(&file) != B_OK) {
        fprintf(stderr, "Error: the message could not be unflattened from file.\n");
        delete archive;
        return;
    }

    BString id;
    if(archive->FindString("identifier", &id) != B_OK)
        fprintf(stderr, "Error, field not found here.\n");

    status_t status;
    if(ks.KeyringByName(keyring.String())->KeyByIdentifier(id.String()) != nullptr) {
        fprintf(stderr, "Error: there is already a key with this name.\n");
        status = B_NAME_IN_USE;
        goto exit;
    }

    if((status = ks.KeyringByName(keyring.String())->ImportKey(archive)) != B_OK) {
        fprintf(stderr, "Error: the key could not be successfully added to the keystore.\n");
        goto exit;
    }

exit:
    _Notify((void*)msg->GetPointer(kConfigWho, nullptr), msg, status);
    delete archive;
}

void KeysApplication::ExportKey(BMessage* msg)
{
    status_t status = B_OK;

    entry_ref dirref;
    BString name, keyring, id;
    if(msg->FindRef("directory", &dirref) != B_OK ||
    msg->FindString("name", &name) != B_OK ||
    msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    msg->FindString(kConfigKeyName, &id) != B_OK)
        return;

    BMessage* archive = new BMessage;
    if((status = ks.KeyringByName(keyring.String())->KeyByIdentifier(id.String())->Export(archive)) != B_OK) {
        delete archive;
        return;
    }

    BDirectory directory(&dirref);
    BFile file(&directory, name.String(), B_READ_WRITE | B_CREATE_FILE | B_FAIL_IF_EXISTS);
    if((status = file.InitCheck()) != B_OK) {
        delete archive;
        return;
    }
    status = archive->Flatten(&file);

    _Notify(nullptr, msg, status);

    delete archive;
}

void KeysApplication::RemoveKey(BMessage* msg)
{
    BString keyring, id;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    msg->FindString(kConfigKeyName, &id) != B_OK)
        return;

    status_t status = ks.KeyringByName(keyring.String())->RemoveKey(id.String(), true);

    _Notify(nullptr, msg, status);
}

void KeysApplication::RemoveApp(BMessage* msg)
{
    BString keyring, signature;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    msg->FindString(kConfigSignature, &signature) != B_OK)
        return;

    status_t status = ks.KeyringByName(keyring.String())->RemoveApplication(signature.String(), true);

    _Notify(nullptr, msg, status);
}

// #pragma mark -

void KeysApplication::_InitAppData(KeystoreImp* ks, BKeyStore* keystore)
{
    bool next = true;
    uint32 keyringCookie = 0;
    BString keyringName;

    ks->Reset();

    while(next) {
        switch(keystore->GetNextKeyring(keyringCookie, keyringName))
        {
            case B_OK:
            {
                fprintf(stderr, "Found keyring: %s\n", keyringName.String());
                ks->AddKeyring(keyringName.String());
                _InitKeyring(ks, keystore, keyringName.String());
                break;
            }
            case B_ENTRY_NOT_FOUND:
                fprintf(stderr, "no keyrings left\n");
                next = false;
                break;
            default:
                fprintf(stderr, "%s comms error\n", keyringName.String());
                next = false;
                break;
        }

    }
}

void KeysApplication::_InitKeyring(KeystoreImp* ks, BKeyStore* keystore, const char* kr)
{
    bool next = true;
    uint32 keyCookie = 0;
    BKey key;

    ks->KeyringByName(kr)->Reset();

    while(next) {
        switch(keystore->GetNextKey(kr, B_KEY_TYPE_GENERIC, B_KEY_PURPOSE_ANY,
            keyCookie, key))
        {
            case B_OK:
                ks->KeyringByName(kr)->AddKey(key.Purpose(), key.Type(),
                    key.Identifier(), key.SecondaryIdentifier());
                break;
            case B_BAD_VALUE:
                // fprintf(stderr, "%s : bad value, this should not have happened\n", kr);
                next = false;
                break;
            case B_NOT_ALLOWED:
                // fprintf(stderr, "%s : access denied\n", kr);
                next = false;
                break;
            case B_ENTRY_NOT_FOUND:
                // fprintf(stderr, "%s : keys not found\n", kr);
                next = false;
                break;
            default:
                // fprintf(stderr, "%s : comms error\n", kr);
                next = false;
                break;
        }
    }

    next = true;
    keyCookie = 0;
    BPasswordKey pwdkey;

    while(next) {
        switch(keystore->GetNextKey(kr, B_KEY_TYPE_PASSWORD, B_KEY_PURPOSE_ANY,
            keyCookie, pwdkey))
        {
            case B_OK:
                ks->KeyringByName(kr)->AddKey(pwdkey.Purpose(), pwdkey.Type(),
                    pwdkey.Identifier(), pwdkey.SecondaryIdentifier());
                break;
            case B_BAD_VALUE:
                // fprintf(stderr, "%s : bad value, this should not have happened\n", kr);
                next = false;
                break;
            case B_NOT_ALLOWED:
                // fprintf(stderr, "%s : access denied\n", kr);
                next = false;
                break;
            case B_ENTRY_NOT_FOUND:
                // fprintf(stderr, "%s : keys not found\n", kr);
                next = false;
                break;
            default:
                // fprintf(stderr, "%s : comms error\n", kr);
                next = false;
                break;
        }
    }

    next = true;
    uint32 appCookie = 0;
    BString appSignature;

    while(next) {
        switch(keystore->GetNextApplication(kr, appCookie, appSignature))
        {
            case B_OK:
                ks->KeyringByName(kr)->AddApplicationToList(appSignature.String());
                break;
            case B_BAD_VALUE:
            case B_NOT_ALLOWED:
            case B_ENTRY_NOT_FOUND:
            default:
                next = false;
                break;
        }
    }
}

void KeysApplication::_Notify(void* ptr, BMessage* msg, status_t result)
{
    BMessage reply(msg->what);
    reply.AddUInt32(kConfigWhat, msg->what);
    reply.AddInt32(kConfigResult, static_cast<int>(result));

    if(ptr)
        reply.AddPointer(kConfigWho, ptr);

    msg->SendReply(&reply);
}

status_t KeysApplication::_RestartServer()
{
    status_t status = B_OK;
    BMessenger msgr("application/x-vnd.Haiku-keystore_server");
    if(!msgr.IsValid()) {
        fprintf(stderr, "Keystore server not running. Starting it...\n");
        status = be_roster->Launch("application/x-vnd.Haiku-keystore_server");
        if(status != B_OK && status != B_ALREADY_RUNNING) {
            fprintf(stderr, "Error. The server could not be run successfully.\n");
            return B_ERROR;
        }
    }
    return status;
}

status_t KeysApplication::_LoadSettings()
{
    status_t status = B_OK;
    BPath usrsetpath;
    if((status = find_directory(B_USER_SETTINGS_DIRECTORY, &usrsetpath)) != B_OK) {
        return status;
    }

    usrsetpath.Append(kAppName ".settings");
    BFile file(usrsetpath.Path(), B_READ_ONLY);
    if((status = file.InitCheck()) != B_OK) {
        return status;
    }

    if((status = currentSettings.Unflatten(&file)) != B_OK) {
        return status;
    }

    if(currentSettings.FindRect("frame", &frame) != B_OK)
        return B_ERROR;

    return status;
}

status_t KeysApplication::_SaveSettings()
{
    status_t status = B_OK;

    if(currentSettings.ReplaceRect("frame", window->Frame()) != B_OK)
        currentSettings.AddRect("frame", window->Frame());

    BPath usrsetpath;
    if((status = find_directory(B_USER_SETTINGS_DIRECTORY, &usrsetpath)) != B_OK) {
        return status;
    }

    usrsetpath.Append(kAppName ".settings");
    BFile file(usrsetpath.Path(), B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
    if((status = file.InitCheck()) != B_OK) {
        return status;
    }
    file.SetPermissions(DEFFILEMODE);

    if((status = currentSettings.Flatten(&file)) != B_OK) {
        return status;
    }

    return status;
}
