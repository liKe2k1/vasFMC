///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2005-2006 Martin Böhme
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
///////////////////////////////////////////////////////////////////////////////

// vasfmc_gauge_main.cpp

#include <QMouseEvent>
#include <QDir>
#include <QFile>

#include "vasfmc_gauge_main.h"

#include "defines.h"
#include "logger.h"
#include "vas_path.h"

#include "fmc_cdu.h"
#include "fmc_fcu.h"
#include "fmc_console.h"
#include "fmc_navdisplay.h"
#include "fmc_pfd.h"
#include "fmc_ecam.h"
#include "vas_gauge_thread.h"

#include <windows.h>

#include <fs9gauges.h>

namespace
// Anonymous namespace for local helper classes
{
class PfdGauge
{
public:
    static void Callback(PGAUGEHDR pgauge, SINT32 service_id, UINT32 extra_data);
    static BOOL MouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags);

private:
    static void ParseParameters(PGAUGEHDR pgauge);
    static bool PopupVisible();

    static uint s_idPopupPanel;
    static PGAUGEHDR s_pGaugeBuiltinPFD, s_pGaugePopupPFD;
};


/////////////////////////////////////////////////////////////////////////////

class NdGauge
{
public:
    enum Style { A_STYLE, 
                 B_STYLE,
                 G_STYLE
    };

    static void Callback(PGAUGEHDR pgauge, SINT32 service_id, UINT32 extra_data);
    static BOOL MouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags);

private:
    static void ParseParameters(PGAUGEHDR pgauge);
    static bool PopupVisible();

    static uint s_idPopupPanel;
    static PGAUGEHDR s_pGaugeBuiltinND, s_pGaugePopupND;
    static Style s_style;
    static int s_connectCount;
};

/////////////////////////////////////////////////////////////////////////////

class UecamGauge
{
public:
    static void Callback(PGAUGEHDR pgauge, SINT32 service_id, UINT32 extra_data);
    static BOOL MouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags);

private:
    static void ParseParameters(PGAUGEHDR pgauge);
    static bool PopupVisible();

    static uint s_idPopupPanel;
    static PGAUGEHDR s_pGaugeBuiltinUECAM, s_pGaugePopupUECAM;
};

/////////////////////////////////////////////////////////////////////////////

class McduGauge
{
public:
    static void Callback(PGAUGEHDR pgauge, SINT32 service_id, UINT32 extra_data);
    static BOOL MouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags);

    static void toggleKeyboard();

private:
    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam,
                                         LPARAM lParam);

    static HHOOK s_hhookKeyboard;
    static bool s_fMcduKbdActive;
};

class FcuGauge
{
public:
    static void Callback(PGAUGEHDR pgauge, SINT32 service_id, UINT32 extra_data);
    static BOOL MouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags);

private:
};
}

/////////////////////////////////////////////////////////////////////////////

static VasGaugeThread *s_pThread=NULL;

static QKeyEvent GenerateKeyEvent(WPARAM wParam, LPARAM lParam);
static void GenerateMouseEvent(PPIXPOINT arg1, FLAGS32 mouse_flags,
                               VasWidget *pWidget, int widthGauge, int heightGauge);

//////////////////////////////////////////////////////////////////////////////
// PFD

uint PfdGauge::s_idPopupPanel=0;
PGAUGEHDR PfdGauge::s_pGaugeBuiltinPFD=0, PfdGauge::s_pGaugePopupPFD=0;

extern "C" void FSAPI PfdCallback(PGAUGEHDR pgauge, SINT32 service_id, UINT32 extra_data)
{
    PfdGauge::Callback(pgauge, service_id, extra_data);
}

/////////////////////////////////////////////////////////////////////////////

extern "C" BOOL FSAPI PfdMouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags)
{
    return PfdGauge::MouseFunction(arg1, mouse_flags);
}

/////////////////////////////////////////////////////////////////////////////

