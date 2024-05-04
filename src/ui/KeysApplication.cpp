/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <StorageKit.h>
#include <Catalog.h>
#include <KeyStore.h>
#include <private/interface/AboutWindow.h>
#include <cstdio>
#include "KeysApplication.h"
#include "KeysDefs.h"
#include "KeysWindow.h"
#include "KeystoreImp.h"

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
        case M_USER_PROVIDES_KEY:
        case M_USER_REMOVES_KEY:
        case M_USER_REMOVES_APPAUTH:
        {
            BString targetkeyring = msg->GetString("owner");
            if(!targetkeyring.IsEmpty()) {
                _InitKeyring(&ks, &keystore, targetkeyring.String());
                BMessenger msgr(NULL, window, NULL);
                msgr.SendMessage(msg);
            }
            break;
        }
        case I_SERVER_RESTART:
            _RestartServer();
            break;
        case M_USER_ADDS_KEYRING:
        case M_USER_REMOVES_KEYRING:
        {
            BString owner = msg->GetString("owner");

            window->Update((const void*)owner.String());
            break;
        }
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
                fprintf(stderr, "%s\n", keyringName.String());
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
                // fprintf(stderr, "%s : name(%s), extraname(%s), purpose(%d), type(%d), data(%s)\n",
                    // kr, key.Identifier(),
                    // key.SecondaryIdentifier() == NULL ? "" : key.SecondaryIdentifier(),
                    // key.Purpose(), key.Type(), key.Data());
                ks->KeyringByName(kr)->AddKeyToList(key.Purpose(), key.Type(),
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
                // fprintf(stderr, "%s : name(%s), extraname(%s), purpose(%d), type(%d), data(%s)\n",
                    // kr, pwdkey.Identifier(),
                    // pwdkey.SecondaryIdentifier() == NULL ? "" : pwdkey.SecondaryIdentifier(),
                    // pwdkey.Purpose(), pwdkey.Type(), pwdkey.Data());
                ks->KeyringByName(kr)->AddKeyToList(pwdkey.Purpose(), pwdkey.Type(),
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
