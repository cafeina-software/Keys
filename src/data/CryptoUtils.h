/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __CRYPYO_UTILS_H_
#define __CRYPYO_UTILS_H_

#include <DataIO.h>
#include <SupportDefs.h>

status_t EncryptData(BPositionIO* indata, ssize_t inlenght, const char* inpath,
    const char* pass, const unsigned char* iv, BMallocIO* outdata,
    BPositionIO* outdp, BPositionIO* outiv);
status_t DecryptData(BPositionIO* indata, ssize_t inlenght, const char* pass,
    const unsigned char* iv, BMallocIO* outdata,
    BPositionIO* outdp, BPositionIO* outiv);

status_t GenerateSalt(size_t length, BPositionIO* outdata);
status_t SHA256CheckSum(BPositionIO* indata, ssize_t inlength, BPositionIO* outdata);
const char* HashToHashstring(const unsigned char* indata, ssize_t inlenght);

void memzero(void* ptr, size_t len);

#endif /* __CRYPYO_UTILS_H_ */
