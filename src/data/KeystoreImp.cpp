/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <Catalog.h>
#include <KeyStore.h>
#include <Roster.h>
#include <cstdio>
#include "KeystoreImp.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "String 4 life"

const char* StringForPurpose(BKeyPurpose purpose)
{
    switch(purpose) {
        case B_KEY_PURPOSE_ANY: return B_TRANSLATE("Any");
        case B_KEY_PURPOSE_GENERIC: return B_TRANSLATE("Generic");
        case B_KEY_PURPOSE_KEYRING: return B_TRANSLATE("Keyring");
        case B_KEY_PURPOSE_WEB: return B_TRANSLATE("Web");
        case B_KEY_PURPOSE_NETWORK: return B_TRANSLATE("Network");
        case B_KEY_PURPOSE_VOLUME: return B_TRANSLATE("Volume");
        default: return "";
    }
}

const char* StringForType(BKeyType type) {
    switch(type) {
        case B_KEY_TYPE_ANY: return B_TRANSLATE("Any");
        case B_KEY_TYPE_GENERIC: return B_TRANSLATE("Generic");
        case B_KEY_TYPE_PASSWORD: return B_TRANSLATE("Password");
        case B_KEY_TYPE_CERTIFICATE: return B_TRANSLATE("Certificate");
        default: return "";
    }
}

#undef B_TRANSLATION_CONTEXT

bool IsExportedKey(BMessage* keyFileData)
{
    status_t status = B_ERROR;
    type_code code = B_ANY_TYPE;
    int32 found = 0;

    status = keyFileData->GetInfo("type", &code, &found);
    if(status == B_OK && code == B_UINT32_TYPE && found == 1)
        status = keyFileData->GetInfo("purpose", &code, &found);
    if(status == B_OK && code == B_UINT32_TYPE && found == 1)
        status = keyFileData->GetInfo("identifier", &code, &found);
    if(status == B_OK && code == B_STRING_TYPE && found == 1)
        status = keyFileData->GetInfo("secondaryIdentifier", &code, &found);
    if(status == B_OK && code == B_STRING_TYPE && found == 1)
        status = keyFileData->GetInfo("data", &code, &found);
    if(status == B_OK && code == B_ANY_TYPE && found == 1)
        status = B_OK; // We currently will not require owner and
                       // creationTime as they are not currently used in the API

    return status == B_OK;
}

/* Operation modes for read-write methods:
 *  + model-only: only works on the model ADT
 *  + model-and-database: works both on model and database
 *  + model-opt-database: works on model, optionally on database
 *  + database-only: works on database only
 */

// #pragma mark - KeyImp

// ctor: model-only
KeyImp::KeyImp(KeyringImp* parent)
: fParent(parent)
{
    SetTo(B_KEY_PURPOSE_GENERIC, B_KEY_TYPE_GENERIC, "", nullptr, 0, "");
}

// ctor2: model-only
KeyImp::KeyImp(KeyringImp* parent, BKey key)
: fParent(parent)
{
    SetTo(key.Purpose(), key.Type(), key.Identifier(),
        key.SecondaryIdentifier(), key.CreationTime(), key.Owner());
}

// ctor3: model-only
KeyImp::KeyImp(KeyringImp* parent, BKeyPurpose p, BKeyType t,
    const char* id, const char* secid, bigtime_t dc, const char* owner)
: fParent(parent)
{
    SetTo(p, t, id, secid, dc, owner);
}

// SetTo: model-only
void KeyImp::SetTo(BKeyPurpose p, BKeyType t,
    const char* id, const char* secid, bigtime_t dc, const char* owner)
{
    fPurpose = p;
    fType = t;
    fIdentifier.SetTo(id);
    secid ? fSecIdentifier.SetTo(secid) : fSecIdentifier.SetTo("");
    fCreated = dc;
    fOwner.SetTo(owner);
}

KeyringImp* KeyImp::Parent()
{
    return fParent;
}

const char* KeyImp::Identifier()
{
    return fIdentifier.String();
}

const char* KeyImp::SecondaryIdentifier()
{
    return fSecIdentifier.String();
}

BKeyType KeyImp::Type()
{
    return fType;
}

BKeyPurpose KeyImp::Purpose()
{
    return fPurpose;
}

bigtime_t KeyImp::Created()
{
    return fCreated;
}

const char* KeyImp::Owner()
{
    return fOwner.String();
}

