/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <String.h>
#include <fcntl.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include "CryptoUtils.h"

struct CryptoUtilsStore {
    EVP_CIPHER_CTX* context = nullptr;
    unsigned char* passphrase = nullptr;
    unsigned char* init_vector = nullptr;
    unsigned char* inbuffer = nullptr;
    unsigned char* outbuffer = nullptr;

    ~CryptoUtilsStore() {
        ERR_print_errors_fp(stderr);

        if(context)
            EVP_CIPHER_CTX_free(context);
        if(passphrase)
            delete[] passphrase;
        if(init_vector)
            delete[] init_vector;
        if(inbuffer)
            delete[] inbuffer;
        if(outbuffer)
            delete[] outbuffer;
    }
};
status_t InitializeKeyData(CryptoUtilsStore* store, const char* key, const unsigned char* iv);
int MakeDerivatedKey(const char* pass, const unsigned char* salt, int iterations, int length, unsigned char*& outbuffer);

// #pragma mark - Public

status_t EncryptData(BPositionIO* indata, ssize_t inlenght, const char* inpath, const char* pass,
    const unsigned char* iv, BMallocIO* outdata, BPositionIO* outdp, BPositionIO* outiv)
{
    CryptoUtilsStore store;
    store.context = EVP_CIPHER_CTX_new();
    store.passphrase = new unsigned char[32];
    store.init_vector = new unsigned char[16];
    int blocksize = 1024;
    store.inbuffer = new unsigned char[blocksize];
    store.outbuffer = new unsigned char[blocksize];
    outdata->SetBlockSize(blocksize);

    if(!store.context || !store.passphrase || !store.init_vector) {
        fprintf(stderr, "Error: data store could not be initialized.\n");
        return B_BAD_DATA;
    }

    if(InitializeKeyData(&store, pass, iv) != B_OK)
        return B_ERROR;

    if(EVP_EncryptInit_ex(store.context, EVP_aes_256_cbc(), NULL,
    store.passphrase, store.init_vector) != 1) {
        fprintf(stderr, "Error: could not initialize encryption.\n");
        return B_ERROR;
    }

    int cryptolength = 0;
    ssize_t readlength = 0;
    ssize_t readchunk = 0;
    int outlength = 0;

    while(readlength < inlenght) {
        ssize_t curBlockSize = (inlenght - readlength < blocksize) ? (inlenght - readlength) : blocksize;
        readchunk = indata->ReadAt(readlength, store.inbuffer, curBlockSize);
        if(readchunk < 0) {
            fprintf(stderr, "Error: bad read during encryption, at %zd.\n", readlength);
            return B_FILE_ERROR;
        }

        if(EVP_EncryptUpdate(store.context, store.outbuffer, &outlength, store.inbuffer, readchunk) != 1) {
            fprintf(stderr, "Error: encryption error in chunk at %zd.\n", readlength);
            return B_ERROR;
        }

        if(outdata->WriteAt(cryptolength, store.outbuffer, outlength) < 0) {
            fprintf(stderr, "Error writing to alloc during encryption\n");
            return B_ERROR;
        }

        readlength += readchunk;
        cryptolength += outlength;
    }

    if(EVP_EncryptFinal_ex(store.context, store.outbuffer, &outlength) != 1) {
        fprintf(stderr, "Error: encryption error during final step.\n");
        return B_ERROR;
    }

    if (outlength > 0) {
        if (outdata->WriteAt(cryptolength, store.outbuffer, outlength) < 0) {
            fprintf(stderr, "Error writing final encrypted data\n");
        }
    }

    outdp->Write(store.passphrase, strlen((char*)store.passphrase));
    outiv->Write(store.init_vector, strlen((char*)store.init_vector));

    return B_OK;
}

