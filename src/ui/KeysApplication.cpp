/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <Application.h>
#include <Catalog.h>
#include <DateTime.h>
#include <FindDirectory.h>
#include <KeyStore.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <PropertyInfo.h>
#include <private/interface/AboutWindow.h>
#include <atomic>
#include <cstdio>
#include <unordered_map>
#include "KeysApplication.h"
#include "KeysWindow.h"
#include "../KeysDefs.h"
#include "../data/BackUpUtils.h"
#include "../data/CryptoUtils.h"
#include "../data/KeystoreImp.h"
#include "../data/PasswordStrength.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Scripting properties"

static property_info kKeysProperties[] = {
    {
        .name       = "StartServer",
        .commands   = { B_EXECUTE_PROPERTY, 0 },
        .specifiers = { B_DIRECT_SPECIFIER, 0 },
        .usage      = B_TRANSLATE("Keystore server: execution."),
        .extra_data = 0,
        .types      = { }
    },
    {
        .name       = "Keyrings",
        .commands   = { B_GET_PROPERTY, B_COUNT_PROPERTIES, 0 },
        .specifiers = { B_DIRECT_SPECIFIER, 0 },
        .usage      = B_TRANSLATE("Keyrings list: query information."),
        .extra_data = 0,
        .types      = { B_STRING_TYPE, B_INT32_TYPE }
    },
    {
        .name       = "Keyring",
        .commands   = { B_GET_PROPERTY, 0 },
        .specifiers = { B_NAME_SPECIFIER, B_INDEX_SPECIFIER, 0 },
        .usage      = B_TRANSLATE("Keyring: query information."),
        .extra_data = 0,
        .types      = { B_MESSAGE_TYPE }
    },
    {
        .name       = "Keyring",
        .commands   = { B_CREATE_PROPERTY, 0 },
        .specifiers = { B_DIRECT_SPECIFIER, 0 },
        .usage      = B_TRANSLATE("Keyring: creation."),
        .extra_data = 0,
        .types      = { B_STRING_TYPE }
    },
    {
        .name       = "Keyring",
        .commands   = { B_DELETE_PROPERTY, 0 },
        .specifiers = { B_DIRECT_SPECIFIER, B_NAME_SPECIFIER, 0 },
        .usage      = B_TRANSLATE("Keyring: deletion."),
        .extra_data = 0,
        .types      = { B_STRING_TYPE }
    },
    { 0 }
};
enum { PROPERTY_SERVER, PROPERTY_KEYRINGS, PROPERTY_KEYRING_READ, PROPERTY_KEYRING_CREATE, PROPERTY_KEYRING_DELETE };
const char* kKeyStoreServerSignature = "application/x-vnd.Haiku-keystore_server";

// #pragma mark -

KeysApplication::KeysApplication()
: BApplication(kAppSignature),
  window(NULL),
  frame(BRect(50, 50, 720, 480)),
  ks(new KeystoreImp()),
  inFocus(NULL),
  hasDataCopied(false)
{
    /* Start the server if not yet started */
    StartServer(false);

    /* Data initialization */
    LoadSettings();
    _InitAppData(&currentSettings);
    _InitKeystoreData(ks, &keystore);

    window = new KeysWindow(frame, ks, &keystore);

    /* Node monitor */
    BPath path;
    DBPath(&path);
    BEntry(path.Path()).GetNodeRef(&databaseNRef);
    watch_node(&databaseNRef, B_WATCH_ALL, this);

    /* Safety measures */
    clipboardCleanerRunner = new BMessageRunner(this,
        new BMessage(M_ASK_FOR_CLIPBOARD_CLEANUP), 30000000, -1);
}

KeysApplication::~KeysApplication()
{
    delete clipboardCleanerRunner;
    watch_node(&databaseNRef, B_STOP_WATCHING, this);
    delete ks;
}

bool KeysApplication::QuitRequested()
{
    SaveSettings();
    if(hasDataCopied) {
        BAlert* alert = new BAlert(B_TRANSLATE("Clipboard clean-up confirmation"),
        B_TRANSLATE("The system clipboard may still contain a copied key secret. "
        "Do you want to clear the clipboard before exiting?"),
        B_TRANSLATE("Clear clipboard"), B_TRANSLATE("Do not clear"), NULL,
        B_WIDTH_FROM_LABEL, B_WARNING_ALERT);
        alert->SetDefaultButton(alert->ButtonAt(0));
        if(alert->Go() == 0)
            _ClipboardJanitor();
    }
    return BApplication::QuitRequested();
}

void KeysApplication::ReadyToRun()
{
    BApplication::ReadyToRun();

    if(!BScreen().Frame().Contains(frame)) {
        // If the window goes offscreen, it moves it until it is fully visible
        window->MoveOnScreen(B_MOVE_IF_PARTIALLY_OFFSCREEN);
        frame = window->Frame();
    }
    if(inFocus) // Focus on the desired keyring (if it exists)
        window->SetUIStatus(S_UI_SET_KEYRING_FOCUS, inFocus);
    window->Show();
}

