/*
 * Copyright 2025, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <Application.h>
#include <Button.h>
#include <Catalog.h>
#include <File.h>
#include <Key.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <Window.h>
#include <private/interface/ColumnListView.h>
#include <private/interface/ColumnTypes.h>
#include "MultipleImporterDialogBox.h"
#include "../data/KeystoreImp.h"
#include "../ui/ListViewEx.h"
#include "../KeysDefs.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Multiple importer dialog box"

MultipleImporterDialogBox::MultipleImporterDialogBox(BWindow* parent,
    const char* target, BMessage* data, BMessage* discarded)
: BWindow(BRect(0, 0, 400, 400), "", B_FLOATING_WINDOW,
    B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
  fParent(parent),
  fTarget(target),
  importerData(*data)
{
    fSelectedStatus[B_CONTROL_ON] = 0;
    fSelectedStatus[B_CONTROL_OFF] = 0;
    fSelectedStatus[B_CONTROL_PARTIALLY_ON] = 0;

    BString title(B_TRANSLATE("Multiple importer: keys for %keyringName%"));
    title.ReplaceAll("%keyringName%", target);
    SetTitle(title.String());

    BString description(B_TRANSLATE("The following files contain keys that can be imported to the %keyringName% keyring:"));
    description.ReplaceAll("%keyringName%", target);
    BStringView* initialView = new BStringView("sv_initial", description.String());

    fImportableView = new BColumnListView("clv_import", 0, B_FANCY_BORDER);
    fImportableView->SetSelectionMessage(new BMessage(MIDB_IMPORTABLE_SELECTED));
    fImportableView->AddColumn(new BCheckStringColumn(B_TRANSLATE_COMMENT("File ref.", "Abbreviated from \'File reference\'"), 150, 100, 300, false), 0);
    fImportableView->AddColumn(new BStringColumn(B_TRANSLATE("Identifier"), 125, 100, 300, false), 1);
    fImportableView->AddColumn(new BStringColumn(B_TRANSLATE("Sec. identifier"), 125, 100, 300, false), 2);
    fImportableView->AddColumn(new BStringColumn(B_TRANSLATE("Type"), 100, 50, 150, false), 3);
    fImportableView->AddColumn(new BStringColumn(B_TRANSLATE("Purpose"), 100, 50, 150, false), 4);
    fImportableView->AddColumn(new BStringColumn(B_TRANSLATE("File path"), 125, 100, 300, false), 5);

    BString dropped(B_TRANSLATE("The following files will not be used because of some error:"));
    BStringView* droppedView = new BStringView("sv_dropped", dropped.String());

    fDroppedView = new BColumnListView("dropped", 0, B_FANCY_BORDER);
    fDroppedView->AddColumn(new BStringColumn(B_TRANSLATE_COMMENT("File ref.", "Abbreviated from \'File reference\'"), 150, 100, 300, false), 0);
    fDroppedView->AddColumn(new BStringColumn(B_TRANSLATE("Reason"), 200, fDroppedView->StringWidth("Reason"), 500, false), 1);

    fCheckOnButton = new BButton("bt_on", B_TRANSLATE("Enable"), new BMessage(MIDB_IMPORTABLE_ENABLED));
    fCheckOnButton->SetEnabled(false);
    fCheckOffButton = new BButton("bt_off", B_TRANSLATE("Disable"), new BMessage(MIDB_IMPORTABLE_DISABLED));
    fCheckOffButton->SetEnabled(false);
    fImportButton = new BButton("bt_import", B_TRANSLATE("Import keys"), new BMessage(MIDB_GO));
    fImportButton->SetEnabled(true); // Later all initialized as B_CONTROL_ON
    fCancelButton = new BButton("bt_cancel", B_TRANSLATE("Cancel"), new BMessage(MIDB_CANCEL));

    BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
        .SetInsets(B_USE_WINDOW_INSETS)
        .AddSplit(B_VERTICAL, B_USE_SMALL_SPACING)
            .AddGroup(B_VERTICAL, 0, 0.70f)
                .AddGroup(B_VERTICAL, 0)
                    .Add(initialView)
                    .Add(fImportableView)
                .End()
                .AddStrut(4.0f)
                .AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
                    .Add(fCheckOnButton)
                    .Add(fCheckOffButton)
                    .AddGlue()
                    .Add(fImportButton)
                    .Add(fCancelButton)
                .End()
            .End()
            .AddGroup(B_VERTICAL, 0, 0.30f)
                .Add(droppedView)
                .Add(fDroppedView)
            .End()
        .End()
    .End();

    _InitAppData(data, discarded);

    CenterIn(fParent->Frame());
}

MultipleImporterDialogBox::~MultipleImporterDialogBox()
{
}

void MultipleImporterDialogBox::AttachedToWindow()
{
    fImportableView->SetTarget(this);
    fCheckOnButton->SetTarget(this);
    fCheckOffButton->SetTarget(this);
    fImportButton->SetTarget(this);
    fCancelButton->SetTarget(this);
}

void MultipleImporterDialogBox::MessageReceived(BMessage* msg)
{
    switch(msg->what)
    {
        case MIDB_IMPORTABLE_SELECTED:
        {
            fSelectedStatus[B_CONTROL_ON] = 0;
            fSelectedStatus[B_CONTROL_OFF] = 0;
            fSelectedStatus[B_CONTROL_PARTIALLY_ON] = 0;

            auto checkSelectionStatuses = [this] (BRow* row) {
                int32 value = dynamic_cast<BCheckStringField*>(row->GetField(0))->Value();
                if(value > 2) // Prevent to pass invalid values
                    return;
                fSelectedStatus[value]++;
            };
            _ForEachSelected(checkSelectionStatuses);

            fCheckOnButton->SetEnabled(fSelectedStatus[B_CONTROL_OFF] > 0 || fSelectedStatus[B_CONTROL_PARTIALLY_ON] > 0);
            fCheckOffButton->SetEnabled(fSelectedStatus[B_CONTROL_ON] > 0 || fSelectedStatus[B_CONTROL_PARTIALLY_ON] > 0);

            bool allDisabled = true;
            for(int i = 0; allDisabled && i < fImportableView->CountRows(); i++)
                allDisabled = ((BCheckStringField*)fImportableView->RowAt(i)->GetField(0))->Value() == B_CONTROL_OFF;

            fImportButton->SetEnabled(!allDisabled);
            break;
        }
        case MIDB_IMPORTABLE_ENABLED:
        {
            auto enabler = [] (BRow* row) {
                ((BCheckStringField*)row->GetField(0))->SetValue(B_CONTROL_ON);
            };
            _ForEachSelected(enabler);
            PostMessage(MIDB_IMPORTABLE_SELECTED);
            break;
        }
        case MIDB_IMPORTABLE_DISABLED:
        {
            auto disabler = [] (BRow* row) {
                ((BCheckStringField*)row->GetField(0))->SetValue(B_CONTROL_OFF);
            };
            _ForEachSelected(disabler);
            PostMessage(MIDB_IMPORTABLE_SELECTED);
            break;
        }
        case MIDB_GO:
        {
            BMessage request(M_KEY_IMPORT);
            request.AddString(kConfigKeyring, fTarget);
            BString wantedRef;
            for(int i = 0; i < fImportableView->CountRows(); i++) {
                BRow* row = fImportableView->RowAt(i);
                if(((BCheckStringField*)row->GetField(0))->Value() == B_CONTROL_ON) {
                    entry_ref ref;
                    BEntry entry(((BCheckStringField*)row->GetField(5))->String());
                    entry.GetRef(&ref);
                    request.AddRef("refs", &ref);
                }
            }
            be_app->PostMessage(&request);
            Quit();
            break;
        }
        case MIDB_CANCEL:
            Quit();
            break;
        default: return BWindow::MessageReceived(msg);
    }
}

// #pragma mark - Private

void MultipleImporterDialogBox::_InitAppData(BMessage* accepted, BMessage* discarded)
{
    int32 index = 0;
    entry_ref ref;
    BRow* row = NULL;
    BFile keyFile;
    BMessage keyFileData;
    BString reason;

    while(accepted->FindRef("refs", index, &ref) == B_OK) {
        keyFile.SetTo(&ref, B_READ_ONLY); // No need for sanitization, already done before
        keyFileData.Unflatten(&keyFile);
        BEntry entry(&ref);
        BPath path;
        entry.GetPath(&path);

        row = new BRow();
        row->SetField(new BCheckStringField(ref.name, B_CONTROL_ON), 0);
        row->SetField(new BStringField(keyFileData.GetString("identifier")), 1);
        row->SetField(new BStringField(keyFileData.GetString("secondaryIdentifier")), 2);
        row->SetField(new BStringField(StringForType(static_cast<BKeyType>(keyFileData.GetUInt32("type", B_KEY_TYPE_ANY)))), 3);
        row->SetField(new BStringField(StringForPurpose(static_cast<BKeyPurpose>(keyFileData.GetUInt32("purpose", B_KEY_PURPOSE_ANY)))), 4);
        row->SetField(new BStringField(path.Path()), 5);
        fImportableView->AddRow(row);

        keyFileData.MakeEmpty();
        keyFile.Unset();
        index++;
    }

    if(fImportableView->CountRows() < 1)
        fImportButton->SetEnabled(false);

    index = 0;
    ref = {};
    row = NULL;
    while(discarded->FindRef("refs", index, &ref) == B_OK) {
        discarded->FindString("reason", index, &reason);

        row = new BRow();
        row->SetField(new BStringField(ref.name), 0);
        row->SetField(new BStringField(reason.String()), 1);
        fDroppedView->AddRow(row);

        index++;
    }
}

void MultipleImporterDialogBox::_ForEachSelected(std::function<void(BRow*)> fn)
{
    BRow* current;
    BRow* prev;
    for(prev = NULL;; prev = current) {
        current = fImportableView->CurrentSelection(prev);
        if(prev != NULL) {
            fn(prev);
            fImportableView->InvalidateRow(prev);
        }

        if(current == NULL)
            break;
    }
}

