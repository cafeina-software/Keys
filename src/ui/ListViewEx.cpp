/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "ListViewEx.h"
#include <ControlLook.h>
#include <Window.h>

// #pragma mark - BCheckStringField

BCheckStringField::BCheckStringField(const char* text, int32 initialState)
: BStringField(text), fValue(initialState)
{
}

void BCheckStringField::SetValue(int32 value)
{
    fValue = value;
}

int32 BCheckStringField::Value()
{
    return fValue;
}

// #pragma mark - BCheckStringColumn

BCheckStringColumn::BCheckStringColumn(const char* title, float width,
    float minWidth, float maxWidth, uint32 truncate, alignment align)
: BStringColumn(title, width, minWidth, maxWidth, truncate, align)
{
    SetWantsEvents(true);
}

void BCheckStringColumn::DrawField(BField* field, BRect rect, BView* targetView)
{
    BRect cbRekt(rect.LeftTop(), BSize(rect.bottom - rect.top, rect.Height()));
    float rectLeft = cbRekt.right;
    cbRekt = cbRekt.InsetBySelf(2, 2).OffsetBySelf(2, 0);

    uint32 checkBoxFlags = BControlLook::B_BLEND_FRAME;
    if(((BCheckStringField*)field)->Value() == B_CONTROL_ON)
        checkBoxFlags |= BControlLook::B_ACTIVATED;
    else if(((BCheckStringField*)field)->Value() == B_CONTROL_PARTIALLY_ON)
        checkBoxFlags |= BControlLook::B_PARTIALLY_ACTIVATED;

    be_control_look->DrawCheckBox(targetView, cbRekt, rect, {0, 0, 0}, checkBoxFlags);

    targetView->SetHighUIColor(B_LIST_ITEM_TEXT_COLOR);
    rect.left = rectLeft;
    BStringColumn::DrawField(field, rect, targetView);
}

void BCheckStringColumn::MouseDown(BColumnListView* parent, BRow* row,
    BField* field, BRect fieldRect, BPoint point, uint32 buttons)
{
    BRect cbRekt(fieldRect.LeftTop(), BSize(fieldRect.Height(), fieldRect.Height()));
    bool clickOnCb = cbRekt.InsetByCopy(2, 2).OffsetByCopy(2, 0).Contains(point);
    if(clickOnCb && (buttons & B_PRIMARY_MOUSE_BUTTON)) {
        int32 value = ((BCheckStringField*)field)->Value();
        if(value == B_CONTROL_ON)
            ((BCheckStringField*)field)->SetValue(B_CONTROL_OFF);
        else
            ((BCheckStringField*)field)->SetValue(B_CONTROL_ON);
    }
    else if(clickOnCb && (buttons & B_TERTIARY_MOUSE_BUTTON)) {
        ((BCheckStringField*)field)->SetValue(B_CONTROL_PARTIALLY_ON);
    }
    BStringColumn::MouseDown(parent, row, field, fieldRect, point, buttons);
}

bool BCheckStringColumn::AcceptsField(const BField* field) const
{
    return static_cast<bool>(dynamic_cast<const BCheckStringField*>(field));
}

float BCheckStringColumn::GetPreferredWidth(BField* field, BView* parent)
{
    float preferred = BStringColumn::GetPreferredWidth(field, parent);
    float checkbox = 0;

    BColumnListView* clv = dynamic_cast<BColumnListView*>(parent);
    BRow* row = NULL;
    int32 i = 0;
    while(i < clv->CountRows()) {
        if(clv->RowAt(i)->GetField(LogicalFieldNum()) == field) {
            row = clv->RowAt(i);
            break;
        }
        else
            i++;
    }
    if(row != NULL) {
        BRect fieldRect = clv->GetFieldRect(row, this);
        checkbox = fieldRect.Height();
    }
    else {
        checkbox = BRow().Height();
    }
    return preferred + checkbox * 2.5f;
}

// #pragma mark - BCheckStringItem

BCheckStringItem::BCheckStringItem(const char* text, int32 initialState)
: BStringItem(text),
  fValue(initialState)
{
}

