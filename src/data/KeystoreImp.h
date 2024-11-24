/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __KEYRING_IMP_H_
#define __KEYRING_IMP_H_

#include <Key.h>
#include <ObjectList.h>
#include <SupportDefs.h>

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

template <typename T>
T* FindInList2(BObjectList<T> list, const char* idstring, const char* secstring) {
    T* selection = NULL;

	int i = 0;
	bool found = false;

	while(i < list.CountItems() && !found) {
		if(strcmp(list.ItemAt(i)->Identifier(), idstring) == 0 &&
        strcmp(list.ItemAt(i)->SecondaryIdentifier(), secstring) == 0) {
			found = true;
			selection = list.ItemAt(i);
		}
		i++;
	}

	return selection;
}

class KeyringImp;
class KeystoreImp;

class KeyImp
{
public:
                KeyImp(KeyringImp* parent);
                KeyImp(KeyringImp* parent, BKey key);
                KeyImp(KeyringImp* parent, BKeyPurpose, BKeyType t,
                    const char* id, const char* secid = nullptr,
                    bigtime_t dc = 0, const char* owner = "");

        /* Getters */
    KeyringImp *Parent();
    const char *Identifier();
    const char *SecondaryIdentifier();
    BKeyType    Type();
    BKeyPurpose Purpose();
    bigtime_t   Created();
    const char *Owner();
    void        Data(const void* ptr, size_t* len);

        /* No setters: the API does not seem to allow direct key editing... */

        /* Data output */
    [[maybe_unused]]
    void        PrintToStream();
    status_t    Export(BMessage* archive);
private:
    void        SetTo(BKeyPurpose, BKeyType t, const char* id, const char* secid,
                    bigtime_t dc, const char* owner);
private:
    KeyringImp *fParent;
    BKeyPurpose fPurpose;
    BKeyType    fType;
    BString     fIdentifier,
                fSecIdentifier,
                fOwner;
    bigtime_t   fCreated;
};

class ApplicationAccessImp
{
public:
                ApplicationAccessImp(KeyringImp* parent, const char* signature);

    KeyringImp *Parent();
    const char *Identifier();
    status_t    GetRef(entry_ref* ref);
    [[maybe_unused]]
    void        PrintToStream();
private:
    KeyringImp *fParent;
    BString     fSignature;
};

class KeyringImp
{
public:
                KeyringImp(KeystoreImp* parent, const char* name);
               ~KeyringImp();

   KeystoreImp *Parent();
    const char *Identifier();
    bool        IsUnlocked();
    status_t    Lock();
    status_t    Unlock();
    status_t    SetUnlockKey(BMessage* data);
    status_t    RemoveUnlockKey();

    status_t    AddKey(BKeyPurpose p, BKeyType t, const char* id,
                    const char* secid, const uint8* data = nullptr,
                    size_t length = 0, bool createInDb = false);
    status_t    ImportKey(BMessage* archive);
    status_t    RemoveKey(const char* id, bool deleteInDb = false);
    status_t    RemoveKey(const char* id, const char* secid = nullptr,
                    bool deleteInDb = false);

    KeyImp     *KeyAt(int32 index);
    KeyImp     *KeyByIdentifier(const char* id);
    KeyImp     *KeyByIdentifier(const char* id, const char* secondary_id);
    int32       KeyCount(BKeyType = B_KEY_TYPE_ANY, BKeyPurpose = B_KEY_PURPOSE_ANY);

    void        AddApplicationToList(const char* signature);
    status_t    RemoveApplication(const char* signature, bool deleteInDb = false);
    ApplicationAccessImp *ApplicationAt(int32 index);
    ApplicationAccessImp *ApplicationBySignature(const char* signature);
    int32       ApplicationCount();

    [[maybe_unused]]
    void        PrintToStream();
    void        Reset();
private:
   KeystoreImp *fParent;
    BString     fName;
    bool        fHasUnlockKey,
                fIsUnlocked;
    BObjectList<KeyImp> fKeyList;
    BObjectList<ApplicationAccessImp> fAppList;
};

class KeystoreImp
{
public:
    KeystoreImp();
    ~KeystoreImp();

    status_t    AddKeyring(const char* name, bool createInDb = false);
    status_t    RemoveKeyring(const char* name, bool deleteInDb = false);
    KeyringImp *KeyringAt(int32 index);
    KeyringImp *KeyringByName(const char* name);
    int32       KeyringCount();

    [[maybe_unused]]
    void        PrintToStream();
    bool        IsEmpty();
    void        Reset();
private:
    BObjectList<KeyringImp> fKeyringList;
};

#endif