void KeyImp::Data(const void* ptr, size_t* len)
{
    BKey key;
    BKeyStore().GetKey(fParent->Identifier(), Type(), Identifier(),
        SecondaryIdentifier(), false, key);
    ptr = reinterpret_cast<const void*>(key.Data());
    *len = key.DataLength();
}

void KeyImp::PrintToStream()
{
    printf("\t\tKey: %s, %s. parent(%s), type(%d), purpose(%d)\n",
        Identifier(), strlen(SecondaryIdentifier()) > 0 ? SecondaryIdentifier() :
        "[no secondary id.]" , Parent()->Identifier(), Type(), Purpose());
}

// Export: database-only
status_t KeyImp::Export(BMessage* archive)
{
    switch(Type())
    {
        case B_KEY_TYPE_GENERIC:
        {
            BKey key;
            if(BKeyStore().GetKey(fParent->Identifier(), B_KEY_TYPE_GENERIC,
            Identifier(), SecondaryIdentifier(), false, key) != B_OK)
                return B_ERROR;
            return key.Flatten(*archive);
        }
        case B_KEY_TYPE_PASSWORD:
        {
            BPasswordKey pwdkey;
            if(BKeyStore().GetKey(fParent->Identifier(), B_KEY_TYPE_PASSWORD,
            Identifier(), SecondaryIdentifier(), false, pwdkey) != B_OK)
                return B_ERROR;
            return pwdkey.Flatten(*archive);
        }
        case B_KEY_TYPE_CERTIFICATE:
        case B_KEY_TYPE_ANY:
        default:
            return B_NOT_SUPPORTED;
    }
}

// #pragma mark - AppImp

ApplicationAccessImp::ApplicationAccessImp(KeyringImp* parent, const char* signature)
: fParent(parent)
{
    fSignature.SetTo(signature);
}

KeyringImp* ApplicationAccessImp::Parent()
{
    return fParent;
}

const char* ApplicationAccessImp::Identifier()
{
    return fSignature.String();
}

status_t ApplicationAccessImp::GetRef(entry_ref* ref)
{
    return be_roster->FindApp(Identifier(), ref);
}

void ApplicationAccessImp::PrintToStream()
{
    printf("\t\tApplication: %s, parent(%s)\n", Identifier(), Parent()->Identifier());
}

// #pragma mark - KeyringImp

KeyringImp::KeyringImp(KeystoreImp* parent, const char* name)
: fParent(parent),
  fHasUnlockKey(false),
  fIsUnlocked(BKeyStore().IsKeyringUnlocked(name))
{
    fName.SetTo(name);
}

KeyringImp::~KeyringImp()
{
    Reset();
}

KeystoreImp* KeyringImp::Parent()
{
    return fParent;
}

const char* KeyringImp::Identifier()
{
    return fName.String();
}

// IsUnlocked: database-only
bool KeyringImp::IsUnlocked()
{
    return BKeyStore().IsKeyringUnlocked(fName.String());
}

// Lock: database-only
status_t KeyringImp::Lock()
{
    return BKeyStore().LockKeyring(fName.String());
}

// Unlock: none
status_t KeyringImp::Unlock()
{
    return B_NOT_SUPPORTED;
}

status_t KeyringImp::SetUnlockKey(BMessage* data)
{
    status_t status = B_OK;
    BPasswordKey key;
    if((status = key.Unflatten(*data)) != B_OK)
        return status;

    if((status = BKeyStore().SetUnlockKey(fName.String(), key)) == B_OK)
        fHasUnlockKey = true;

    return status;
}

status_t KeyringImp::RemoveUnlockKey()
{
    status_t status = B_OK;

    if((status = BKeyStore().RemoveUnlockKey(fName.String())) == B_OK)
        fHasUnlockKey = false;

    return status;
}

// AddKey: model-opt-database
status_t KeyringImp::AddKey(BKeyPurpose p, BKeyType t, const char* id,
    const char* secid, const uint8* data, size_t length, bool createInDb)
{
    status_t status = B_OK;

    if(createInDb) {
        switch(t) {
            case B_KEY_TYPE_GENERIC:
                status = BKeyStore().AddKey(Identifier(),
                    BKey(p, id, secid, data, length));
                break;
            case B_KEY_TYPE_PASSWORD:
            {
                const char* password = reinterpret_cast<const char*>(data);
                status = BKeyStore().AddKey(Identifier(),
                    BPasswordKey(password, p, id, secid));
                break;
            }
            default:
                return B_NOT_SUPPORTED;
        }
    }

    bool added = false;
    if(status == B_OK)
        added = fKeyList.AddItem(new KeyImp(this, p, t, id, secid));

    return added ? B_OK : B_ERROR;
}

