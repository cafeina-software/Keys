/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <KeyStore.h>
#include <cstdio>
#include "KeystoreImp.h"

// #pragma mark -

KeyImp::KeyImp(KeyringImp* _owner, BKeyPurpose _purpose, BKeyType t,
    const char* _id, const char* _secid)
: owner(_owner),
  purpose(_purpose),
  type(t)
{
    identifier = _id;
    if(_secid != NULL)
        secidentifier = _secid;
}

const char* KeyImp::Identifier()
{
    return identifier.String();
}

const char* KeyImp::SecondaryIdentifier()
{
    return secidentifier.String();
}

BKeyType KeyImp::Type()
{
    return type;
}

BKeyPurpose KeyImp::Purpose()
{
    return purpose;
}

BKey KeyImp::GetBKey()
{
	BKeyStore keystore;
    BKey key;

	keystore.GetKey(owner->Identifier(), B_KEY_TYPE_ANY, Identifier(), key);

    return key;
}

void KeyImp::PrintToStream()
{
    printf("\t\tKey: %s. type(%d), purpose(%d), data(%s)\n",
        Identifier(), Type(), Purpose(), GetBKey().Data());
}

// #pragma mark -

ApplicationAccessImp::ApplicationAccessImp(const char* _signature)
{
    signature = _signature;
}

const char* ApplicationAccessImp::Identifier()
{
    return signature.String();
}

void ApplicationAccessImp::PrintToStream()
{
    printf("\t\tApplication: %s\n", Identifier());
}

// #pragma mark -

KeyringImp::KeyringImp(const char* _name)
{
    name = _name;
}

KeyringImp::~KeyringImp()
{
    Reset();
}

const char* KeyringImp::Identifier()
{
    return name.String();
}

void KeyringImp::CreateKey(BKeyPurpose _purpose, const char* _id, const char* _secid,
    const uint8* _data, size_t _length)
{
    BKeyStore keystore;
    status_t status = keystore.AddKey(Identifier(), BKey(_purpose, _id, _secid,
        _data, _length));

    BKey key;
    keystore.GetKey(Identifier(), B_KEY_TYPE_ANY, _id, key);

    if(status == B_OK)
        AddKeyToList(_purpose, key.Type(), _id, _secid);
}

void KeyringImp::AddKeyToList(BKeyPurpose _purpose, BKeyType _t, const char* _id, const char* _secid)
{
    keylist.AddItem(new KeyImp(this, _purpose, _t, _id, _secid));
}

void KeyringImp::RemoveKeyFromList(const char* _keyid)
{
    KeyImp* todelete = FindInList(keylist, _keyid);

    if(todelete)
        keylist.RemoveItem(todelete);
}

status_t KeyringImp::DeleteKey(const char* _keyid)
{
    BKeyStore keystore;
    BKey key;
    status_t status = keystore.GetKey(Identifier(), B_KEY_TYPE_ANY, _keyid, key);

    if(status == B_OK) {
        RemoveKeyFromList(_keyid);
        keystore.RemoveKey(Identifier(), key);
    }
    return status;
}

void KeyringImp::AddApplicationToList(const char* _appname)
{
    applicationlist.AddItem(new ApplicationAccessImp(_appname));
}

// uint32 KeyringImp::RetrieveApplications()
// {
    // BKeyStore keystore;
    // uint32 cookie = 0;
    // BString signature;
    // while(keystore.GetNextApplication(Identifier(), cookie, signature) == B_OK) {
        // applicationlist.AddItem(new ApplicationAccessImp(signature));
    // }
    // return cookie;
// }

status_t KeyringImp::RemoveApplication(const char* _signature)
{
    status_t status = B_ERROR;
    BKeyStore keystore;
    ApplicationAccessImp* todelete = FindInList(applicationlist, _signature);

    if(todelete)
        if((status = keystore.RemoveApplication(Identifier(), _signature)) == B_OK)
            applicationlist.RemoveItem(todelete);

    return status;
}

KeyImp* KeyringImp::KeyByIdentifier(const char* _id)
{
    return FindInList(keylist, _id);
}

KeyImp* KeyringImp::KeyAt(int32 index)
{
    return keylist.ItemAt(index);
}

int32 KeyringImp::KeyCount(BKeyType type, BKeyPurpose purpose)
{
    int count = 0;
    for(int i = 0; i < keylist.CountItems(); i++) {
        if((type == B_KEY_TYPE_ANY || keylist.ItemAt(i)->Type() == type) &&
        (purpose == B_KEY_PURPOSE_ANY || keylist.ItemAt(i)->Purpose() == purpose))
            count++;
    }
    return count;
}

ApplicationAccessImp* KeyringImp::ApplicationBySignature(const char* _signature)
{
    return FindInList(applicationlist, _signature);
}

ApplicationAccessImp* KeyringImp::ApplicationAt(int32 index)
{
    return applicationlist.ItemAt(index);
}

int32 KeyringImp::ApplicationCount()
{
    return applicationlist.CountItems();
}

void KeyringImp::PrintToStream()
{
    printf("\tKeyring: %s. %d keys. %d applications.\n", Identifier(),
        keylist.CountItems(), applicationlist.CountItems());
    for(int i = 0; i < keylist.CountItems(); i++)
        keylist.ItemAt(i)->PrintToStream();
    for(int i = 0; i < applicationlist.CountItems(); i++)
        applicationlist.ItemAt(i)->PrintToStream();
}

void KeyringImp::Reset()
{
    // Reset the data structure without touching the actual data on disk
    for(int i = 0; i < keylist.CountItems(); i++)
        keylist.RemoveItem(keylist.ItemAt(i));
    for(int i = 0; i < applicationlist.CountItems(); i++)
        applicationlist.RemoveItem(applicationlist.ItemAt(i));
}

// #pragma mark -

KeystoreImp::KeystoreImp()
{
}

KeystoreImp::~KeystoreImp()
{
    Reset();
}

status_t KeystoreImp::CreateKeyring(const char* _name)
{
    BKeyStore keystore;
    status_t status = keystore.AddKeyring(_name);
    if(status == B_OK)
        AddKeyring(_name);

    return status;
}

void KeystoreImp::AddKeyring(const char* _name)
{
    keyringlist.AddItem(new KeyringImp(_name));
}

status_t KeystoreImp::RemoveKeyring(const char* _name)
{
    status_t status;
    KeyringImp* todelete = FindInList(keyringlist, _name);

    if(todelete) {
        BKeyStore keystore;
        status = keystore.RemoveKeyring(todelete->Identifier());
        keyringlist.RemoveItem(todelete);
    }
    return status;
}

KeyringImp* KeystoreImp::KeyringByName(const char* _name)
{
    return FindInList(keyringlist, _name);
}

KeyringImp* KeystoreImp::KeyringAt(int32 index)
{
    return keyringlist.ItemAt(index);
}

int32 KeystoreImp::KeyringCount()
{
    return keyringlist.CountItems();
}

void KeystoreImp::PrintToStream()
{
    printf("Keystore. %d keyrings.\n", keyringlist.CountItems());

    for(int i = 0; i < keyringlist.CountItems(); i++)
        keyringlist.ItemAt(i)->PrintToStream();
}

void KeystoreImp::Reset()
{
    // Reset the data structure without touching the actual data on disk
    for(int i = 0; i < keyringlist.CountItems(); i++)
        keyringlist.RemoveItem(keyringlist.ItemAt(i));
}

