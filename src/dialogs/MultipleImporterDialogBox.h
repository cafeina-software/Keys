/*
 * Copyright 2025, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __MULTIPLE_IMPORTER_DIALOG__
#define __MULTIPLE_IMPORTER_DIALOG__

#include <SupportDefs.h>
#include <Window.h>
#include <functional>
#include <private/interface/ColumnListView.h>

enum MIDBCmds {
    MIDB_CANCEL = 'cncl',
    MIDB_GO = 'gogo',
    MIDB_IMPORTABLE_SELECTED = 'isel',
    MIDB_IMPORTABLE_ENABLED = 'ichk',
    MIDB_IMPORTABLE_DISABLED = 'idis'
};

class MultipleImporterDialogBox : public BWindow
{
public:
                    MultipleImporterDialogBox(BWindow* parent, const char* target, BMessage* data, BMessage* discarded);
    virtual         ~MultipleImporterDialogBox();

    virtual void    AttachedToWindow();
    virtual void    MessageReceived(BMessage* msg);

            BWindow* Parent() { return fParent; }
private:
            void    _InitAppData(BMessage* data, BMessage* discarded);
            void    _ForEachSelected(std::function<void(BRow*)> fn);
            // void    _ImporterCall();
private:
    BWindow* fParent;
    BColumnListView* fImportableView;
    BColumnListView* fDroppedView;
    BButton* fCheckOnButton;
    BButton* fCheckOffButton;
    BButton* fImportButton;
    BButton* fCancelButton;

    const char* fTarget;
    BMessage importerData;
    int32 fSelectedStatus[3];
};

#endif /* __MULTIPLE_IMPORTER_DIALOG__ */