// ImportKey: model-and-database
status_t KeyringImp::ImportKey(BMessage* archive)
{
    BKeyType type;
    if(archive->FindUInt32("type", (uint32*)&type) != B_OK)
        return B_BAD_DATA; // not even a flattened key

    status_t status = B_OK;
    switch(type) {
        case B_KEY_TYPE_GENERIC:
        {
            BKey key;
            if((status = key.Unflatten(*archive)) != B_OK)
                return status;
            if((status = BKeyStore().AddKey(Identifier(), key)) != B_OK)
                return status;
            // The following addition is model-only because it has just been directly added to db
            AddKey(key.Purpose(), key.Type(), key.Identifier(), key.SecondaryIdentifier());
            return B_OK;
        }
        case B_KEY_TYPE_PASSWORD:
        {
            BPasswordKey key;
            if((status = key.Unflatten(*archive)) != B_OK)
                return status;
            if((status = BKeyStore().AddKey(Identifier(), key)) != B_OK)
                return status;
            // The following addition is model-only because it has just been directly added to db
            AddKey(key.Purpose(), key.Type(), key.Identifier(), key.SecondaryIdentifier());
            return B_OK;
        }
        case B_KEY_TYPE_CERTIFICATE:
        case B_KEY_TYPE_ANY:
        default:
            return B_NOT_SUPPORTED;
    }
}

// RemoveKey: model-opt-database
status_t KeyringImp::RemoveKey(const char* id, bool deleteInDb)
{
    status_t status = B_OK;

    KeyImp* keyentry = FindInList(fKeyList, id);
    if(!keyentry)
        return B_ENTRY_NOT_FOUND;

    if(deleteInDb) {
        switch(keyentry->Type()) {
            case B_KEY_TYPE_GENERIC: {
                BKey key;
                if((status = BKeyStore().GetKey(Identifier(), keyentry->Type(),
                keyentry->Identifier(), key)) != B_OK)
                    return status;
                status = BKeyStore().RemoveKey(Identifier(), key);
                break;
            }
            case B_KEY_TYPE_PASSWORD: {
                BPasswordKey key;
                if((status = BKeyStore().GetKey(Identifier(), keyentry->Type(),
                keyentry->Identifier(), key)) != B_OK)
                    return status;
                status = BKeyStore().RemoveKey(Identifier(), key);
                break;
            }
            default:
                return B_NOT_SUPPORTED;
        }
    }

    bool removed = false;
    if(status == B_OK)
        removed = fKeyList.RemoveItem(keyentry);

    return removed ? B_OK : B_ERROR;
}

// RemoveKey: model-opt-database
status_t KeyringImp::RemoveKey(const char* id, const char* secid, bool deleteInDb)
{
    status_t status = B_OK;
    KeyImp* keyentry = FindInList2(fKeyList, id, secid);
    if(!keyentry) // does not necessarily mean that the key does not exist
        return B_ENTRY_NOT_FOUND;

    if(deleteInDb) {
        switch(keyentry->Type()) {
            case B_KEY_TYPE_GENERIC: {
                BKey key;
                if((status = BKeyStore().GetKey(Identifier(), keyentry->Type(),
                keyentry->Identifier(), keyentry->SecondaryIdentifier(), false,
                key)) != B_OK)
                    return status;
                status = BKeyStore().RemoveKey(Identifier(), key);
                break;
            }
            case B_KEY_TYPE_PASSWORD: {
                BPasswordKey key;
                if((status = BKeyStore().GetKey(Identifier(), keyentry->Type(),
                keyentry->Identifier(), keyentry->SecondaryIdentifier(), false,
                key)) != B_OK)
                    return status;
                status = BKeyStore().RemoveKey(Identifier(), key);
                break;
            }
            default:
                return B_NOT_SUPPORTED;
        }
    }

    bool removed = false;
    if(status == B_OK)
        removed = fKeyList.RemoveItem(keyentry);

    return removed ? B_OK : B_ERROR;
}

KeyImp* KeyringImp::KeyAt(int32 index)
{
    return fKeyList.ItemAt(index);
}

KeyImp* KeyringImp::KeyByIdentifier(const char* id)
{
    // return FindInList(fKeyList, id);
    return FindInList2(fKeyList, id, "");
}