void BCheckStringItem::DrawItem(BView* owner, BRect frame, bool complete)
{
    BRect checkBoxRekt(frame.InsetByCopy(2, 2));
    checkBoxRekt.right = checkBoxRekt.Height();

    uint32 checkBoxFlags = BControlLook::B_BLEND_FRAME;
    if(Value() == B_CONTROL_ON)
        checkBoxFlags |= BControlLook::B_ACTIVATED;
    else if(Value() == B_CONTROL_PARTIALLY_ON)
        checkBoxFlags |= BControlLook::B_PARTIALLY_ACTIVATED;

    be_control_look->DrawCheckBox(owner, checkBoxRekt, frame, ui_color(B_PANEL_BACKGROUND_COLOR), checkBoxFlags);

    owner->SetHighUIColor(B_LIST_ITEM_TEXT_COLOR);
    frame.left += frame.Height();
    BStringItem::DrawItem(owner, frame, complete);
}

void BCheckStringItem::SetValue(int32 value)
{
    fValue = value;
}

int32 BCheckStringItem::Value()
{
    return fValue;
}

// #pragma mark - BListViewEx

BListViewEx::BListViewEx(BRect frame, const char* name, list_view_type type,
    uint32 resizeMask, uint32 flags, BPopUpMenu* pmenu, BPopUpMenu* imenu)
: BListView(frame, name, type, resizeMask, flags),
  parentmenu(pmenu),
  itemmenu(imenu),
  isEnabled(true)
{
}

BListViewEx::BListViewEx(const char* name, list_view_type type,
    uint32 flags, BPopUpMenu* pmenu, BPopUpMenu* imenu)
: BListView(name, type, flags),
  parentmenu(pmenu),
  itemmenu(imenu),
  isEnabled(true)
{
}

BListViewEx::BListViewEx(list_view_type type, BPopUpMenu* pmenu, BPopUpMenu* imenu)
: BListView(type),
  parentmenu(pmenu),
  itemmenu(imenu),
  isEnabled(true)
{
}

BListViewEx::BListViewEx(BMessage* data, BPopUpMenu* pmenu, BPopUpMenu* imenu)
: BListView(data),
  parentmenu(pmenu),
  itemmenu(imenu),
  isEnabled(true)
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

void BListViewEx::KeyDown(const char* bytes, int32 numBytes)
{
    if(!IsEnabled())
        return BListView::KeyDown(bytes, numBytes);

    if(numBytes == 1 && bytes[0] == B_DELETE) {
        BListItem* item = ItemAt(CurrentSelection());
        if(IsFocus() && item && IsItemSelected(IndexOf(item))) {
            BMessage request(LVX_DELETE_REQUESTED);
            request.AddPointer("origin", this);
            request.AddPointer("item_to_delete", item);
            Window()->PostMessage(&request);
        }
    }
    else
        BListView::KeyDown(bytes, numBytes);
}

void BListViewEx::MouseDown(BPoint where)
{
    uint32 buttons;
    GetMouse(&where, &buttons, true);
    if(buttons & B_SECONDARY_MOUSE_BUTTON) {
        BPoint spawnPoint(where);
        Parent()->ConvertToScreen(&spawnPoint);

        if(IndexOf(where) < 0) {
            if(IsEnabled() && parentmenu)
                parentmenu->Go(spawnPoint, true, true, true);
        }
        else {
            if(IsEnabled() && itemmenu) // we should somehow do not do this blindly
                itemmenu->Go(spawnPoint, true, true, true);
        }
    }

    BListView::MouseDown(where);

}

void BListViewEx::MessageReceived(BMessage* msg)
{
    BListView::MessageReceived(msg);
}

void BListViewEx::Draw(BRect updateRect)
{
    BListView::Draw(updateRect);

    SetViewUIColor(IsEnabled() ? B_LIST_BACKGROUND_COLOR : B_PANEL_BACKGROUND_COLOR);

    Invalidate();
}

void BListViewEx::SetEnabled(bool enabled)
{
    isEnabled = enabled;
}

bool BListViewEx::IsEnabled()
{
    return isEnabled;
}
