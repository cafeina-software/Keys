/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "ListViewEx.h"
#include <Window.h>

BListViewEx::BListViewEx(BRect frame, const char* name, list_view_type type,
    uint32 resizeMask, uint32 flags, BPopUpMenu* pmenu, BPopUpMenu* imenu)
: BListView(frame, name, type, resizeMask, flags),
  parentmenu(pmenu),
  itemmenu(imenu)
{
}

BListViewEx::BListViewEx(const char* name, list_view_type type,
    uint32 flags, BPopUpMenu* pmenu, BPopUpMenu* imenu)
: BListView(name, type, flags),
  parentmenu(pmenu),
  itemmenu(imenu)
{
}

BListViewEx::BListViewEx(list_view_type type, BPopUpMenu* pmenu, BPopUpMenu* imenu)
: BListView(type),
  parentmenu(pmenu),
  itemmenu(imenu)
{
}

BListViewEx::BListViewEx(BMessage* data, BPopUpMenu* pmenu, BPopUpMenu* imenu)
: BListView(data),
  parentmenu(pmenu),
  itemmenu(imenu)
{
}

BListViewEx::~BListViewEx()
{
}

void BListViewEx::AttachedToWindow()
{
    BListView::AttachedToWindow();
    parentmenu->SetTargetForItems(Window());
    itemmenu->SetTargetForItems(Window());
}

void BListViewEx::MouseDown(BPoint where)
{
    uint32 buttons;
    GetMouse(&where, &buttons, true);
    if(buttons & B_SECONDARY_MOUSE_BUTTON) {
        BPoint spawnPoint(where);
        Parent()->ConvertToScreen(&spawnPoint);

        if(IndexOf(where) < 0) {
            if(parentmenu)
                parentmenu->Go(spawnPoint, true, true, true);
        }
        else {
            if(itemmenu) // we should somehow do not do this blindly
                itemmenu->Go(spawnPoint, true, true, true);
        }
    }

    BListView::MouseDown(where);

}

void BListViewEx::MessageReceived(BMessage* msg)
{
    BListView::MessageReceived(msg);
}