void KeysApplication::MessageReceived(BMessage* msg)
{
    switch(msg->what)
    {
        case B_COUNT_PROPERTIES:
        case B_CREATE_PROPERTY:
        case B_DELETE_PROPERTY:
        case B_EXECUTE_PROPERTY:
        case B_GET_PROPERTY:
        case B_SET_PROPERTY:
        {
            if(!msg->HasSpecifiers())
                break;

            HandleScripting(msg);
            break;
        }
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
        case M_ASK_FOR_CLIPBOARD_CLEANUP:
        {
            _ClipboardJanitor();
            break;
        }
        case I_SERVER_RESTART:
            StartServer(false, false);
            break;
        case I_SERVER_STOP:
            StopServer(true);
            break;

        case M_KEYSTORE_BACKUP:
            if(msg->IsSourceRemote() || msg->WasDropped())
                break; // Dismiss foreign messages for keystore operations

            KeystoreBackup(msg);
            break;
        case M_KEYSTORE_RESTORE:
            if(msg->IsSourceRemote())
                break; // Dismiss foreign messages for dangerous keystore operations
                         // but possibly allow drag and drop backups
            KeystoreRestore(msg);
            break;
        case M_KEYSTORE_WIPE_CONTENTS:
            if(msg->IsSourceRemote() || msg->WasDropped())
                break;

            WipeKeystoreContents(msg);
            break;

        case M_KEYRING_CREATE:
            if(msg->IsSourceRemote() || msg->WasDropped())
                break;

            AddKeyring(msg);
            break;
        case M_KEYRING_DELETE:
            if(msg->IsSourceRemote() || msg->WasDropped())
                break;

            RemoveKeyring(msg);
            break;
        case M_KEYRING_WIPE_CONTENTS:
            if(msg->IsSourceRemote() || msg->WasDropped())
                break;

            WipeKeyringContents(msg);
            break;
        case M_KEYRING_LOCK:
            if(msg->IsSourceRemote() || msg->WasDropped())
                break;

            LockKeyring(msg);
            break;
        case M_KEYRING_SET_LOCKKEY:
            if(msg->IsSourceRemote())
                break;

            SetKeyringLockKey(msg);
            break;
        case M_KEYRING_UNSET_LOCKKEY:
            if(msg->IsSourceRemote() || msg->WasDropped())
                break;

            RemoveKeyringLockKey(msg);
            break;

        case M_KEY_CREATE:
            if(msg->IsSourceRemote())
                break;

            AddKey(msg);
            break;
        case M_KEY_GENERATE_PASSWORD:
            if(msg->IsSourceRemote() || msg->WasDropped())
                break;

            GeneratePwdKey(msg);
            break;
        case M_KEY_IMPORT:
        {
            if(msg->IsSourceRemote())
                break;

            int32 i = 0;
            entry_ref ref;
            while(msg->FindRef("refs", i, &ref) == B_OK) {
                BMessage request(msg->what);
                request.AddString(kConfigKeyring, msg->GetString(kConfigKeyring));
                request.AddRef("refs", &ref);
                ImportKey(&request);
                i++;
            }
            break;
        }
        case M_KEY_EXPORT:
            if(msg->IsSourceRemote() || msg->WasDropped())
                break;

            ExportKey(msg);
            break;
        case M_KEY_DELETE:
            if(msg->IsSourceRemote() || msg->WasDropped())
                break;

            RemoveKey(msg);
            break;
        case M_KEY_COPY_SECRET:
            if(msg->IsSourceRemote() || msg->WasDropped())
                break;

            CopyKeyData(msg);
            break;

        case M_APP_DELETE:
            if(msg->IsSourceRemote() || msg->WasDropped())
                break;

            RemoveApp(msg);
            break;
        default:
            return BApplication::MessageReceived(msg);
    }
}

void KeysApplication::ArgvReceived(int32 argc, char** argv)
{
    /*
        Values for params:

        --keyring           <string>    Opens with <keyring> in focus.
        --reset-settings    <NULL>      Reset settings before init.

        Params not parsed here (only in main()):

        --help
        --version
    */
    if(argc == 1)
        return BApplication::ArgvReceived(argc, argv);

    std::unordered_map<const char*, const char*> paramMap;
    const char* paramName = NULL;
    const char* paramValue = NULL;
    for(int32 i = 1; i < argc; i++) { // Ignore application executable name
        if(strncmp(argv[i], "--", strlen("--")) == 0) { // Found a param
            if(paramName == NULL) {
                paramName = argv[i];
                continue;
            }
            else { // There is already a param waiting, let's emplace it to deal with the new one
                paramMap.emplace(std::make_pair(paramName, paramValue));
                paramName = argv[i];
            }
        }
        else { // Found value, push back if...
            if(paramName) { // there is a param name
                paramValue = argv[i];
                paramMap.emplace(std::make_pair(paramName, paramValue));
            }
            else { // otherwise ignore a paramValue without a paramName
                __trace("Param value \'%s\': not processed (cannot have a key value without a key name).\n", argv[i]);
                __trace("To pass more than one value, please use \'<paramName> \"<paramValue1>,<paramValue2>,...\"\'.\n");
                __trace("Please take into account that currently there are no parameters supporting multiple values.\n");
            }
            paramName = NULL;
            paramValue = NULL;
        }
    }
    if(paramName != NULL) // There may be waiting a last one, so do not miss it
        paramMap.emplace(std::make_pair(paramName, paramValue));

    for(const auto& it : paramMap) {
        if(strcmp(it.first, "--keyring") == 0) {// Parse it to be dealt by ReadyToRun()
            if(it.second && ks->KeyringByName(it.second)) {
                // Do not do anything if it is NULL or there is not a keyring named <it.second>
                __trace("Info: in focus: \'%s\'.\n", it.second);
                inFocus = it.second;
            }
        }
        else if(strcmp(it.first, "--reset-settings") == 0) { // No need of value
            CreateSettings(&currentSettings);
        }
        else
            __trace("Error: unrecognized parameter: \'%s\'.\n", it.first);
    }

    return BApplication::ArgvReceived(argc, argv);
}