status_t DecryptData(BPositionIO* indata, ssize_t inlenght, const char* pass,
    const unsigned char* iv, BMallocIO* outdata, BPositionIO* outdp, BPositionIO* outiv)
{
    CryptoUtilsStore store;
    store.context = EVP_CIPHER_CTX_new();
    store.passphrase = new unsigned char[32];
    store.init_vector = new unsigned char[16];
    int blocksize = 1024;
    store.inbuffer = new unsigned char[blocksize];
    store.outbuffer = new unsigned char[blocksize];
    outdata->SetBlockSize(blocksize);

    if(!store.context || !store.passphrase || !store.init_vector) {
        fprintf(stderr, "Error: data store could not be initialized.\n");
        return B_BAD_DATA;
    }

    if(InitializeKeyData(&store, pass, iv) != B_OK)
        return B_ERROR;

    if(EVP_DecryptInit_ex(store.context, EVP_aes_256_cbc(), NULL,
    store.passphrase, store.init_vector) != 1) {
        fprintf(stderr, "Error: could not initialize decryption.\n");
        return  B_ERROR;
    }

    ssize_t readlength = 0;
    ssize_t readchunk = 0;
    int cryptolength = 0;
    int outlength = 0;

    while(readlength < inlenght) {
        ssize_t curBlockSize = (inlenght - readlength < blocksize) ? (inlenght - readlength) : blocksize;
        readchunk = indata->ReadAt(readlength, store.inbuffer, curBlockSize);
        if (readchunk < 0) {
            fprintf(stderr, "Error reading from cryptofile.\n");
            break;
        }

        if(EVP_DecryptUpdate(store.context, store.outbuffer, &outlength, store.inbuffer, readchunk) != 1) {
            fprintf(stderr, "Error: could not decrypt chunk at %zd.\n", readlength);
            break;
        }

        if (outdata->WriteAt(cryptolength, store.outbuffer, outlength) < 0) {
            fprintf(stderr, "Error writing to alloc during decryption\n");
            return B_ERROR;
        }

        readlength += readchunk;
        cryptolength += outlength;
    }

    if(EVP_DecryptFinal_ex(store.context, store.outbuffer, &outlength) != 1) {
        fprintf(stderr, "Error: could not end decryption operation.\n");
        return B_ERROR;
    }

    if (outlength > 0) {
        if (outdata->WriteAt(cryptolength, store.outbuffer, outlength) < 0) {
            fprintf(stderr, "Error writing final encrypted data\n");
        }
    }

    outdp->Write(store.passphrase, strlen((char*)store.passphrase));
    outiv->Write(store.init_vector, strlen((char*)store.init_vector));

    return B_OK;
}

status_t SHA256CheckSum(BPositionIO* indata, ssize_t inlength, BPositionIO* outdata)
{
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if(!context)
        return B_NO_MEMORY;

    if(EVP_DigestInit_ex(context, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(context);
        return B_ERROR;
    }

    int blocksize = 1024;
    unsigned char* buffer = new unsigned char[blocksize];
    ssize_t readlength = 0;
    ssize_t readchunk = 0;

    while(readlength < inlength) {
        ssize_t curBlockSize = (inlength - readlength < blocksize) ? (inlength - readlength) : blocksize;
        readchunk = indata->ReadAt(readlength, buffer, curBlockSize);
        if(readchunk < 0) {
            fprintf(stderr, "Readed %zd of %zd.\n", readlength, inlength);
            fprintf(stderr, "Error: bad read during checksum.\n");
            delete[] buffer;
            EVP_MD_CTX_free(context);
            return B_ERROR;
        }
        else if(readchunk == 0)
            break;

        if(EVP_DigestUpdate(context, buffer, readchunk) != 1) {
            fprintf(stderr, "Error: bad digest update.\n");
            delete[] buffer;
            EVP_MD_CTX_free(context);
            return B_ERROR;
        }

        readlength += readchunk;
    }
    delete[] buffer;

    unsigned int hashlen = 0;
    unsigned char *md = new unsigned char[EVP_MAX_MD_SIZE];
    if(EVP_DigestFinal_ex(context, md, &hashlen) != 1) {
        fprintf(stderr, "Error: bad digest finalization.\n");
        delete[] md;
        EVP_MD_CTX_free(context);
        return B_ERROR;
    }
    outdata->Write((const void*)md, hashlen);

    delete[] md;
    return B_OK;
}

const char* HashToHashstring(const unsigned char* indata, ssize_t inlenght)
{
    BString out;
    char tmp[3];
    int i = 0;
    while(i < inlenght) {
        sprintf(tmp, "%02x", indata[i]);
        out.Append(tmp);
        i++;
    }
    return out.String();
}

status_t GenerateSalt(size_t length, BPositionIO* outdata)
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

    outdata->Write((const void*)buffer, length);

    int result = close(fd);
    if(result != 0)
        return static_cast<status_t>(errno);

    return B_OK;
}

// #pragma mark - Private

int InitializeKeyData(CryptoUtilsStore* store, const char* key, const unsigned char* iv)
{
    if(MakeDerivatedKey(key, iv, 1000, 32, store->passphrase) != 1) {
        fprintf(stderr, "Error: passphrase could not be derived.\n");
        return -1;
    }

    if(MakeDerivatedKey((const char*)iv, iv, 1000, 16, store->init_vector) != 1) {
        fprintf(stderr, "Error: initialization vector could not be derived.\n");
        return -1;
    }

    return 0;
}

int MakeDerivatedKey(const char* pass, const unsigned char* salt,
    int iterations, int length, unsigned char*& outbuffer)
{
    outbuffer = new unsigned char[length];
    int result = PKCS5_PBKDF2_HMAC(pass, strlen(pass), salt, strlen((const char*)salt), iterations, EVP_sha1(), length, outbuffer);

    return result;
}