KeyImp* KeyringImp::KeyByIdentifier(const char* id, const char* secondary_id)
{
    if(!secondary_id)
        secondary_id = "";

    return FindInList2(fKeyList, id, secondary_id);
}

int32 KeyringImp::KeyCount(BKeyType type, BKeyPurpose purpose)
{
    int count = 0;
    for(int i = 0; i < fKeyList.CountItems(); i++) {
        if((type == B_KEY_TYPE_ANY || fKeyList.ItemAt(i)->Type() == type) &&
        (purpose == B_KEY_PURPOSE_ANY || fKeyList.ItemAt(i)->Purpose() == purpose))
            count++;
    }
    return count;
}

void KeyringImp::AddApplicationToList(const char* signature)
{
    fAppList.AddItem(new ApplicationAccessImp(this, signature));
}

status_t KeyringImp::RemoveApplication(const char* signature, bool deleteInDb)
{
    status_t status = B_OK;

    if(deleteInDb)
        status = BKeyStore().RemoveApplication(Identifier(), signature);

    ApplicationAccessImp* app = FindInList(fAppList, signature);
    if(status == B_OK && app) {
        bool result = fAppList.RemoveItem(app);
        if(result) status = B_OK;
        else status = B_ERROR;
    }

    return status;
}

ApplicationAccessImp* KeyringImp::ApplicationAt(int32 index)
{
    return fAppList.ItemAt(index);
}

ApplicationAccessImp* KeyringImp::ApplicationBySignature(const char* signature)
{
    return FindInList(fAppList, signature);
}

int32 KeyringImp::ApplicationCount()
{
    return fAppList.CountItems();
}

void KeyringImp::PrintToStream()
{
    printf("\tKeyring: %s. %d keys. %d applications.\n", Identifier(),
        fKeyList.CountItems(), fAppList.CountItems());
    for(int i = 0; i < fKeyList.CountItems(); i++)
        fKeyList.ItemAt(i)->PrintToStream();
    for(int i = 0; i < fAppList.CountItems(); i++)
        fAppList.ItemAt(i)->PrintToStream();
}

void KeyringImp::Reset()
{
    // Reset the data structure without touching the actual data on disk
    for(int i = 0; i < fKeyList.CountItems(); i++)
        fKeyList.RemoveItem(fKeyList.ItemAt(i));
    for(int i = 0; i < fAppList.CountItems(); i++)
        fAppList.RemoveItem(fAppList.ItemAt(i));
}

// #pragma mark - KeystoreImp

KeystoreImp::KeystoreImp()
{
}

KeystoreImp::~KeystoreImp()
{
    Reset();
}

status_t KeystoreImp::AddKeyring(const char* name, bool createInDb)
{
    status_t status = B_OK;

    if(createInDb) {
        status = BKeyStore().AddKeyring(name);
    }

    if(status == B_OK) {
        bool result = fKeyringList.AddItem(new KeyringImp(this, name));
        if(result) status = B_OK;
        else status = B_ERROR;
    }

    return status;
}

status_t KeystoreImp::RemoveKeyring(const char* name, bool deleteInDb)
{
    status_t status = B_OK;

    KeyringImp* keyring = FindInList(fKeyringList, name);
    if(keyring) {
        bool removed = fKeyringList.RemoveItem(keyring);
        if(removed && deleteInDb) {
            status = BKeyStore().RemoveKeyring(name);
        }
    }
    else
        return B_ENTRY_NOT_FOUND;

    return status;
}

KeyringImp* KeystoreImp::KeyringAt(int32 index)
{
    return fKeyringList.ItemAt(index);
}

KeyringImp* KeystoreImp::KeyringByName(const char* name)
{
    return FindInList(fKeyringList, name);
}

int32 KeystoreImp::KeyringCount()
{
    return fKeyringList.CountItems();
}

void KeystoreImp::PrintToStream()
{
    printf("Keystore. %d keyrings.\n", fKeyringList.CountItems());

    for(int i = 0; i < fKeyringList.CountItems(); i++)
        fKeyringList.ItemAt(i)->PrintToStream();
}

bool KeystoreImp::IsEmpty()
{
    return fKeyringList.IsEmpty();
}

void KeystoreImp::Reset()
{
    // Reset the data structure without touching the actual data on disk
    if(!IsEmpty()) {
        for(int i = 0; i < fKeyringList.CountItems(); i++)
            fKeyringList.RemoveItem(fKeyringList.ItemAt(i), true);
    }
}
