/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <DateTime.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Roster.h>
#include <cstdio>
#include <openssl/evp.h>
#include "BackUpUtils.h"
// #if defined(USE_OPENSSL)
#include "CryptoUtils.h"
// #endif

status_t DBBasePath(BPath* path);
const char* CurrentDateTimeString();
status_t InitDataBaseFile(BFile* file, BString* path);
status_t WriteMetadata(const char* basepath, const char* original_filename, ssize_t inlenght,
    const char* target_filename,const char* pass, const char* iv,
    const unsigned char* derived_pass, const unsigned char* derived_iv);

// #pragma mark - Public

status_t DoPlainKeystoreBackup()
{
    BFile infile;
    BString inpath;
    if(InitDataBaseFile(&infile, &inpath) != B_OK)
        return B_ERROR;

    if(infile.Lock() != B_OK)
        return B_ERROR;

    off_t size = 0;
    infile.GetSize(&size);
    unsigned char* buffer = new unsigned char[size];
    ssize_t readbytes = infile.Read(buffer, size);
    if(readbytes != size) {
        delete[] buffer;
        infile.Unlock();
        return B_ERROR;
    }

    BPath outpath;
    if(DBBasePath(&outpath) != B_OK)
        return B_ERROR;
    BString targetname("keystore_database_");
    targetname.Append(CurrentDateTimeString());
    outpath.Append(targetname.String());
    BFile outfile(outpath.Path(), B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
    if(outfile.InitCheck() != B_OK) {
        delete[] buffer;
        infile.Unlock();
        return B_ERROR;
    }
    ssize_t writtenbytes = outfile.Write(buffer, size);
    if(writtenbytes != size) {
        BEntry entry(outpath.Path());
        entry.Remove();
        delete[] buffer;
        infile.Unlock();
        return B_ERROR;
    }

    delete[] buffer;
    infile.Unlock();
    return B_OK;
}

// #if defined(USE_OPENSSL)

status_t DoEncryptedKeystoreBackup(const char* password)
{
    BFile infile;
    BString inpath;
    if(InitDataBaseFile(&infile, &inpath) != B_OK)
        return B_ERROR;

    if(infile.Lock() != B_OK)
        return B_ERROR;

    ssize_t filesize = 0;
    if(infile.GetSize(&filesize) != B_OK) {
        fprintf(stderr, "Error: file size of file could not be retrieved.\n");
        return B_ERROR;
    }

    BMallocIO salt;
    if(GenerateSalt(16, &salt) != B_OK)
        return B_ERROR;

    BMallocIO alloc, outdp, outiv;
    if(EncryptData(&infile, filesize, inpath.String(), password, (const unsigned char*)salt.Buffer(), &alloc, &outdp, &outiv) != B_OK) {
        alloc.SetSize(0);
        infile.Unlock();
        return B_ERROR;
    }

    BPath basepath;
    DBBasePath(&basepath);
    BString outpathstr;
    outpathstr.Append("keystore_database_").Append(CurrentDateTimeString()).Append(".crypt");
    BPath outpath(basepath.Path(), outpathstr.String());
    BFile outfile(outpath.Path(), B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
    if(outfile.InitCheck() != B_OK) {
       fprintf(stderr, "Error!!! Unable to open outfile %s.\n", outpath.Path());
        return -1;
    }

    if(outfile.Write(alloc.Buffer(), alloc.BufferLength()) < 0)
        fprintf(stderr, "Error!!! Not any bytes written to outfile.\n");

    infile.Unlock();

    if(WriteMetadata(basepath.Path(), BPath(inpath.String()).Leaf(), filesize,
    outpathstr.String(), password, (char*)salt.Buffer(),
    (const unsigned char*)outdp.Buffer(), (const unsigned char*)outiv.Buffer()) != 0)
        fprintf(stderr, "Warning: the metadata file could not be written. Please remember your data.\n");

    return B_OK;
}

status_t RestoreEncryptedKeystoreBackup(const char* path, const char* password)
{
    BFile datafile(path, B_READ_ONLY);
    if(datafile.InitCheck() != B_OK)
        return B_NO_INIT;

    BMessage data;
    data.Unflatten(&datafile);

    BString originalfn, targetfn, ivec;
    if(data.FindString("original_file_name", &originalfn) != B_OK ||
    data.FindString("target_file_name", &targetfn) != B_OK ||
    data.FindString("ivec", &ivec) != B_OK) {
        fprintf(stderr, "Error: bad data. Unrecognized data file or the file has missing fields.\n");
        return B_BAD_DATA;
    }

    BPath basepath;
    DBBasePath(&basepath);
    BPath cryptopath(basepath.Path(), targetfn.String(), true);
    BFile cryptofile(cryptopath.Path(), B_READ_ONLY);
    ssize_t inlength = 0;
    cryptofile.GetSize(&inlength);
    BMallocIO alloc, outdp, outiv;
    alloc.SetBlockSize(1024);
    if(DecryptData(&cryptofile, inlength, password, (const unsigned char*)ivec.String(), &alloc, &outdp, &outiv) != B_OK) {
        fprintf(stderr, "Error: decryption error.\n");
        return B_ERROR;
    }

    BPath dbpath;
    DBPath(&dbpath);
    if(BEntry(dbpath.Path()).Exists()) {
        // First we try to stop the keystore server to prevent conflicts with the current database file
        while(be_roster->IsRunning("application/x-vnd.Haiku-keystore_server")) {
            team_id team = be_roster->TeamFor("application/x-vnd.Haiku-keystore_server");
            BMessenger msgr("application/x-vnd.Haiku-keystore_server", team);
            msgr.SendMessage(B_QUIT_REQUESTED);
        }

        // Check again
        if(be_roster->IsRunning("application/x-vnd.Haiku-keystore_server")) {
            fprintf(stderr, "Error: despite the attempt to stop it, it is still running.\n");
            return B_NOT_ALLOWED;
        }

        // Rename the file
        if(BEntry(dbpath.Path()).Rename(BString(dbpath.Leaf()).Append("_").Append(CurrentDateTimeString()), false) != B_OK)
            fprintf(stderr, "Error: the old database file could not be renamed.\n");
    }

    // With the old file moved (or not existing), now we can create the new decrypted file
    BFile outfile(BPath(dbpath.Path(), "keystore_database").Path(), B_READ_WRITE | B_CREATE_FILE | B_FAIL_IF_EXISTS);
    if(outfile.Write(alloc.Buffer(), alloc.BufferLength()) < 0)
        fprintf(stderr, "Error: the data was not written properly or at all.\n");

    return 0;
}

// #endif

status_t DBPath(BPath* path)
{
    BPath dbpath;
    if(DBBasePath(&dbpath) != B_OK)
        return B_ERROR;
    if(dbpath.Append("keystore_database", true) != B_OK)
        return B_ERROR;
    if(path->SetTo(dbpath.Path()) != B_OK)
        return B_ERROR;
    return B_OK;
}

// #pragma mark - Private

status_t DBBasePath(BPath* path)
{
    BPath basepath;
    if(find_directory(B_USER_SETTINGS_DIRECTORY, &basepath) != B_OK)
        return B_ERROR;
    if(basepath.Append("system/keystore", true) != B_OK)
        return B_ERROR;
    if(path->SetTo(basepath.Path()) != B_OK)
        return B_ERROR;
    return B_OK;
}

const char* CurrentDateTimeString()
{
    BDateTime now(BDateTime::CurrentDateTime(B_GMT_TIME));
    BString targetname("");
    targetname << (now.Date().Year())                   << "-"
               << (now.Date().Month()  < 10 ? "0" : "") << now.Date().Month()  << "-"
               << (now.Date().Day()    < 10 ? "0" : "") << now.Date().Day()    << "_"
               << (now.Time().Hour()   < 10 ? "0" : "") << now.Time().Hour()   << "-"
               << (now.Time().Minute() < 10 ? "0" : "") << now.Time().Minute() << "-"
               << (now.Time().Second() < 10 ? "0" : "") << now.Time().Second();
    return targetname.String();
}

status_t InitDataBaseFile(BFile* file, BString* path)
{
    BPath inpath;
    if(DBPath(&inpath) != B_OK) {
        fprintf(stderr, "Error: could not retrieve database path.\n");
        return B_ERROR;
    }

    file->SetTo(inpath.Path(), B_READ_ONLY);
    if(file->InitCheck() != B_OK) {
        fprintf(stderr, "Error: could not initialize BFile for database.\n");
        return B_ERROR;
    }
    if(path)
        path->SetTo(inpath.Path());

    return B_OK;
}

// #if defined(USE_OPENSSL)

status_t WriteMetadata(const char* basepath, const char* original_filename,
    ssize_t inlenght, const char* target_filename, const char* pass, const char* iv,
    const unsigned char* derived_pass, const unsigned char* derived_iv)
{
    BMessage metadata;
    metadata.AddString("base_path", basepath);
    metadata.AddString("original_file_name", original_filename);
    metadata.AddString("target_file_name", target_filename);
    metadata.AddString("pass", pass);
    metadata.AddString("ivec", iv);
    metadata.AddData("hashed_pass", B_RAW_TYPE, derived_pass, strlen((const char*)derived_pass));
    metadata.AddData("hashed_ivec", B_RAW_TYPE, derived_iv, strlen((const char*)derived_iv));

    BPath path(basepath, original_filename, true);
    BFile infile(path.Path(), B_READ_ONLY);

    infile.Lock();
    BMallocIO sum;
    SHA256CheckSum(&infile, inlenght, &sum);
    const char* sumstring = HashToHashstring((const unsigned char*)sum.Buffer(), sum.BufferLength());
    infile.Unlock();

    metadata.AddString("original_file_hash", sumstring);

    BPath outpath(basepath, target_filename, true);
    BFile outfile(outpath.Path(), B_READ_ONLY);

    outfile.Lock();
    ssize_t outlength = 0;
    outfile.GetSize(&outlength);
    BMallocIO tgsum;
    SHA256CheckSum(&outfile, outlength, &tgsum);
    const char* tgsumstring = HashToHashstring((const unsigned char*)tgsum.Buffer(), tgsum.BufferLength());
    outfile.Unlock();

    metadata.AddString("target_file_hash", tgsumstring);

    BPath targetfilepath(target_filename);
    BPath parentpath;
    targetfilepath.GetParent(&parentpath);
    BPath metadatapath(basepath, BString(targetfilepath.Leaf()).Append(".dat").String());

    fprintf(stderr, "Meta file at: %s\n", metadatapath.Path());
    BFile metaoutfile(metadatapath.Path(), B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
    if(metaoutfile.InitCheck() != B_OK)
        return -1;

    metadata.Flatten(&metaoutfile);
    return 0;
}

// #endif
