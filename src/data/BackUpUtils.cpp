/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <DateTime.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include "BackUpUtils.h"

status_t DBPath(BPath* path);
status_t DBBasePath(BPath* path);
const char* CurrentDateTimeString();

status_t DoPlainKeystoreBackup()
{
    BPath inpath;
    if(DBPath(&inpath) != B_OK)
        return B_ERROR;

    BFile infile(inpath.Path(), B_READ_ONLY);
    if(infile.InitCheck() != B_OK)
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
