/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __BACKUP_UTILS_H_
#define __BACKUP_UTILS_H_

#include <Path.h>
#include <SupportDefs.h>

status_t DoPlainKeystoreBackup();
status_t DoEncryptedKeystoreBackup(const char* password);
status_t RestoreEncryptedKeystoreBackup(const char* path, const char* password);

status_t DBPath(BPath* path);

#endif /* __BACKUP_UTILS_H_ */
