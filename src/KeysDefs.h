/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __KEYS_DEFS_H_
#define __KEYS_DEFS_H_

#define kAppName "Keys"
#define kAppDesc ""
#define kAppSignature "application/x.vnd.cafeapp.keystoremgmt"
#define kAppVersionStr "0.0.1"
#define kAppHomePage ""

#if defined(DEBUG) || defined(_DEBUG)
#define __trace(x, ...) fprintf(stderr, kAppName " @ %s: " x, __func__, ##__VA_ARGS__)
#else
#define __trace(x, ...)
#endif

#define M_DATA_TO_BACKUP         'bkp_'
#define M_USER_ADDS_KEYRING      'adkr'
#define M_USER_REMOVES_KEYRING   'rmkr'
#define M_USER_PROVIDES_KEY      'hsky'
#define M_USER_REMOVES_KEY       'rmky'
#define M_USER_REMOVES_APPAUTH   'rmap'

enum ui_status {
    S_UI_NO_KEYRING_IN_FOCUS,
    S_UI_HAS_KEYRING_IN_FOCUS
};

#endif /* __KEYS_DEFS_H_ */