void KeysApplication::RefsReceived(BMessage* msg)
{
    // There is not currently any usage for this

    BApplication::RefsReceived(msg);
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

// #pragma mark - Scripting support

status_t KeysApplication::GetSupportedSuites(BMessage* msg)
{
    msg->AddString("suites", kAppSuitesSgn);

    BPropertyInfo propertyInfo(kKeysProperties);
    msg->AddFlat("messages", &propertyInfo);

    return BApplication::GetSupportedSuites(msg);
}

BHandler* KeysApplication::ResolveSpecifier(BMessage* msg, int32 index,
    BMessage* specifier, int32 what, const char* property)
{
    BPropertyInfo propertyInfo(kKeysProperties);
    if(propertyInfo.FindMatch(msg, index, specifier, what, property) >= 0)
        return this;

    return BApplication::ResolveSpecifier(msg, index, specifier, what, property);
}

void KeysApplication::HandleScripting(BMessage* msg)
{
    BMessage reply(B_REPLY);
    int32 index = 0;
    BMessage specifier;
    int32 what = 0;
    const char* property = NULL;
    if(msg->GetCurrentSpecifier(&index, &specifier, &what, &property) == B_OK) {
        status_t status = B_ERROR;

        switch(BPropertyInfo(kKeysProperties).FindMatch(msg, index, &specifier, what, property))
        {
            case PROPERTY_SERVER:
            {
                if(msg->what == B_EXECUTE_PROPERTY) {
                    status = StartServer(true, true);
                }
                break;
            }
            case PROPERTY_KEYRINGS:
            {
                if(msg->what == B_GET_PROPERTY) {
                    for(int i = 0; i < ks->KeyringCount(); i++)
                        reply.AddString("result", ks->KeyringAt(i)->Identifier());
                    status = B_OK;
                }
                else if(msg->what == B_COUNT_PROPERTIES) {
                    reply.AddInt32("result", ks->KeyringCount());
                    status = B_OK;
                }
                break;
            }
            case PROPERTY_KEYRING_READ:
            {
                if(msg->what == B_GET_PROPERTY) {
                    KeyringImp* keyring = nullptr;
                    if(what == B_NAME_SPECIFIER)
                        keyring = ks->KeyringByName(specifier.GetString("name"));
                    else if(what == B_INDEX_SPECIFIER)
                        keyring = ks->KeyringAt(specifier.GetInt32("index", -1));

                    if(!keyring) {
                        status = B_ENTRY_NOT_FOUND;
                        break;
                    }

                    BMessage replyData(B_ARCHIVED_OBJECT);
                    replyData.AddString("name", keyring->Identifier());
                    replyData.AddBool("unlocked", keyring->IsUnlocked());
                    replyData.AddInt32("keys", keyring->KeyCount());
                    replyData.AddInt32("applications", keyring->ApplicationCount());
                    reply.AddMessage("result", &replyData);
                    status = B_OK;
                }
                break;
            }
            case PROPERTY_KEYRING_CREATE:
            {
                if(msg->what == B_CREATE_PROPERTY) {
                    const char* name = msg->GetString("name");
                    if(!name || strcasecmp(name, "Master") == 0) {
                        status = B_BAD_VALUE;
                        break;
                    }

                    if(ks->KeyringByName(name)) {
                        status = EEXIST;
                        break;
                    }

                    status = ks->AddKeyring(name, true);
                    if(status == B_OK && ks->KeyringByName(name))
                        _RebuildModel();
                    else
                        status = B_ERROR;
                }
                break;
            }
            case PROPERTY_KEYRING_DELETE:
            {
                if(msg->what == B_DELETE_PROPERTY) {
                    const char* name = specifier.GetString("name");
                    if(!name || strcasecmp(name, "Master") == 0) {
                        status = B_BAD_VALUE;
                        break;
                    }

                    if(!ks->KeyringByName(name)) {
                        status = B_ENTRY_NOT_FOUND;
                        break;
                    }

                    status = ks->RemoveKeyring(name, true);
                    if(status == B_OK && !ks->KeyringByName(name))
                        _RebuildModel();
                    else
                        status = B_ERROR;
                }
                break;
            }
            default:
                return BApplication::MessageReceived(msg);
        }

        if(status < B_OK) {
            reply.what = B_MESSAGE_NOT_UNDERSTOOD;
            reply.AddInt32("error", status);
            reply.AddString("message", strerror(status));
        }
        msg->SendReply(&reply);
    }
}

// #pragma mark - Settings

void KeysApplication::CreateSettings(BMessage* archive)
{
    archive->AddRect("frame", BRect(50, 50, 720, 480));
}

status_t KeysApplication::LoadSettings()
{
    status_t status = B_OK;
    BPath usrSettingsPath;
    if((status = find_directory(B_USER_SETTINGS_DIRECTORY, &usrSettingsPath)) != B_OK)
        __trace("Error: user's settings directory could not be found.\n");
    else {
        usrSettingsPath.Append(kAppSettings, true);
        BFile file(usrSettingsPath.Path(), B_READ_ONLY);
        if((status = file.InitCheck()) != B_OK)
            __trace("Error: settings file could not be located.\n");
        else {
            if((status = currentSettings.Unflatten(&file)) != B_OK)
                __trace("Error: the file \'%s\' could not be unflattened.\n",
                    usrSettingsPath.Path());
        }
    }

    if(status != B_OK) {
        // Somewhere in the steps before failed, let's use a temporary value set
        CreateSettings(&currentSettings);
    }

    return status;
}

status_t KeysApplication::SaveSettings()
{
    status_t status = B_OK;

    if(currentSettings.ReplaceRect("frame", window->Frame()) != B_OK)
        currentSettings.AddRect("frame", window->Frame());

    BPath usrSettingsPath;
    if((status = find_directory(B_USER_SETTINGS_DIRECTORY, &usrSettingsPath)) != B_OK) {
        __trace("Error: user's settings directory could not be found.\n");
        return status;
    }

    usrSettingsPath.Append(kAppSettings, true);
    BFile file(usrSettingsPath.Path(), B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
    if((status = file.InitCheck()) != B_OK) {
        __trace("Error: settings file \'%s\' could not be initialized.", usrSettingsPath.Path());
        return status;
    }
    file.SetPermissions(DEFFILEMODE);

    if((status = currentSettings.Flatten(&file)) != B_OK) {
        __trace("Error: settings data could not be written.\n");
        return status;
    }

    return status;
}

// #pragma mark - Server operations

status_t KeysApplication::StartServer(bool rebuildModel, bool forceRestart)
{
    status_t status = B_OK;

    if(be_roster->IsRunning(kKeyStoreServerSignature) && forceRestart)
        StopServer();

    BMessenger msgr(kKeyStoreServerSignature);
    if(!msgr.IsValid()) {
        __trace("Info: Keystore server is not currently running. Starting it...\n");
        status = be_roster->Launch(kKeyStoreServerSignature);
        if(status != B_OK && status != B_ALREADY_RUNNING) {
            __trace("Error: The server could not be launched successfully.\n");
            return B_ERROR;
        }
        // Here we should rebuild the data model
        if(rebuildModel) {
            __trace("Info: Rebuilding model...\n");
            // _RebuildModel();
        }
    }

    if(window != nullptr) { // Only notify to the window if there is a window
        BMessage reply(B_REPLY);
        reply.AddInt32(kConfigWhat, I_SERVER_RESTART);
        window->PostMessage(&reply);
    }

    return status;
}

status_t KeysApplication::StopServer(bool rebuildModel)
{
    if(be_roster->IsRunning(kKeyStoreServerSignature)) {
        team_id team = be_roster->TeamFor(kKeyStoreServerSignature);
        if(team == B_ERROR)
            return B_BAD_TEAM_ID;

        fprintf(stderr, "Info: trying to stop keystore server...\n");
        BMessenger msgr(kKeyStoreServerSignature, team);
        msgr.SendMessage(B_QUIT_REQUESTED);

        BMessage reply(B_REPLY);
        reply.AddInt32(kConfigWhat, I_SERVER_STOP);
        window->PostMessage(&reply);

        thServerMonitor = spawn_thread(_CallServerMonitor, "Keystore server monitor",
            B_NORMAL_PRIORITY, this);
        resume_thread(thServerMonitor);

        // here we should rebuild the data model
        if(rebuildModel) {
            fprintf(stderr, "Info: Rebuilding model...\n");
            ks->Reset();

            if(window != nullptr) { // Only notify to the window if there is a window
                BMessage reply(B_REPLY);
                reply.AddInt32(kConfigWhat, I_SERVER_STOP);
                window->PostMessage(&reply);
            }
        }
    }
    return B_OK;
}

// #pragma mark - Keystore operations

status_t KeysApplication::KeystoreBackup(BMessage* msg)
{
    if(!msg) {
        __trace("Error: %s.", strerror(B_BAD_VALUE));
        return B_BAD_VALUE;
    }

    uint32 method;
    BString* password = new BString;
    if(msg->FindUInt32("method", &method) != B_OK ||
    msg->FindString("password", password) != B_OK) {
        __trace("Error: %s. There are missing fields.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    status_t status = B_ERROR;
    switch(method) {
        case 'copy': {
            status = DoPlainKeystoreBackup();
            break;
        }
// #if defined(USE_OPENSSL)
        case 'ssl ':
            status = DoEncryptedKeystoreBackup(password->String());
            break;
// #endif
        default:
            break;
    }

    if(status != B_OK) { // Notify any errors
        BMessage reply(B_REPLY);
        reply.AddInt32(kConfigWhat, msg->what);
        reply.AddInt32(kConfigResult, status);
        window->PostMessage(&reply);
    }

    /* Secure erase memory containing the password */
    size_t passwordLength = password->Length();
    char* passwordData = password->LockBuffer(0);
    memzero(passwordData, passwordLength);
    std::atomic_thread_fence(std::memory_order_seq_cst); // Force zero-ing after usage
    password->UnlockBuffer(-1);
    delete password;

    return status;
}

status_t KeysApplication::KeystoreRestore(BMessage* msg)
{
    if(!msg) {
        __trace("Error: %s.", strerror(B_BAD_VALUE));
        return B_BAD_VALUE;
    }

    entry_ref ref;
    BString pass;
    msg->FindRef("refs", &ref);

    BFile datafile(&ref, B_READ_ONLY);
    BPath path(&ref);
    BMessage data;
    data.Unflatten(&datafile);
    pass = data.GetString("pass");

    status_t status = RestoreEncryptedKeystoreBackup(path.Path(), pass.String());
    if(status == B_OK) {
        StartServer(true);
        window->Update();
    }
    else { // Notify any errors
        BMessage reply(B_REPLY);
        reply.AddInt32(kConfigWhat, msg->what);
        reply.AddInt32(kConfigResult, status);
        window->PostMessage(&reply);
    }

    return status;
}

void KeysApplication::WipeKeystoreContents(BMessage* msg)
{
    // Not in the API, it's just a convenience method to quickly clean the database,
    //  mostly for dev purposes
    int32 count = ks->KeyringCount();
    for(int i = count - 1; i >= 0; i--) {
        // Master is protected, we can only clean it up
        if(strcasecmp(ks->KeyringAt(i)->Identifier(), "Master") == 0) {
            BMessage data;
            data.AddString(kConfigKeyring, "Master");
            WipeKeyringContents(&data);
        }
        // For the others, we can straight up delete them
        else ks->RemoveKeyring(ks->KeyringAt(i)->Identifier(), true);
    }
    window->Update();
}

// #pragma mark - Keyring operations

status_t KeysApplication::AddKeyring(BMessage* msg)
{
    if(!msg) {
        __trace("Error: %s.", strerror(B_BAD_VALUE));
        return B_BAD_VALUE;
    }

    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK) {
        __trace("Error: %s. No keyring name received.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

	if(strcasecmp(keyring.String(), "Master") == 0) {
		__trace("Error: a keyring cannot be created with the name \"Master\".\n");
		return B_NOT_ALLOWED;
	}

    if(ks->KeyringByName(keyring.String())) {
        __trace("Error: %s.\n", strerror(B_NAME_IN_USE));
        return B_NAME_IN_USE;
    }

    status_t status = ks->AddKeyring(keyring.String(), true);
    if(status == B_OK) {
        // we could here refill the database to retrieve entries under the keyring,
        //  but at this point, the keyring is empty anyways, so let's save cycles
        __trace("Info: keyring \"%s\" was successfully created.\n", keyring.String());
        window->Update((const void*)keyring.String()); // Update in focus
    }
    else {
        __trace("Error: the keyring could not be created in the store.\n");
        BMessage reply(B_REPLY);
        reply.AddInt32(kConfigWhat, msg->what);
        reply.AddInt32(kConfigResult, status);
        window->PostMessage(&reply);
    }

    return status;
}

status_t KeysApplication::LockKeyring(BMessage* msg)
{
	if(!msg) {
        __trace("Error: %s.", strerror(B_BAD_VALUE));
        return B_BAD_VALUE;
    }

    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        __trace("Error: %s. No keyring name received or bad keyring name.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    status_t status = ks->KeyringByName(keyring.String())->Lock();
    if(status == B_OK) {
        __trace("Info: keyring \"%s\" was successfully locked.\n", keyring.String());
        window->Update((const void*)keyring.String()); // Update in focus
    }
    else {
        __trace("Error: keyring \"%s\" could not be locked.\n", keyring.String());
        BMessage reply(B_REPLY);
        reply.AddInt32(kConfigWhat, msg->what);
        reply.AddInt32(kConfigResult, status);
        window->PostMessage(&reply);
    }

    return status;
}

status_t KeysApplication::SetKeyringLockKey(BMessage* msg)
{
	if(!msg) {
        __trace("Error: %s.", strerror(B_BAD_VALUE));
        return B_BAD_VALUE;
    }

    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        __trace("Error: %s. No keyring name received or bad keyring name.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    const void* data = nullptr;
    ssize_t length = 0;
    if(msg->FindData(kConfigKeyData, B_RAW_TYPE, &data, &length) != B_OK) {
        __trace("Error: %s. There are missing fields.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    BMessage* key = new BMessage;
    BPasswordKey* dummy = new BPasswordKey(reinterpret_cast<const char*>(data),
        B_KEY_PURPOSE_KEYRING, "", NULL);
    dummy->Flatten(*key);
    delete dummy;

    status_t status = ks->KeyringByName(keyring.String())->SetUnlockKey(key);
    if(status == B_OK)
        window->Update(keyring.String()); // Update in focus
    else {
        __trace("Error: The unlock key could not be applied to the keyring in the store.\n");
        BMessage reply(B_REPLY);
        reply.AddInt32(kConfigWhat, msg->what);
        reply.AddInt32(kConfigResult, status);
        window->PostMessage(&reply);
    }

    delete key;
    return B_OK;
}

status_t KeysApplication::RemoveKeyringLockKey(BMessage* msg)
{
	if(!msg) {
        __trace("Error: %s.", strerror(B_BAD_VALUE));
        return B_BAD_VALUE;
    }

    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        __trace("Error: %s. No keyring name received or bad keyring name.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    status_t status = ks->KeyringByName(keyring.String())->RemoveUnlockKey();
    if(status == B_OK)
        window->Update(keyring.String()); // Update in focus
    else {
        __trace("Error: The unlock key of this keyring could not be removed in the store.\n");
        BMessage reply(B_REPLY);
        reply.AddInt32(kConfigWhat, msg->what);
        reply.AddInt32(kConfigResult, status);
        window->PostMessage(&reply);
    }

    return status;
}

void KeysApplication::WipeKeyringContents(BMessage* msg)
{
	if(!msg) {
        __trace("Error: %s.", strerror(B_BAD_VALUE));
        return;
    }

    // Not in the API, it's just a convenience method to quickly clean the keyring,
    //  mostly for dev purposes
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        __trace("Error: %s. No keyring name received or bad keyring name.\n", strerror(B_BAD_DATA));
        return;
    }

    KeyringImp* target = ks->KeyringByName(keyring.String());
    int count = target->KeyCount();
    for(int i = count - 1 ; i >= 0; i--)
        target->RemoveKey(target->KeyAt(i)->Identifier(), true);

    window->Update(keyring.String()); // Update in focus
}

status_t KeysApplication::RemoveKeyring(BMessage* msg)
{
    if(!msg) {
        __trace("Error: %s.", strerror(B_BAD_VALUE));
        return B_BAD_VALUE;
    }

    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        __trace("Error: bad data. No keyring name received or bad keyring name.\n");
        return B_BAD_DATA;
    }

	if(strcasecmp(keyring.String(), "Master") == 0) {
		__trace("Error: the \"Master\" keyring cannot be removed.\n");
		return B_NOT_ALLOWED;
	}

    status_t status = ks->RemoveKeyring(keyring.String(), true);
    if(status == B_OK)
        window->Update(keyring.String()); // Update in focus
    else {
        __trace("Error: the keyring %s could not be removed from the store.\n", keyring.String());
        BMessage reply(B_REPLY);
        reply.AddInt32(kConfigWhat, msg->what);
        reply.AddInt32(kConfigResult, status);
        window->PostMessage(&reply);
    }

    return status;
}

// #pragma mark - Keys operations

status_t KeysApplication::AddKey(BMessage* msg)
{
	if(!msg) {
        __trace("Error: %s.", strerror(B_BAD_VALUE));
        return B_BAD_VALUE;
    }

    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        __trace("Error: %s. No keyring name received or bad keyring name.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
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
        __trace("Error: %s. There are missing fields.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    /* Check for duplicates. The API seems to allow partial duplicates
        as long as they are either of different type or have different
        secondary identifiers.
    */
    KeyImp* key = ks->KeyringByName(keyring.String())->KeyByIdentifier(id.String(), sec.String());
    if(key) {
        fprintf(stderr, "Error: key (%s, %s) in %s already exists.\n", id.String(), sec.String(), keyring.String());
        BMessage answer(B_REPLY);
        answer.AddInt32(kConfigWhat, msg->what);
        answer.AddInt32(kConfigResult, B_NAME_IN_USE);
        window->PostMessage(&answer);
        return B_BAD_DATA;
    }

    status_t status = ks->KeyringByName(keyring.String())->AddKey(p, t,
        id.String(), sec.String(), (const uint8*)data, length, true);
    if(status == B_OK) {
        __trace("Info: the key (%s, %s) was created in %s keyring successfully.\n",
            id.String(), sec.String(), keyring.String());
        window->Update(keyring.String()); // Update in focus
    }
    else {
        __trace("Error: %s. The key (%s, %s) could not be added to %s.\n",
            strerror(status), id.String(), sec.String(), keyring.String());
        BMessage reply(B_REPLY);
        reply.AddInt32(kConfigWhat, msg->what);
        reply.AddInt32(kConfigResult, status);
        window->PostMessage(&reply);
    }

    return status;
}

status_t KeysApplication::GeneratePwdKey(BMessage* msg)
{
    if(!msg) {
        __trace("Error: %s.", strerror(B_BAD_VALUE));
        return B_BAD_VALUE;
    }

    status_t status = B_OK;
    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        __trace("Error: %s. No keyring name received or bad keyring name.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    BString id, sec;
    BKeyPurpose p;
    uint32 length;
    if(msg->FindString(kConfigKeyName, &id) != B_OK ||
    msg->FindUInt32(kConfigKeyPurpose, (uint32*)&p) != B_OK ||
    msg->FindUInt32(kConfigKeyGenLength, &length) != B_OK) {
        __trace("Error: %s. There are missing fields.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    if(msg->FindString(kConfigKeyAltName, &sec) != B_OK)
        sec.SetTo(""); // default if not set or missing

    BPasswordKey pwdkey("", p, id.String(), sec.String());
    status = GeneratePassword(pwdkey, length, 0);
    BMessage data;
    pwdkey.Flatten(data);
    status = ks->KeyringByName(keyring.String())->ImportKey(&data);
    if(status == B_OK) {
        __trace("Info: the key (%s, %s) was created in %s keyring successfully.\n",
            id.String(), sec.String(), keyring.String());
        window->Update(keyring.String()); // Update in focus
    }
    else {
        BMessage reply(B_REPLY);
        reply.AddInt32(kConfigWhat, msg->what);
        reply.AddInt32(kConfigResult, status);
        window->PostMessage(&reply);
    }

    return status;
}

status_t KeysApplication::ImportKey(BMessage* msg)
{
    if(!msg) {
        __trace("Error: %s.", strerror(B_BAD_VALUE));
        return B_BAD_VALUE;
    }

    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        __trace("Error: %s. No keyring name received or bad keyring name.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    entry_ref ref;
    if(msg->FindRef("refs", &ref) != B_OK) {
        __trace("Error: %s. No entry reference received.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    BFile file(&ref, B_READ_ONLY);
    status_t status = file.InitCheck();
    if(status != B_OK) {
        __trace("Error: %s. No file from where import.\n", strerror(status));
        return status;
    }

    BMessage* archive = new BMessage;
    status = archive->Unflatten(&file);
    if(status != B_OK) {
        __trace("Error: %s. The message could not be unflattened from file.\n", strerror(status));
        delete archive;
        return status;
    }

    BString id, sec;
    if(archive->FindString("identifier", &id) != B_OK ||
    archive->FindString("secondaryIdentifier", &sec) != B_OK) {
        __trace("Error: %s. Key message with missing fields.\n", strerror(B_BAD_DATA));
        delete archive;
        return B_BAD_DATA;
    }

    status = B_NAME_IN_USE;
    KeyringImp* kr = ks->KeyringByName(keyring.String());
    if(kr->KeyByIdentifier(id.String(), sec.String()) == nullptr) {
        status = kr->ImportKey(archive);
    }

    if(status == B_OK) {
        __trace("Info: key successfully imported.\n");
        window->Update(kr->Identifier());
    }
    else {
        __trace("Error: key already exists in keyring or there was an error during the import.\n");
        BMessage reply(B_REPLY);
        reply.AddInt32(kConfigWhat, msg->what);
        reply.AddInt32(kConfigResult, status);
        window->PostMessage(&reply);
    }

    delete archive;
    return status;
}

status_t KeysApplication::ExportKey(BMessage* msg)
{
    if(!msg) {
        __trace("Error: %s.", strerror(B_BAD_VALUE));
        return B_BAD_VALUE;
    }

    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        __trace("Error: %s. No keyring name received or bad keyring name.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    status_t status = B_OK;
    entry_ref dirref;
    BString name, id, alt;
    if(msg->FindRef("directory", &dirref) != B_OK ||
    msg->FindString("name", &name) != B_OK ||
    msg->FindString(kConfigKeyName, &id) != B_OK ||
    msg->FindString(kConfigKeyAltName, &alt) != B_OK) {
        __trace("Error: %s. There are missing fields.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    BMessage* archive = new BMessage;
    if((status = ks->KeyringByName(keyring.String())->KeyByIdentifier(id.String(), alt.String())->Export(archive)) != B_OK) {
        __trace("Error: the key (%s, %s) in %s could not be exported to a flattened BMessage.\n",
            id.String(), alt.String(), keyring.String());
        delete archive;
        return status;
    }

    BDirectory directory(&dirref);
    BFile file(&directory, name.String(), B_READ_WRITE | B_CREATE_FILE | B_FAIL_IF_EXISTS);
    if((status = file.InitCheck()) != B_OK) {
        __trace("The file %s to where export the flattened BMessage could not be initialized.\n",
            BPath(&directory, name.String(), true).Path());
        delete archive;
        return status;
    }

    status = archive->Flatten(&file);
    if(status != B_OK) {
        BMessage reply(B_REPLY);
        reply.AddInt32(kConfigWhat, msg->what);
        reply.AddInt32(kConfigResult, status);
        window->PostMessage(&reply);
    }
    else {
        // Only the creator can handle the exported key file
        file.SetPermissions(S_IRUSR | S_IWUSR);
    }

    delete archive;
    return status;
}

status_t KeysApplication::RemoveKey(BMessage* msg)
{
    if(!msg) {
        __trace("Error: %s.", strerror(B_BAD_VALUE));
        return B_BAD_VALUE;
    }

    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        __trace("Error: %s. No keyring name received or bad keyring name.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    BString id, alt;
    if(msg->FindString(kConfigKeyName, &id) != B_OK ||
    msg->FindString(kConfigKeyAltName, &alt) != B_OK) {
        __trace("Error: %s. There are missing fields.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    status_t status = ks->KeyringByName(keyring.String())->RemoveKey(id.String(), alt.String(), true);
    if(status == B_OK) {
        window->Update(keyring.String());
    }
    else {
        BMessage reply(B_REPLY);
        reply.AddInt32(kConfigWhat, msg->what);
        reply.AddInt32(kConfigResult, status);
        window->PostMessage(&reply);
    }

    return status;
}

status_t KeysApplication::CopyKeyData(BMessage* msg)
{
    if(!msg) {
        __trace("Error: %s.", strerror(B_BAD_VALUE));
        return B_BAD_VALUE;
    }

    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        __trace("Error: %s. No keyring name received or bad keyring name.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    BString id, alt;
    BKeyType type = B_KEY_TYPE_ANY;
    if(msg->FindString(kConfigKeyName, &id) != B_OK ||
    msg->FindString(kConfigKeyAltName, &alt) != B_OK ||
    msg->FindUInt32(kConfigKeyType, (uint32*)&type) != B_OK) {
        __trace("Error: %s. There are missing fields.\n", strerror(B_BAD_DATA));
        return B_BAD_DATA;
    }

    if(!(type > B_KEY_TYPE_ANY && type < B_KEY_TYPE_CERTIFICATE)) {
        __trace("Error: %s.\n", strerror(B_BAD_TYPE));
        return B_BAD_TYPE;
    }

    // Obtain key data pointer
    const uint8* data = NULL;
    size_t dataLength = 0;
    if(type == B_KEY_TYPE_PASSWORD) {
        BPasswordKey pwdkey;
        BKeyStore().GetKey(keyring.String(), type, id.String(), alt.String(), false, pwdkey);
        data = reinterpret_cast<const uint8*>(pwdkey.Password());
        dataLength = strlen(pwdkey.Password());
    }
    else {
        BKey key;
        BKeyStore().GetKey(keyring.String(), type, id.String(), alt.String(), false, key);
        data = reinterpret_cast<const uint8*>(key.Data());
        dataLength = key.DataLength();
    }
    if(data == NULL)
        return ENODATA;

    // Copy that data into the clipboard
    if(be_clipboard->Lock() && data != NULL) {
        be_clipboard->Clear();

        BMessage *clipmsg = be_clipboard->Data();
        clipmsg->AddData("text/plain", B_MIME_TYPE,
            reinterpret_cast<const char*>(data), dataLength);
        status_t status = be_clipboard->Commit();
        if(status != B_OK) {
            fprintf(stderr, "Error saving data to clipboard.\n");
        } else {
            hasDataCopied = true;
        }

        be_clipboard->Unlock();
    }

    return B_OK;
}

// #pragma mark - Applications operations

status_t KeysApplication::RemoveApp(BMessage* msg)
{
	if(!msg) {
        __trace("Error: %s.", strerror(B_BAD_VALUE));
        return B_BAD_VALUE;
    }

    BString keyring;
    if(msg->FindString(kConfigKeyring, &keyring) != B_OK ||
    !ks->KeyringByName(keyring.String())) {
        __trace("Error: bad data. No keyring name received or bad keyring name.\n");
        return B_BAD_DATA;
    }

    BString signature;
    if(msg->FindString(kConfigSignature, &signature) != B_OK) {
        __trace("Error: bad data. There are missing fields.\n");
        return B_BAD_DATA;
    }

    status_t status = ks->KeyringByName(keyring.String())->RemoveApplication(signature.String(), true);

    if(status == B_OK)
		window->Update(keyring.String());
    else {
        BMessage reply(B_REPLY);
        reply.AddInt32(kConfigWhat, msg->what);
        reply.AddInt32(kConfigResult, status);
        window->PostMessage(&reply);
    }

    return status;
}

// #pragma mark - Private

void KeysApplication::_InitAppData(const BMessage* data)
{
    BMessage defaultSettings;
    CreateSettings(&defaultSettings);

    if(currentSettings.FindRect("frame", &frame) != B_OK)
        defaultSettings.FindRect("frame", &frame);
}

void KeysApplication::_InitKeystoreData(KeystoreImp*& ks, BKeyStore* keystore)
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
                __trace("Info: found keyring: %s\n", keyringName.String());
                ks->AddKeyring(keyringName.String());
                _InitKeyring(ks, keystore, keyringName.String());
                break;
            }
            case B_ENTRY_NOT_FOUND:
                __trace("Info: no keyrings left\n");
                next = false;
                break;
            default:
                __trace("Error: %s comms error\n", keyringName.String());
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
    _InitKeystoreData(ks, &keystore);
    window->Update();
}

void KeysApplication::_ClipboardJanitor()
{
    if(hasDataCopied) {
        if(be_clipboard->Lock()) {
            be_clipboard->Clear();
            status_t cleared = be_clipboard->Commit();
            if(cleared == B_OK)
                hasDataCopied = false;
            be_clipboard->Unlock();
        }
    }
}
