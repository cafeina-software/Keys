/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "PasswordStrength.h"
#include <Key.h>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

/* Check password entropy: the higher the chance of randomness, the less
   likely it will be guessed (or at least it tries to do that).

   It does not check any "more arbitrary" requirements, such as the enforcement
   of a minimum length or the requirement of having different kinds of symbols.
*/
float PasswordStrength(const char* password)
{
    if(!password || strcmp(password, "") == 0)
        return 0.0f;

    size_t length = strlen(password);

    size_t dts_count = strlen("0123456789"); // 10
    size_t lcs_count = strlen("abcdefghijklmnopqrstuvwxyz"); // 26
    size_t ucs_count = strlen("ABCDEFGHIJKLMNOPQRSTUVWXYZ"); // 26
    size_t sym_count = strlen(" !\"#$%&\'()*+,-./:;<=>\?@[\\]^_`{|}~"); // 33

    uint64 lc = 0, uc = 0, dt = 0, sym = 0;
    for(int i = 0; i < static_cast<int>(length); i++) {
        if(islower(password[i]) != 0) lc++;
        else if(isupper(password[i]) != 0) uc++;
        else if(isdigit(password[i]) != 0) dt++;
        else if(ispunct(password[i]) != 0 || password[i] == ' ') sym++;
    }

    float maxentropy = log2(pow(dts_count + lcs_count + ucs_count + sym_count, length));
    size_t count = (dt > 0 ? dt : 0) +
                   (lc > 0 ? lc : 0) +
                   (uc > 0 ? uc : 0) +
                   (sym > 0 ? sym : 0);
    float actualent = log2(pow(count, length));

    if(maxentropy == 0) // prevent division by zero
        return 0.0f;

    float result = actualent / maxentropy;

    // prevent out of range values
    if(result < 0.0f)
        return 0.0f;
    if(result > 1.0f)
        return 1.0f;

    return result;
}

status_t GeneratePassword(BPasswordKey& password, size_t length, uint32 flags)
{
    /* Seed data initialization */
    int fd = open("/dev/random", O_RDONLY);
    if(fd < 0) {
        fprintf(stderr, "Error (%s): %s (%d).\n", __func__, strerror(errno), errno);
        return static_cast<status_t>(errno);
    }
    unsigned char* buffer = new unsigned char[length];
    ssize_t readbytes = read(fd, buffer, length);
    if(readbytes < 0) {
        fprintf(stderr, "Error (%s): %s (%d).\n", __func__, strerror(errno), errno);
        delete[] buffer;
        return static_cast<status_t>(errno);
    }

    /* Dictionary and hashing */
    const char* dictionary = "0123456789abcdefghijklmnopqrstuvwxyzABCD"
                             "EFGHIJKLMNOPQRSTUVWXYZ!#$%&()*+,-./:;<=>"
                             "@[]^_`{|}~";
    auto hash = [=](unsigned char iv, int length) {
        return length * iv % strlen(dictionary);
    };

    char* pwddata = new char[length + 1];
    if(!pwddata) {
        fprintf(stderr, "Error (%s): no memory.\n", __func__);
        delete[] buffer;
        return B_NO_MEMORY;
    }
    for(int i = 0; i < static_cast<int>(strlen(reinterpret_cast<char*>(buffer))); i++) {
        pwddata[i] = dictionary[hash(buffer[i], length)];
    }
    pwddata[length] = '\0';

    /* Export */
    password.SetTo(reinterpret_cast<const char*>(pwddata), password.Purpose(),
        password.Identifier(), password.SecondaryIdentifier());

    delete[] pwddata;
    delete[] buffer;
    int result = close(fd);
    if(result != 0)
        return static_cast<status_t>(errno);

    return B_OK;
}
