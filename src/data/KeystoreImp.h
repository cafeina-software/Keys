/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __KEYRING_IMP_H_
#define __KEYRING_IMP_H_

#include <Key.h>
#include <list>

template <typename T>
T* FindInList(BObjectList<T> list, const char* idstring) {
	T* selection = NULL;

	int i = 0;
	bool found = false;

	while(i < list.CountItems() && !found) {
		if(strcmp(list.ItemAt(i)->Identifier(), idstring) == 0) {
			found = true;
			selection = list.ItemAt(i);
		}
		i++;
	}

	return selection;
}

class KeyringImp;

class KeyImp
{
public:
    KeyImp(KeyringImp* _owner, BKeyPurpose purpose, BKeyType t, const char* id,
        const char* secid = NULL);

    const char* Identifier();
    const char* SecondaryIdentifier();
    BKeyType Type();
    BKeyPurpose Purpose();

    BKey GetBKey();

    void PrintToStream();
private:
    KeyringImp *owner;
    BKeyPurpose purpose;
    BKeyType type;
    BString identifier;
    BString secidentifier;
};

class ApplicationAccessImp
{
public:
    ApplicationAccessImp(const char* _signature);
    const char* Identifier();

    void PrintToStream();
private:
    BString signature;
};

class KeyringImp
{
public:
    KeyringImp(const char* _name);
    ~KeyringImp();
    const char* Identifier();

	void CreateKey(BKeyPurpose purpose, const char* id, const char* secid = NULL,
        const uint8* data = NULL, size_t length = 0);
    void AddKeyToList(BKeyPurpose _purpose, BKeyType t, const char* _id, const char* _secid);
    void RemoveKeyFromList(const char* _id);
    status_t DeleteKey(const char* _id);
    KeyImp* KeyByIdentifier(const char* _id);
    KeyImp* KeyAt(int32 index);
    int32 KeyCount(BKeyType type = B_KEY_TYPE_ANY, BKeyPurpose purpose = B_KEY_PURPOSE_ANY);

    void AddApplicationToList(const char* _appname);
	// uint32 RetrieveApplications();
    status_t RemoveApplication(const char* _signature);
    ApplicationAccessImp* ApplicationBySignature(const char* _signature);
    ApplicationAccessImp* ApplicationAt(int32 index);
    int32 ApplicationCount();

    void PrintToStream();
    void Reset();
private:
    BString name;
    BObjectList<KeyImp> keylist;
    BObjectList<ApplicationAccessImp> applicationlist;
};

class KeystoreImp
{
public:
    KeystoreImp();
    ~KeystoreImp();
    status_t CreateKeyring(const char* _name);
    void AddKeyring(const char* _name);
    status_t RemoveKeyring(const char* _name);
    KeyringImp* KeyringByName(const char* _name);
    KeyringImp* KeyringAt(int32 index);
    int32 KeyringCount();

    void PrintToStream();
    void Reset();
private:
    BObjectList<KeyringImp> keyringlist;
};

#endif
