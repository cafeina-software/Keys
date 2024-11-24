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
#include "../data/PasswordStrength.h"

const char* kKeyStoreServerSignature = "application/x-vnd.Haiku-keystore_server";

KeysApplication::KeysApplication()
: BApplication(kAppSignature), frame(BRect(50, 50, 720, 480)),
  ks(new KeystoreImp())
{
    /* Start the server if not yet started */
    _RestartServer(false);

    _InitAppData(ks, &keystore);
    _LoadSettings();
    window = new KeysWindow(frame, ks, &keystore);

    /* Node monitor */
    BPath path;
    DBPath(&path);
    BEntry(path.Path()).GetNodeRef(&databaseNRef);
    watch_node(&databaseNRef, B_WATCH_ALL, this);
}

KeysApplication::~KeysApplication()
{
    watch_node(&databaseNRef, B_STOP_WATCHING, this);
    delete ks;
}

void KeysApplication::ReadyToRun()
{
    if(!BScreen().Frame().Contains(frame)) {
        // If the window goes offscreen, it moves it until it is fully visible
        window->MoveOnScreen(B_MOVE_IF_PARTIALLY_OFFSCREEN);
        frame = window->Frame();
    }
    window->Show();
}

void KeysApplication::MessageReceived(BMessage* msg)
{
    switch(msg->what)
    {
        case B_NODE_MONITOR:
            // _InitAppData(&ks, &keystore);
            // window->PostMessage(new BMessage(I_DATA_REFRESH));
            break;
        case B_QUIT_REQUESTED:
            QuitRequested();
            break;
        case B_ABOUT_REQUESTED:
            AboutRequested();
            break;
        case M_ASK_FOR_REFRESH:
        {
            // Someone asked for an update to its respective entry in the data model
            BMessage reply;
            BString keyring;
            status_t status = B_ERROR;
            if(msg->FindString(kConfigKeyring, &keyring) == B_OK &&
            ks->KeyringByName(keyring.String()) != nullptr) {
                ks->KeyringByName(keyring.String())->Reset();
                _InitKeyring(ks, &keystore, keyring.String());
                status = B_OK;
            }
            reply.AddInt32("result", status);
            msg->SendReply(&reply);
            break;
        }
        case I_SERVER_RESTART:
            _RestartServer(false, false);
            break;
        case I_SERVER_STOP:
            _StopServer(true);
            break;

        case M_KEYSTORE_BACKUP:
            KeystoreBackup(msg);
            break;
        case M_KEYSTORE_RESTORE:
            KeystoreRestore(msg);
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
        case M_KEY_GENERATE_PASSWORD:
            GeneratePwdKey(msg);
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
    BString verinfo(B_TRANSLATE("Version %versionString%: \"%versionName%\"."));
    verinfo.ReplaceAll("%versionString%", "0.1");
    verinfo.ReplaceAll("%versionName%", "Carlos");
    const char* extrainfo = verinfo.String();
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
        B_TRANSLATE("0.1.0\tData model and messaging rework. More comprehensive API coverage. Bugfixes."),
		B_TRANSLATE("0.0.1\tInitial version."),
		NULL
	};
    const char* assist [] = {B_TRANSLATE(
#if defined(USE_OPENSSL)
        "This software makes use of the OpenSSL library.\n"
#endif
        "Check the README for general information.\n\t"
        "Please read the help file to get more information about how to use this application."
    ),NULL};


	BAboutWindow *about = new BAboutWindow(B_TRANSLATE_SYSTEM_NAME(kAppName),
		kAppSignature);
	about->AddDescription(B_TRANSLATE("Haiku key store manager."));
	about->AddCopyright(2024, "Cafeina", extraCopyrights);
	about->AddExtraInfo(extrainfo);
    about->AddAuthors(authors);
	about->AddText(B_TRANSLATE("Website:"), website);
    about->AddSpecialThanks(thanks);
	about->AddVersionHistory(history);
    about->AddText(B_TRANSLATE("Assistance:"), assist);
	about->SetVersion(kAppVersionStr);

    if(window) {
        about->ResizeTo(window->Frame().Width() / 2, window->Frame().Height() / 1.5);
        about->CenterIn(window->Frame());
    }
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
// #if defined(USE_OPENSSL)
        case 'ssl ':
            DoEncryptedKeystoreBackup(password.String());
            return;
// #endif
        default:
            return;
    }
}

void KeysApplication::KeystoreRestore(BMessage* msg)
{
    entry_ref ref;
    BString pass;
    msg->FindRef("refs", &ref);

    BFile datafile(&ref, B_READ_ONLY);
    BPath path(&ref);
    BMessage data;
    data.Unflatten(&datafile);
    pass = data.GetString("pass");

    if(RestoreEncryptedKeystoreBackup(path.Path(), pass.String()) == B_OK)
        _RestartServer(true);
}

