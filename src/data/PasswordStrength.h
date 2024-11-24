/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __PASSWD_STRENGTH_IMP_H_
#define __PASSWD_STRENGTH_IMP_H_

#include <Key.h>
#include <SupportDefs.h>

float PasswordStrength(const char* password);
status_t GeneratePassword(BPasswordKey& password, size_t length, uint32 flags);

#endif /* __PASSWD_STRENGTH_IMP_H_ */
