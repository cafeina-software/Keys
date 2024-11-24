/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef __LIST_VIEW_EX_H_
#define __LIST_VIEW_EX_H_

#include <PopUpMenu.h>
#include <SupportDefs.h>
#include <ListView.h>

#define LVX_DELETE_REQUESTED 'xdel'

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
private:
    BPopUpMenu     *parentmenu, *itemmenu;
};

#endif /* __LIST_VIEW_EX_H_ */