void KeysApplication::WipeKeystoreContents(BMessage* msg)
{
    // Not in the API, it's just a convenience method to quickly clean the database,
    //  mostly for dev purposes
    int32 count = ks->KeyringCount();
    for(int i = count - 1; i >= 0; i--) {
        // Master is protected, we can only clean it up
        if(strcmp(ks->KeyringAt(i)->Identifier(), "Master") == 0) {
            BMessage data;
            data.AddString(kConfigKeyring, "Master");
            WipeKeyringContents(&data);
        }
        // For the others, we can straight up delete them
        else ks->RemoveKeyring(ks->KeyringAt(i)->Identifier(), true);
    }
    window->Update();
}

void KeysApplication::AddKeyring(BMessage* msg)
{
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK) {
        fprintf(stderr, "Error: bad data. No keyring name received.\n");
        return;
    }

    if(ks->KeyringByName(keyring.String())) {
        fprintf(stderr, "Error: the keyring name received is already in use.\n");
        return;
    }

    if(ks->AddKeyring(keyring.String(), true) != B_OK) {
        fprintf(stderr, "Error: the keyring could not be created in the store.\n");
        return;
    }
    // we could here refill the database to retrieve entries under the keyring,
    //  but at this point, the keyring is empty anyways, so let's save cycles
    fprintf(stderr, "Info: keyring \"%s\" was successfully created.\n", keyring.String());

    void* ptr = nullptr;
    if(msg->FindPointer(kConfigWho, &ptr) == B_OK)
        ((KeysWindow*)ptr)->Update((const void*)keyring.String());
}

void KeysApplication::LockKeyring(BMessage* msg)
{
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        fprintf(stderr, "Error: bad data. No keyring name received or bad keyring name.\n");
        return;
    }

    status_t status = ks->KeyringByName(keyring.String())->Lock();

    _Notify(nullptr, msg, status);
}

void KeysApplication::SetKeyringLockKey(BMessage* msg)
{
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        fprintf(stderr, "Error: bad data. No keyring name received or bad keyring name.\n");
        return;
    }

    const void* data = nullptr;
    ssize_t length = 0;
    if(msg->FindData(kConfigKeyData, B_RAW_TYPE, &data, &length) != B_OK) {
        fprintf(stderr, "Error: bad data. There are missing fields.\n");
        return;
    }

    BMessage* key = new BMessage;
    BPasswordKey* dummy = new BPasswordKey(reinterpret_cast<const char*>(data),
        B_KEY_PURPOSE_KEYRING, "", NULL);
    dummy->Flatten(*key);
    delete dummy;

    if(ks->KeyringByName(keyring.String())->SetUnlockKey(key) != B_OK) {
        fprintf(stderr, "Error: The unlock key could not be applied to the keyring in the store.\n");
        delete key;
        return;
    }

    delete key;
    void* ptr = nullptr;
    if(msg->FindPointer(kConfigWho, &ptr) == B_OK)
        ((KeyringView*)ptr)->Update();
    // ((KeysWindow*)window)->Update();
}

void KeysApplication::RemoveKeyringLockKey(BMessage* msg)
{
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        fprintf(stderr, "Error: bad data. No keyring name received or bad keyring name.\n");
        return;
    }

    status_t status = ks->KeyringByName(keyring.String())->RemoveUnlockKey();
    if(status != B_OK) {
        fprintf(stderr, "Error: The unlock key of this keyring could not be removed in the store.\n");
        // no return, let's notify first
    }

    _Notify(nullptr, msg, status);
}

void KeysApplication::WipeKeyringContents(BMessage* msg)
{
    // Not in the API, it's just a convenience method to quickly clean the keyring,
    //  mostly for dev purposes
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        fprintf(stderr, "Error: bad data. No keyring name received or bad keyring name.\n");
        return;
    }

    KeyringImp* target = ks->KeyringByName(keyring.String());
    int count = target->KeyCount();
    for(int i = count - 1 ; i >= 0; i--)
        target->RemoveKey(target->KeyAt(i)->Identifier(), true);

    _Notify((void*)msg->GetPointer(kConfigWho, nullptr), msg, B_OK);
}

void KeysApplication::RemoveKeyring(BMessage* msg)
{
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        fprintf(stderr, "Error: bad data. No keyring name received or bad keyring name.\n");
        return;
    }

    status_t status = ks->RemoveKeyring(keyring.String(), true);
    if(status != B_OK)
        fprintf(stderr, "Error: the keyring %s could not be removed from the store.\n", keyring.String());

    _Notify(nullptr, msg, status);
}

