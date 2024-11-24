/*
 * Copyright 2024, cafeina <cafeina@world>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include <Catalog.h>
#include <KeyStore.h>
#include <string>
#include "DataViewerDialogBox.h"
#include "../data/KeystoreImp.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Data viewer dialog box"

DataViewerDialogBox::DataViewerDialogBox(BWindow* parent, BRect frame,
    KeystoreImp* imp, const char* keyring, const char* keyid, const char* secid)
: BWindow(frame, B_TRANSLATE("Data viewer: <no key>"), B_FLOATING_WINDOW,
    B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS | B_CLOSE_ON_ESCAPE),
  fImp(imp),
  fKeyringName(keyring),
  fKeyId(keyid),
  fKeySecondaryId(secid)
{
    BString title(B_TRANSLATE("Data viewer: "));
    title.Append(fKeyId);
    SetTitle(title.String());

    rgb_color viewColor = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
    rgb_color textColor = ui_color(B_DOCUMENT_TEXT_COLOR);

    tcIdentifier = new BTextControl("tc_id", B_TRANSLATE("Identifier"), "", 0);
    tcIdentifier->TextView()->MakeEditable(false);
    tcIdentifier->TextView()->SetFontAndColor(be_plain_font, B_FONT_ALL, &textColor);
    tcIdentifier->TextView()->SetLowColor(viewColor);
    tcIdentifier->TextView()->SetViewColor(viewColor);
    tcIdentifier->SetEnabled(false);

    tcSecIdentifier = new BTextControl("tc_2id", B_TRANSLATE("Sec. identifier"), "", 0);
    tcSecIdentifier->TextView()->MakeEditable(false);
    tcSecIdentifier->TextView()->SetFontAndColor(be_plain_font, B_FONT_ALL, &textColor);
    tcSecIdentifier->SetEnabled(false);

    tcType = new BTextControl("tc_type", B_TRANSLATE("Type"), "", 0);
    tcType->TextView()->MakeEditable(false);
    tcType->TextView()->SetFontAndColor(be_plain_font, B_FONT_ALL, &textColor);
    tcType->SetEnabled(false);

    tcPurpose = new BTextControl("tc_pur", B_TRANSLATE("Purpose"), "", 0);
    tcPurpose->TextView()->MakeEditable(false);
    tcPurpose->TextView()->SetFontAndColor(be_plain_font, B_FONT_ALL, &textColor);
    tcPurpose->SetEnabled(false);

    tcDCreated = new BTextControl("tc_date", B_TRANSLATE("Creation time (*)"), "", 0);
    tcDCreated->TextView()->MakeEditable(false);
    tcDCreated->TextView()->SetFontAndColor(be_plain_font, B_FONT_ALL, &textColor);
    tcDCreated->SetEnabled(false);

    tcOwner = new BTextControl("tc_owner", B_TRANSLATE("Owner (*)"), "", 0);
    tcOwner->TextView()->MakeEditable(false);
    tcOwner->TextView()->SetFontAndColor(be_plain_font, B_FONT_ALL, &textColor);
    tcOwner->SetEnabled(false);

    tvData = new BTextView("tv_data", be_plain_font, 0, B_WILL_DRAW);
    tvData->MakeEditable(false);
    tvData->MakeResizable(false);
    BScrollView *scDataScroll = new BScrollView("scv_hex", tvData, 0, false,
        true, B_FANCY_BORDER);

    BFont font(be_fixed_font);
    rgb_color c = {0, 3, 255, 255};
    tvHex = new BTextView("tv_hex", &font, &c, B_WILL_DRAW);
    tvHex->MakeEditable(false);
    tvHex->MakeResizable(false);
    BScrollView *scHexScroll = new BScrollView("scv_hex", tvHex, 0, false,
        true, B_FANCY_BORDER);

    _InitUIData();

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .SetInsets(0)
        .SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_TOP))
        .AddGroup(B_VERTICAL)
            .SetInsets(B_USE_WINDOW_INSETS)
            .AddGrid()
                .AddTextControl(tcIdentifier, 0, 0)
                .AddTextControl(tcSecIdentifier, 0, 1)
                .AddTextControl(tcType, 0, 2)
                .AddTextControl(tcPurpose, 0, 3)
                .AddTextControl(tcDCreated, 0, 4)
                .AddTextControl(tcOwner, 0, 5)
            .End()
            .Add(new BStringView("sv_not", B_TRANSLATE("(*) currently not implemented by the Key Storage API.")))
            .Add(new BSeparatorView())
            .AddGrid()
                .Add(new BStringView(NULL, B_TRANSLATE("Key data")), 0, 0)
                .Add(scDataScroll, 1, 0)
                .Add(new BStringView(NULL, B_TRANSLATE("Key data\n(hex dump)")), 0, 1)
                .Add(scHexScroll , 1, 1)
            .End()
        .End()
        .Add(new BSeparatorView())
        .AddGroup(B_HORIZONTAL)
            .SetInsets(B_USE_WINDOW_INSETS)
            .AddGlue()
            .Add(closeButton = new BButton("bt_close", B_TRANSLATE("Close"), new BMessage(DV_CLOSE)))
        .End()
    .End();

    FindView("sv_not")->SetHighColor({128, 128, 128, 255});
    CenterIn(parent->Frame());
}

void DataViewerDialogBox::MessageReceived(BMessage* msg)
{
    switch(msg->what)
    {
        case DV_CLOSE:
            Quit();
            break;
        default:
            BWindow::MessageReceived(msg);
            break;
    }
}

void DataViewerDialogBox::_InitUIData()
{
    KeyImp* key = fImp->KeyringByName(fKeyringName)->KeyByIdentifier(fKeyId, fKeySecondaryId);

    tcIdentifier->SetText(key->Identifier());
    tcSecIdentifier->SetText(key->SecondaryIdentifier());

    BString typedesc;
    BKeyType _type = key->Type();
    switch(_type)
    {
        case B_KEY_TYPE_GENERIC:
            _ProcessEnumData(_type, B_TRANSLATE("Generic"), &typedesc);
            tcType->SetText(typedesc);
            break;
        case B_KEY_TYPE_PASSWORD:
            _ProcessEnumData(_type, B_TRANSLATE("Password"), &typedesc);
            tcType->SetText(typedesc);
            break;
        case B_KEY_TYPE_CERTIFICATE:
            _ProcessEnumData(_type, B_TRANSLATE("Certificate"), &typedesc);
            tcType->SetText(typedesc);
            break;
        case B_KEY_TYPE_ANY:
        default:
            _ProcessEnumData(_type, "", &typedesc);
            tcType->SetText(typedesc);
            break;
    }

    BString purposestr;
    _ProcessPurpose(key->Purpose(), &purposestr);
    tcPurpose->SetText(purposestr);
    tcDCreated->SetText(std::to_string(key->Created()).c_str());
    tcOwner->SetText(key->Owner());

    if(_type == B_KEY_TYPE_PASSWORD) {
        BPasswordKey pwdkey;
        BKeyStore().GetKey(fKeyringName, B_KEY_TYPE_PASSWORD, fKeyId, fKeySecondaryId, false, pwdkey);
        tvData->SetText((const char*)pwdkey.Data());
        size_t inlength = pwdkey.DataLength();
        BString outdata;
        _ProcessHexData(pwdkey.Data(), inlength, &outdata);
        int32 curlen = tvHex->TextLength();
        tvHex->Delete(0, curlen);
        tvHex->SetText(outdata.String());
    }
    else {
        BKey key;
        BKeyStore().GetKey(fKeyringName, B_KEY_TYPE_GENERIC, fKeyId, fKeySecondaryId, false, key);
        tvData->SetText((const char*)key.Data());
        size_t inlength = key.DataLength();
        BString outdata;
        _ProcessHexData(key.Data(), inlength, &outdata);
        int32 curlen = tvHex->TextLength();
        tvHex->Delete(0, curlen);
        tvHex->SetText(outdata.String());
    }
}

void DataViewerDialogBox::_ProcessEnumData(const int type, const char* desc,
    BString* out)
{
    size_t typelen = 3;
    size_t desclen = strlen(desc);

    char* outstr = new char[typelen + 1 + desclen + 3];

    sprintf(outstr, "0x%.1x (%s)", type, desc);

    *out = outstr;
    delete[] outstr;
}

void DataViewerDialogBox::_ProcessHexData(const void* indata, const size_t inlength,
    BString* outdata)
{
    size_t outlen = inlength * 3;
    char* hexstring = new char[outlen + 1];

    char* current = hexstring;
    const unsigned char* data = ((const unsigned char*)indata);

    for(int i = 0, c = 0; i < static_cast<int>(inlength); i++) {
        sprintf(current, "%.2x", data[i]);
        current += 2;
        if(c < 7) {
            *current = ' ';
            current += 1;
            c++;
        }
        else {
            *current = '\n';
            current += 1;
            c = 0;
        }
    }
    *current = '\0';
    //size_t newlength = strlen(hexstring);
    *outdata = hexstring;
    delete[] hexstring;
}

void DataViewerDialogBox::_ProcessPurpose(const BKeyPurpose pur, BString* out)
{
    switch(pur)
    {
        case B_KEY_PURPOSE_GENERIC:
            _ProcessEnumData(pur, B_TRANSLATE("Generic"), out);
            break;
        case B_KEY_PURPOSE_KEYRING:
            _ProcessEnumData(pur, B_TRANSLATE("Keyring"), out);
            break;
        case B_KEY_PURPOSE_NETWORK:
            _ProcessEnumData(pur, B_TRANSLATE("Network"), out);
            break;
        case B_KEY_PURPOSE_WEB:
            _ProcessEnumData(pur, B_TRANSLATE("Web"), out);
            break;
        case B_KEY_PURPOSE_VOLUME:
            _ProcessEnumData(pur, B_TRANSLATE("Volume"), out);
            break;
        case B_KEY_PURPOSE_ANY:
        default:
            _ProcessEnumData(pur, "", out);
            break;
    }
}