void PfdGauge::Callback(PGAUGEHDR pgauge, SINT32 service_id, UINT32)
{
    switch(service_id)
    {
        case PANEL_SERVICE_PRE_INITIALIZE:
            // Parse the gauge parameters
            ParseParameters(pgauge);
            break;

        case PANEL_SERVICE_PRE_DRAW:
        {
            PELEMENT_STATIC_IMAGE pelement = (PELEMENT_STATIC_IMAGE)
                                             pgauge->elements_list[0];

            if(pelement && s_pThread)
            {
                HDC hdc = pelement->hdc;
                PIXPOINT dim = pelement->image_data.final->dim;

                if(hdc)
                {                     
                    // If the popup is visible, the builtin gauge should not
                    // display any content
                    if((PopupVisible() && pgauge==s_pGaugeBuiltinPFD))
                    {
                        SelectObject(hdc, GetStockObject(BLACK_BRUSH));
                        SelectObject(hdc, GetStockObject(NULL_PEN));
                        Rectangle(hdc, 0, 0, dim.x, dim.y);
                        SET_OFF_SCREEN(pelement);
                    }
                    else
                    {
#if 0
                        PIMAGE pimg=pelement->image_data.final;
                        QString str;
                        str.sprintf("format %d", pimg->format);
                        Logger::log(str);

                        // format 1555
                        ((UINT16 *)pimg->image)[0]=0x801f;
                        ((UINT16 *)pimg->image)[1]=0x83e0;
                        for(int j=2; j<pimg->dim.x*pimg->dim.y; j++)
                            ((UINT16 *)pimg->image)[j]=0xfc00;
#endif

                        // Display gauge normally
                        s_pThread->fmcPFD()->paintBitmapToGauge(pelement);
                    }
                }
            }
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

BOOL PfdGauge::MouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags)
{
    PGAUGEHDR pGaugeHdr=GAUGEHDR_FOR_MOUSE_CALLBACK(arg1);

    if(s_pThread==NULL)
        return false;

    // If left mouse button clicked, toggle the state of the PFD popup
    if(mouse_flags==MOUSE_LEFTSINGLE)
    {
        if (s_idPopupPanel != 0)
        {
            panel_window_toggle(s_idPopupPanel);
            return false;
        }
    }

    // Disable mouse input on the builtin PFD while the popup is visible
    if(PopupVisible() && pGaugeHdr == s_pGaugeBuiltinPFD)
        return false;

    // Generate a mouse event
    GenerateMouseEvent(arg1, mouse_flags, s_pThread->fmcPFD(), PFD_WIDTH, PFD_HEIGHT);

    return false;
}

/////////////////////////////////////////////////////////////////////////////

void PfdGauge::ParseParameters(PGAUGEHDR pgauge)
{
    QString     strParams(pgauge->parameters);
    QStringList paramList=strParams.trimmed().split(" ", QString::SkipEmptyParts);
    bool        idOK;

    // Is this the builtin PFD?
    if(paramList.size()>=1 && paramList[0]=="builtin")
    {
        s_pGaugeBuiltinPFD=pgauge;

        // Get ID of popup panel
        if(paramList.size()>=2)
        {
            if(paramList[1].startsWith("id"))
            {
                s_idPopupPanel=paramList[1].mid(2).toUInt(&idOK);
            }
            else
            {
                idOK=false;
            }

            if(!idOK) s_idPopupPanel=0;
        }
    }
    else
    {
        s_pGaugePopupPFD=pgauge;
    }
}

/////////////////////////////////////////////////////////////////////////////

bool PfdGauge::PopupVisible()
{
    return s_idPopupPanel!=0 && is_panel_window_visible_ident(s_idPopupPanel);
}

//////////////////////////////////////////////////////////////////////////////
// ND

uint NdGauge::s_idPopupPanel=0;
PGAUGEHDR NdGauge::s_pGaugeBuiltinND=0, NdGauge::s_pGaugePopupND=0;
NdGauge::Style NdGauge::s_style = NdGauge::A_STYLE;
int NdGauge::s_connectCount=0;

/////////////////////////////////////////////////////////////////////////////

extern "C" void FSAPI NdCallback(PGAUGEHDR pgauge, SINT32 service_id, UINT32 extra_data)
{
    NdGauge::Callback(pgauge, service_id, extra_data);
}

/////////////////////////////////////////////////////////////////////////////

extern "C" BOOL FSAPI NdMouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags)
{
    return NdGauge::MouseFunction(arg1, mouse_flags);
}

/////////////////////////////////////////////////////////////////////////////

void NdGauge::Callback(PGAUGEHDR pgauge, SINT32 service_id,
                                    UINT32 )
{
    switch(service_id)
    {
        case PANEL_SERVICE_CONNECT_TO_WINDOW:
            // Is this the first "connect" call?
            if(s_connectCount==0)
            {
                // Inform vasFMC about path to vasFMC directory
                VasPath::setPath(QDir::currentPath()+ "/vasfmc");
            }

            s_connectCount++;
            break;            

        case PANEL_SERVICE_DISCONNECT:
            s_connectCount--;

            Logger::log(QString("NdGauge::Callback: PANEL_SERVICE_DISCONNECT: count=%1").arg(s_connectCount));

            // Is this the last "disconnect" call?
            if(s_connectCount==0)
            {
                // Stop the vasFMC thread
                if(s_pThread!=NULL)
                {
                    Logger::log("NdGauge::Callback: PANEL_SERVICE_DISCONNECT: stopping thread");
                    s_pThread->quit();
                    s_pThread=NULL;
                }
            }
            break;
            
        case PANEL_SERVICE_PRE_INITIALIZE:
            ParseParameters(pgauge);
            break;

        case PANEL_SERVICE_PRE_UPDATE:
            // Start thread if it hasn't been started yet
            if(s_pThread==NULL && VasGaugeThread::canStart())
            {
                Logger::log("NdGauge::Callback: PANEL_SERVICE_PRE_UPDATE: starting thread");

                if(s_style==G_STYLE)
                    s_pThread=new VasGaugeThread("G");
                if(s_style==B_STYLE)
                    s_pThread=new VasGaugeThread("B");
                else 
                    s_pThread=new VasGaugeThread("A");

                // Wait for thread to be started up
                s_pThread->start();
                s_pThread->waitUntilInitialized();

                Logger::log("NdGauge::Callback: PANEL_SERVICE_PRE_UPDATE: thread started");
            }
            break;
            
        case PANEL_SERVICE_PRE_DRAW:
        {
            PELEMENT_STATIC_IMAGE pelement = (PELEMENT_STATIC_IMAGE)
                                             pgauge->elements_list[0];

            if(pelement && s_pThread)
            {
                HDC hdc = pelement->hdc;
                PIXPOINT dim = pelement->image_data.final->dim;

                if(hdc)
                {                     
                    // If the popup is visible, the builtin gauge should not
                    // display any content
                    if((PopupVisible() && pgauge==s_pGaugeBuiltinND))
                    {
                        SelectObject(hdc, GetStockObject(BLACK_BRUSH));
                        SelectObject(hdc, GetStockObject(NULL_PEN));
                        Rectangle(hdc, 0, 0, dim.x, dim.y);
                        SET_OFF_SCREEN(pelement);
                    }
                    else
                    {
                        // Display gauge normally
                        s_pThread->fmcNavdisplay()->paintBitmapToGauge(pelement);
                    }
                }
            }
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

BOOL NdGauge::MouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags)
{
    PGAUGEHDR pGaugeHdr=GAUGEHDR_FOR_MOUSE_CALLBACK(arg1);

    if(s_pThread==NULL)
        return false;

    // If left mouse button clicked, toggle the state of the ND popup
    if(mouse_flags==MOUSE_LEFTSINGLE)
    {
        if (s_idPopupPanel != 0)
        {
            panel_window_toggle(s_idPopupPanel);
            return false;
        }
    }

    // Disable mouse input on the builtin ND while the popup is visible
    if(PopupVisible() && pGaugeHdr==s_pGaugeBuiltinND)
        return false;

    // Generate a mouse event
    GenerateMouseEvent(arg1, mouse_flags, s_pThread->fmcNavdisplay(), ND_WIDTH, ND_HEIGHT);

    return false;
}

/////////////////////////////////////////////////////////////////////////////

void NdGauge::ParseParameters(PGAUGEHDR pgauge)
{
    QString     strParams(pgauge->parameters);
    QStringList paramList=strParams.trimmed().split(" ", QString::SkipEmptyParts);
    bool        idOK;

    // Is this the builtin ND?
    if(paramList.size()>=1 && paramList[0]=="builtin")
    {
        s_pGaugeBuiltinND=pgauge;

        // Get ID of popup panel
        if(paramList.size()>=2)
        {
            if(paramList[1].startsWith("id"))
                s_idPopupPanel=paramList[1].mid(2).toUInt(&idOK);
            else
                idOK=false;
            if(!idOK)
                s_idPopupPanel=0;
        }

        // Get desired gauge style
        if(paramList.size()>=3)
        {
            if(paramList[2]=="g_style")
                s_style=G_STYLE;
            else if(paramList[2]=="b_style")
                s_style=B_STYLE;
            else
                s_style=A_STYLE;
        }
    }
    else
    {
        s_pGaugePopupND=pgauge;
    }
}

/////////////////////////////////////////////////////////////////////////////

bool NdGauge::PopupVisible()
{
    return s_idPopupPanel!=0 && is_panel_window_visible_ident(s_idPopupPanel);
}

//////////////////////////////////////////////////////////////////////////////
// UECAM

uint UecamGauge::s_idPopupPanel=0;
PGAUGEHDR UecamGauge::s_pGaugeBuiltinUECAM=0, UecamGauge::s_pGaugePopupUECAM=0;

extern "C" void FSAPI UecamCallback(PGAUGEHDR pgauge, SINT32 service_id,
                                    UINT32 extra_data)
{
    UecamGauge::Callback(pgauge, service_id, extra_data);
}

/////////////////////////////////////////////////////////////////////////////

extern "C" BOOL FSAPI UecamMouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags)
{
    return UecamGauge::MouseFunction(arg1, mouse_flags);
}

/////////////////////////////////////////////////////////////////////////////

void UecamGauge::Callback(PGAUGEHDR pgauge, SINT32 service_id,
                                       UINT32 )
{
    switch(service_id)
    {
        case PANEL_SERVICE_PRE_INITIALIZE:
            // Parse the gauge parameters
            ParseParameters(pgauge);
            break;

        case PANEL_SERVICE_PRE_DRAW:
        {
            PELEMENT_STATIC_IMAGE pelement = (PELEMENT_STATIC_IMAGE)
                                             pgauge->elements_list[0];

            if(pelement && s_pThread)
            {
                HDC hdc = pelement->hdc;
                PIXPOINT dim = pelement->image_data.final->dim;

                if(hdc)
                {                     
                    // If the popup is visible, the builtin gauge should not
                    // display any content
                    if((PopupVisible() && pgauge==s_pGaugeBuiltinUECAM))
                    {
                        SelectObject(hdc, GetStockObject(BLACK_BRUSH));
                        SelectObject(hdc, GetStockObject(NULL_PEN));
                        Rectangle(hdc, 0, 0, dim.x, dim.y);
                        SET_OFF_SCREEN(pelement);
                    }
                    else
                    {
                        // Display gauge normally
                        s_pThread->fmcUECAM()->paintBitmapToGauge(pelement);
                    }
                }
            }
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

BOOL UecamGauge::MouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags)
{
    PGAUGEHDR pGaugeHdr=GAUGEHDR_FOR_MOUSE_CALLBACK(arg1);

    if(s_pThread==NULL)
        return false;

    // If left mouse button clicked, toggle the state of the UECAM popup
    if(mouse_flags==MOUSE_LEFTSINGLE)
    {
        if (s_idPopupPanel != 0)
        {
            panel_window_toggle(s_idPopupPanel);
            return false;
        }
    }

    // Disable mouse input on the builtin UECAM while the popup is visible
    if(PopupVisible() && pGaugeHdr==s_pGaugeBuiltinUECAM)
        return false;

    // Generate a mouse event
    GenerateMouseEvent(arg1, mouse_flags, s_pThread->fmcUECAM(), UECAM_WIDTH, UECAM_HEIGHT);

    return false;
}

/////////////////////////////////////////////////////////////////////////////

void UecamGauge::ParseParameters(PGAUGEHDR pgauge)
{
    QString     strParams(pgauge->parameters);
    QStringList paramList=strParams.trimmed().split(" ", QString::SkipEmptyParts);
    bool        idOK;

    // Is this the builtin UECAM?
    if(paramList.size()>=1 && paramList[0]=="builtin")
    {
        s_pGaugeBuiltinUECAM=pgauge;

        // Get ID of popup panel
        if(paramList.size()>=2)
        {
            if(paramList[1].startsWith("id"))
                s_idPopupPanel=paramList[1].mid(2).toUInt(&idOK);
            else
                idOK=false;
            if(!idOK)
                s_idPopupPanel=0;
        }
    }
    else
        s_pGaugePopupUECAM=pgauge;
}

/////////////////////////////////////////////////////////////////////////////

bool UecamGauge::PopupVisible()
{
    return s_idPopupPanel!=0 && is_panel_window_visible_ident(s_idPopupPanel);
}


//////////////////////////////////////////////////////////////////////////////
// MCDU

HHOOK McduGauge::s_hhookKeyboard=NULL;
bool McduGauge::s_fMcduKbdActive=false;

extern "C" void FSAPI McduCallback(PGAUGEHDR pgauge, SINT32 service_id,
                                   UINT32 extra_data)
{
    McduGauge::Callback(pgauge, service_id, extra_data);
}

/////////////////////////////////////////////////////////////////////////////

extern "C" BOOL FSAPI McduMouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags)
{
    return McduGauge::MouseFunction(arg1, mouse_flags);
}

/////////////////////////////////////////////////////////////////////////////

void toggleMcduKeyboard()
{
    Logger::log("McduGauge:toggleMcduKeyboard:");
    McduGauge::toggleKeyboard();
}

/////////////////////////////////////////////////////////////////////////////

void McduGauge::Callback(PGAUGEHDR pgauge, SINT32 service_id, UINT32 )
{
    switch(service_id)
    {
        case PANEL_SERVICE_CONNECT_TO_WINDOW:
            // Set keyboard hook
            if(s_hhookKeyboard==NULL)
            {
                s_hhookKeyboard=SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, 0, GetCurrentThreadId());
            }
            break;

        case PANEL_SERVICE_DISCONNECT:
            // Unhook keyboard hook
            if(s_hhookKeyboard!=NULL)
            {
                UnhookWindowsHookEx(s_hhookKeyboard);
                s_hhookKeyboard=NULL;
            }
            break;

        case PANEL_SERVICE_PRE_DRAW:
        {
            PELEMENT_STATIC_IMAGE pelement = (PELEMENT_STATIC_IMAGE)
                                             pgauge->elements_list[0];
                 
            if(pelement && s_pThread)
            {
                s_pThread->fmcCDUStyleBase()->paintBitmapToGauge(pelement);
            }
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

BOOL McduGauge::MouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags)
{
    if(s_pThread==NULL)
        return false;

    GenerateMouseEvent(arg1, mouse_flags, s_pThread->fmcCDUStyleBase(), MCDU_WIDTH, MCDU_HEIGHT);

    return false;
}

/////////////////////////////////////////////////////////////////////////////

void McduGauge::toggleKeyboard()
{
    s_fMcduKbdActive=!s_fMcduKbdActive;

    Logger::log(QString("McduGauge:toggleKeyboard: new state = %1").arg(s_fMcduKbdActive));
    
    QMetaObject::invokeMethod(s_pThread->fmcCDUStyleBase(),
                              "slotShowKbdActiveIndicator", Qt::QueuedConnection,
                              Q_ARG(bool, s_fMcduKbdActive));
}

// private:

/////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK McduGauge::KeyboardProc(int nCode,
                                                      WPARAM wParam, LPARAM lParam)
{
    if(nCode>=0)
    {
        QKeyEvent event=GenerateKeyEvent(wParam, lParam);

        // Is the MCDU keyboard active?
        if(s_fMcduKbdActive)
        {
            // Synthesize key event
            QCoreApplication::postEvent(s_pThread->fmcCDUStyleBase(), new QKeyEvent(event));
            return -1;
        }
    }

    return CallNextHookEx(s_hhookKeyboard, nCode, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////////
// FCU

extern "C" void FSAPI FcuCallback(PGAUGEHDR pgauge, SINT32 service_id,
                                  UINT32 extra_data)
{
    FcuGauge::Callback(pgauge, service_id, extra_data);
}

/////////////////////////////////////////////////////////////////////////////

extern "C" BOOL FSAPI FcuMouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags)
{
    return FcuGauge::MouseFunction(arg1, mouse_flags);
}

/////////////////////////////////////////////////////////////////////////////

void FcuGauge::Callback(PGAUGEHDR pgauge, SINT32 service_id,
                                     UINT32 )
{
    switch(service_id)
    {
        case PANEL_SERVICE_CONNECT_TO_WINDOW:
            break;

        case PANEL_SERVICE_DISCONNECT:
            break;

        case PANEL_SERVICE_PRE_DRAW:
        {
            PELEMENT_STATIC_IMAGE pelement = (PELEMENT_STATIC_IMAGE)
                                             pgauge->elements_list[0];
                 
            if(pelement && s_pThread)
            {
                s_pThread->fmcFCUStyleBase()->paintBitmapToGauge(pelement);
            }
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

BOOL FcuGauge::MouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags)
{
    if(s_pThread==NULL)
        return false;

    GenerateMouseEvent(arg1, mouse_flags, s_pThread->fmcFCUStyleBase(), FCU_WIDTH, FCU_HEIGHT);

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Helpers

static int s_alphabetKeys[]=
{
Qt::Key_A, Qt::Key_B, Qt::Key_C, Qt::Key_D, Qt::Key_E, Qt::Key_F,
Qt::Key_G, Qt::Key_H, Qt::Key_I, Qt::Key_J, Qt::Key_K, Qt::Key_L,
Qt::Key_M, Qt::Key_N, Qt::Key_O, Qt::Key_P, Qt::Key_Q, Qt::Key_R,
Qt::Key_S, Qt::Key_T, Qt::Key_U, Qt::Key_V, Qt::Key_W, Qt::Key_X,
Qt::Key_Y, Qt::Key_Z
};

static int s_numberKeys[]=
{
Qt::Key_0, Qt::Key_1, Qt::Key_2, Qt::Key_3, Qt::Key_4, Qt::Key_5,
Qt::Key_6, Qt::Key_7, Qt::Key_8, Qt::Key_9
};

/////////////////////////////////////////////////////////////////////////////

static QKeyEvent GenerateKeyEvent(WPARAM wParam, LPARAM lParam)
{
    BYTE                  keyState[256];
    WCHAR                 aChars[16];
    int                   numChars;
    int                   key;
    QChar                 ch;
    QKeyEvent::Type       type;
    Qt::KeyboardModifiers modifiers;
    QString               text;

    // Get keyboard state and translate key to Unicode
    GetKeyboardState(keyState);
    numChars=ToUnicode(wParam, (lParam>>16)&0xff, keyState, aChars, 16, 0);

    // Get type of key event
    if(lParam & 0x80000000)
        type=QEvent::KeyRelease;
    else
        type=QEvent::KeyPress;

    // Get modifiers
    modifiers=Qt::NoModifier;
    if(keyState[VK_SHIFT] & 0x80)
        modifiers|=Qt::ShiftModifier;
    if(keyState[VK_CONTROL] & 0x80)
        modifiers|=Qt::ControlModifier;
    if(keyState[VK_MENU] & 0x80)
        modifiers|=Qt::AltModifier;

    // Get key code and character
    ch=L'\0';
    key=Qt::Key_unknown;
    switch(wParam)
    {
        case VK_F1:
            key=Qt::Key_F1;
            break;
        case VK_F2:
            key=Qt::Key_F2;
            break;
        case VK_F3:
            key=Qt::Key_F3;
            break;
        case VK_F4:
            key=Qt::Key_F4;
            break;
        case VK_F5:
            key=Qt::Key_F5;
            break;
        case VK_F6:
            key=Qt::Key_F6;
            break;
        case VK_F7:
            key=Qt::Key_F7;
            break;
        case VK_F8:
            key=Qt::Key_F8;
            break;
        case VK_F9:
            key=Qt::Key_F9;
            break;
        case VK_F10:
            key=Qt::Key_F10;
            break;
        case VK_F11:
            key=Qt::Key_F11;
            break;
        case VK_F12:
            key=Qt::Key_F12;
            break;
        case VK_LEFT:
            key=Qt::Key_Left;
            break;
        case VK_RIGHT:
            key=Qt::Key_Right;
            break;
        case VK_UP:
            key=Qt::Key_Up;
            break;
        case VK_DOWN:
            key=Qt::Key_Down;
            break;
        case VK_PRIOR:
            key=Qt::Key_PageUp;
            break;
        case VK_NEXT:
            key=Qt::Key_PageDown;
            break;
        case VK_BACK:
            key=Qt::Key_Backspace;
            break;
        case VK_INSERT:
            key=Qt::Key_Insert;
            break;
        case VK_HOME:
            key=Qt::Key_Home;
            break;
        case VK_END:
            key=Qt::Key_End;
            break;
        default:
            if(numChars==1)
            {
                ch=QChar(aChars[0]);
                if(ch.toLower()>=L'a' && ch.toLower()<='z')
                    key=s_alphabetKeys[ch.toLower().unicode()-'a'];
                else if(ch>=L'0' && ch<=L'9')
                    key=s_numberKeys[ch.unicode()-'0'];
                else if(ch==L'/')
                    key=Qt::Key_Slash;
                else if(ch==L' ')
                    key=Qt::Key_Space;
                else if(ch==L'.')
                    key=Qt::Key_Period;
                else if(ch==L'*')
                    key=Qt::Key_Asterisk;
                else if(ch==L'+')
                    key=Qt::Key_Plus;
                else if(ch==L'-')
                    key=Qt::Key_Minus;
            }
            break;
    }

    if(ch==L'\0')
        text="";
    else
        text=ch;

    return QKeyEvent(type, key, modifiers, text);
}

/////////////////////////////////////////////////////////////////////////////

static void GenerateMouseEvent(PPIXPOINT arg1, FLAGS32 mouse_flags,
                               VasWidget *pWidget, int widthGauge, int heightGauge)
{
    PGAUGEHDR   pGaugeHdr=GAUGEHDR_FOR_MOUSE_CALLBACK(arg1);
    PMOUSERECT  pMouseRect=PMOUSERECT_FOR_MOUSE_CALLBACK(arg1);
    QPoint      pt;
    QEvent      *pEvent=0;

    pt.setX((arg1->x+pMouseRect->relative_box.x)*widthGauge /
            pGaugeHdr->size.x);
    pt.setY((arg1->y+pMouseRect->relative_box.y)*heightGauge /
            pGaugeHdr->size.y);

    switch(mouse_flags)
    {
        case MOUSE_LEFTSINGLE:
            pEvent=new QMouseEvent(QEvent::MouseButtonPress, pt, 
                                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            break;
        case MOUSE_RIGHTSINGLE:
            pEvent=new QMouseEvent(QEvent::MouseButtonPress, pt, 
                                   Qt::RightButton, Qt::RightButton, Qt::NoModifier);
            break;
        case MOUSE_LEFTRELEASE:
            pEvent=new QMouseEvent(QEvent::MouseButtonRelease, pt,
                                   Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            break;
        case MOUSE_RIGHTRELEASE:
            pEvent=new QMouseEvent(QEvent::MouseButtonRelease, pt,
                                   Qt::RightButton, Qt::NoButton, Qt::NoModifier);
            break;
        case MOUSE_WHEEL_UP:
            pEvent=new QWheelEvent(pt, 1, Qt::NoButton, Qt::NoModifier);
            break;
        case MOUSE_WHEEL_DOWN:
            pEvent=new QWheelEvent(pt, -1, Qt::NoButton, Qt::NoModifier);
            break;
    }

    if(pEvent!=0)
        QCoreApplication::postEvent(pWidget, pEvent);
}