void KeysApplication::AddKey(BMessage* msg)
{
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        fprintf(stderr, "Error: bad data. No keyring name received or bad keyring name.\n");
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
    msg->FindData(kConfigKeyData, B_RAW_TYPE, &data, &length) != B_OK) {
        fprintf(stderr, "Error: bad data. There are missing fields.\n");
        return;
    }

    /* Check for duplicates. The API seems to allow partial duplicates
        as long as they are either of different type or have different
        secondary identifiers.
    */
    KeyImp* key = ks->KeyringByName(keyring.String())->KeyByIdentifier(id.String(), sec.String());
    if(key) {
        fprintf(stderr, "Error: key (%s, %s) in %s already exists.\n", id.String(), sec.String(), keyring.String());
        BMessage answer(msg->what);
        answer.AddUInt32("result", B_NAME_IN_USE);
        window->PostMessage(&answer);
        return;
    }

    ks->KeyringByName(keyring.String())->AddKey(p, t,
        id.String(), sec.String(), (const uint8*)data, length, true);

    fprintf(stderr, "Info: the key (%s, %s) was created in %s keyring successfully.\n",
        id.String(), sec.String(), keyring.String());

    void* ptr = (void*)msg->GetPointer(kConfigWho);
    if(ptr)
        ((KeyringView*)ptr)->Update();
}

void KeysApplication::GeneratePwdKey(BMessage* msg)
{
    status_t status = B_OK;
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        fprintf(stderr, "Error: bad data. No keyring name received or bad keyring name.\n");
        return;
    }

    BString id, sec;
    BKeyPurpose p;
    uint32 length;
    if(msg->FindString(kConfigKeyName, &id) != B_OK ||
    msg->FindUInt32(kConfigKeyPurpose, (uint32*)&p) != B_OK ||
    msg->FindUInt32(kConfigKeyGenLength, &length) != B_OK) {
        fprintf(stderr, "Error: bad data. There are missing fields.\n");
        return;
    }

    if(msg->FindString(kConfigKeyAltName, &sec) != B_OK)
        sec.SetTo(""); // default if not set or missing

    BPasswordKey pwdkey("", p, id.String(), sec.String());
    status = GeneratePassword(pwdkey, length, 0);
    BMessage data;
    pwdkey.Flatten(data);
    status = ks->KeyringByName(keyring.String())->ImportKey(&data);

    ((KeyringView*)msg->GetPointer(kConfigWho))->Update((void*)&status);
}

void KeysApplication::ImportKey(BMessage* msg)
{
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        fprintf(stderr, "Error: bad data. No keyring name received or bad keyring name.\n");
        return;
    }

    entry_ref ref;
    if(msg->FindRef("refs", &ref) != B_OK) {
        fprintf(stderr, "Error: bad data. No entry reference received.\n");
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

    status_t status;
    BString id, sec;
    if(archive->FindString("identifier", &id) != B_OK ||
    archive->FindString("secondaryIdentifier", &sec) != B_OK) {
        fprintf(stderr, "Error: key message with missing fields.\n");
        status = B_BAD_DATA;
        goto exit;
    }

    if(ks->KeyringByName(keyring.String())->KeyByIdentifier(id.String(), sec.String()) != nullptr) {
        fprintf(stderr, "Error: there is already a key with this name.\n");
        status = B_NAME_IN_USE;
        goto exit;
    }

    if((status = ks->KeyringByName(keyring.String())->ImportKey(archive)) != B_OK) {
        fprintf(stderr, "Error: the key could not be successfully added to the keystore.\n");
        goto exit;
    }

exit:
    _Notify((void*)msg->GetPointer(kConfigWho, nullptr), msg, status);
    delete archive;
}

void KeysApplication::ExportKey(BMessage* msg)
{
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        fprintf(stderr, "Error: bad data. No keyring name received or bad keyring name.\n");
        return;
    }

    status_t status = B_OK;
    entry_ref dirref;
    BString name, id, alt;
    if(msg->FindRef("directory", &dirref) != B_OK ||
    msg->FindString("name", &name) != B_OK ||
    msg->FindString(kConfigKeyName, &id) != B_OK ||
    msg->FindString(kConfigKeyAltName, &alt) != B_OK) {
        fprintf(stderr, "Error: bad data. There are missing fields.\n");
        return;
    }

    BMessage* archive = new BMessage;
    if((status = ks->KeyringByName(keyring.String())->KeyByIdentifier(id.String(), alt.String())->Export(archive)) != B_OK) {
        fprintf(stderr, "Error: the key (%s, %s) in %s could not be exported to a flattened BMessage.\n",
            id.String(), alt.String(), keyring.String());
        delete archive;
        return;
    }

    BDirectory directory(&dirref);
    BFile file(&directory, name.String(), B_READ_WRITE | B_CREATE_FILE | B_FAIL_IF_EXISTS);
    if((status = file.InitCheck()) != B_OK) {
        fprintf(stderr, "The file %s to where export the flattened BMessage could not be initialized.\n",
            BPath(&directory, name.String(), true).Path());
        delete archive;
        return;
    }
    status = archive->Flatten(&file);

    _Notify(nullptr, msg, status);

    delete archive;
}

