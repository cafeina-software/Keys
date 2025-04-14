/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __LIST_VIEW_EX_H_
#define __LIST_VIEW_EX_H_

#include <CheckBox.h>
#include <PopUpMenu.h>
#include <SupportDefs.h>
#include <ListView.h>
#include <private/interface/ColumnListView.h>
#include <private/interface/ColumnTypes.h>

#define LVX_DELETE_REQUESTED 'xdel'

class BCheckStringField : public BStringField
{
public:
    BCheckStringField(const char* text, int32 initialState = B_CONTROL_OFF);
    void SetValue(int32 value);
    int32 Value();
private:
    int32 fValue;
};

class BCheckStringColumn : public BStringColumn
{
public:
                    BCheckStringColumn(const char* title, float width,
                        float minWidth, float maxWidth, uint32 truncate,
                        alignment align = B_ALIGN_LEFT);
    virtual void    DrawField(BField* field, BRect rect, BView* targetView);
    virtual void    MouseDown(BColumnListView* parent, BRow* row,
                        BField* field, BRect fieldRect,
                        BPoint point, uint32 buttons);
    virtual float   GetPreferredWidth(BField* field, BView* parent);
    virtual bool    AcceptsField(const BField* field) const;
};

class BCheckStringItem : public BStringItem
{
public:
                    BCheckStringItem(const char* text,
                        int32 initialState = B_CONTROL_OFF);
    virtual void    DrawItem(BView* owner, BRect frame, bool complete = false);
            void    SetValue(int32 value);
            int32   Value();
private:
    int32           fValue;
};

class BListViewEx : public BListView
{
public:
                    BListViewEx(BRect frame, const char* name,
                        list_view_type type = B_SINGLE_SELECTION_LIST,
                        uint32 resizeMask = B_FOLLOW_LEFT_TOP,
                        uint32 flags = B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE,
                        BPopUpMenu* pmenu = nullptr, BPopUpMenu* imenu = nullptr);
                    BListViewEx(const char* name,
                        list_view_type type = B_SINGLE_SELECTION_LIST,
                        uint32 flags = B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE,
                        BPopUpMenu* pmenu = nullptr, BPopUpMenu* imenu = nullptr);
                    BListViewEx(list_view_type type = B_SINGLE_SELECTION_LIST,
                        BPopUpMenu* pmenu = nullptr, BPopUpMenu* imenu = nullptr);
                    BListViewEx(BMessage* data,
                        BPopUpMenu* pmenu = nullptr, BPopUpMenu* imenu = nullptr);

    virtual	       ~BListViewEx();
    virtual void    AttachedToWindow();
    virtual void    KeyDown(const char* bytes, int32 numBytes);
    virtual void    MouseDown(BPoint where);
    virtual void    MessageReceived(BMessage* msg);
    virtual void    Draw(BRect updateRect);

            void    SetEnabled(bool enabled);
            bool    IsEnabled();
private:
    BPopUpMenu     *parentmenu, *itemmenu;
    bool            isEnabled;
};

#endif /* __LIST_VIEW_EX_H_ */
