/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __KEYS_DEFS_H_
#define __KEYS_DEFS_H_

#define kAppName        "Keys"
#define kAppDesc        "Haiku key store manager"
#define kAppSignature   "application/x.vnd.cafeapp.keystoremgmt"
#define kAppVersionStr  "0.1.0"
#define kAppHomePage    "https://codeberg.org/cafeina/Keys"

#if defined(DEBUG) || defined(_DEBUG)
#define __trace(x, ...) fprintf(stderr, kAppName " @ %s: " x, __func__, ##__VA_ARGS__)
#else
#define __trace(x, ...)
#endif

#define USE_OPENSSL 1

/* Messages config strings */
#define kConfigPrefix               "keys:"
#define kConfigWhat                 kConfigPrefix   "what"
#define kConfigWho                  kConfigPrefix   "who"
#define kConfigResult               kConfigPrefix   "result"
#define kConfigKeyring              kConfigPrefix   "keyring"
#define kConfigKey                  kConfigPrefix   "key"
#define kConfigKeyName              kConfigKey      ":id"
#define kConfigKeyAltName           kConfigKey      ":secondary"
#define kConfigKeyPurpose           kConfigKey      ":purpose"
#define kConfigKeyType              kConfigKey      ":type"
#define kConfigKeyData              kConfigKey      ":data"
#define kConfigKeyCreated           kConfigKey      ":created"
#define kConfigKeyOwner             kConfigKey      ":owner"
#define kConfigKeyGenLength         kConfigPrefix   "keygen:length"
#define kConfigSignature            kConfigPrefix   "signature"

/* Message subjects  */
#define M_ASK_FOR_REFRESH           'rfsh'
#define M_KEYSTORE_BACKUP           'bkp_'
#define M_KEYSTORE_RESTORE          'rstr'
#define M_KEYSTORE_WIPE_CONTENTS    'wipe'
#define M_KEYRING_CREATE            'adkr'
#define M_KEYRING_DELETE            'rmkr'
#define M_KEYRING_WIPE_CONTENTS     'wpkr'
#define M_KEYRING_LOCK              'lkkr'
#define M_KEYRING_SET_LOCKKEY       'anlk'
#define M_KEYRING_UNSET_LOCKKEY     'rnlk'
#define M_KEY_CREATE                'hsky'
#define M_KEY_GENERATE_PASSWORD     'kygn'
#define M_KEY_IMPORT                'imky'
#define M_KEY_EXPORT                'exky'
#define M_KEY_DELETE                'rmky'
#define M_APPAUTH_DELETE            'rmap'

enum ui_status {
    S_UI_NO_KEYRING_IN_FOCUS,
    S_UI_HAS_KEYRING_IN_FOCUS,
    S_UI_SET_KEYRING_FOCUS,
    S_UI_REMOVE_KEYRING_FOCUS
};

#endif /* __KEYS_DEFS_H_ */