void KeysApplication::RemoveKey(BMessage* msg)
{
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        fprintf(stderr, "Error: bad data. No keyring name received or bad keyring name.\n");
        return;
    }

    BString id, alt;
    if(msg->FindString(kConfigKeyName, &id) != B_OK ||
    msg->FindString(kConfigKeyAltName, &alt) != B_OK) {
        fprintf(stderr, "Error: bad data. There are missing fields.\n");
        return;
    }

    status_t status = ks->KeyringByName(keyring.String())->RemoveKey(id.String(), alt.String(), true);

    _Notify(nullptr, msg, status);
}

void KeysApplication::RemoveApp(BMessage* msg)
{
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        fprintf(stderr, "Error: bad data. No keyring name received or bad keyring name.\n");
        return;
    }

    BString signature;
    if(msg->FindString(kConfigSignature, &signature) != B_OK) {
        fprintf(stderr, "Error: bad data. There are missing fields.\n");
        return;
    }

    status_t status = ks->KeyringByName(keyring.String())->RemoveApplication(signature.String(), true);

    _Notify(nullptr, msg, status);
}

// #pragma mark -

void KeysApplication::_InitAppData(KeystoreImp*& ks, BKeyStore* keystore)
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
                fprintf(stderr, "Info: found keyring: %s\n", keyringName.String());
                ks->AddKeyring(keyringName.String());
                _InitKeyring(ks, keystore, keyringName.String());
                break;
            }
            case B_ENTRY_NOT_FOUND:
                fprintf(stderr, "Info: no keyrings left\n");
                next = false;
                break;
            default:
                fprintf(stderr, "Error: %s comms error\n", keyringName.String());
                next = false;
                break;
        }

    }
}

void KeysApplication::_InitKeyring(KeystoreImp*& ks, BKeyStore* keystore, const char* kr)
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

status_t KeysApplication::_StopServer(bool rebuildModel)
{
    if(be_roster->IsRunning(kKeyStoreServerSignature)) {
        team_id team = be_roster->TeamFor(kKeyStoreServerSignature);
        if(team == B_ERROR)
            return B_BAD_TEAM_ID;

        fprintf(stderr, "Info: trying to stop keystore server...\n");
        BMessenger msgr(kKeyStoreServerSignature, team);
        msgr.SendMessage(B_QUIT_REQUESTED);

        thServerMonitor = spawn_thread(_CallServerMonitor, "Keystore server monitor",
            B_NORMAL_PRIORITY, this);
        resume_thread(thServerMonitor);

        // here we should rebuild the data model
        if(rebuildModel) {
            fprintf(stderr, "Info: Rebuilding model...\n");
            ks->Reset();
            // _InitAppData(ks, &keystore);
            window->UpdateAsEmpty();
        }
    }
    return B_OK;
}

status_t KeysApplication::_RestartServer(bool rebuildModel, bool closeIfRunning)
{
    status_t status = B_OK;

    if(be_roster->IsRunning(kKeyStoreServerSignature) && closeIfRunning)
        _StopServer();

    BMessenger msgr(kKeyStoreServerSignature);
    if(!msgr.IsValid()) {
        fprintf(stderr, "Info: Keystore server not running. Starting it...\n");
        status = be_roster->Launch(kKeyStoreServerSignature);
        if(status != B_OK && status != B_ALREADY_RUNNING) {
            fprintf(stderr, "Error: The server could not be run successfully.\n");
            return B_ERROR;
        }
        // here we should rebuild the data model
        if(rebuildModel) {
            fprintf(stderr, "Info: Rebuilding model...\n");
            _RebuildModel();
        }
    }
    return status;
}

int32 KeysApplication::_CallServerMonitor(void* data)
{
    bool stop = false;
    app_info info;
    while(!stop) {
        fprintf(stderr, "Not running\n");
        snooze(500000);
        be_roster->GetAppInfo(kKeyStoreServerSignature, &info);
        if(info.team != -1)
            stop = true;
    }
    fprintf(stderr, "running\n");
    on_exit_thread(_CallRebuildModel, data);
    return 0;
}

void KeysApplication::_CallRebuildModel(void* data)
{
    ((KeysApplication*)data)->_RebuildModel();
}

void KeysApplication::_RebuildModel()
{
    ks->Reset();
    _InitAppData(ks, &keystore);
    window->Update();
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
