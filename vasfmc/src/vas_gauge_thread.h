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

/*! \file    vas_gauge_thread.h
    \author  Martin Böhme
*/

#ifndef __VAS_GAUGE_THREAD_H__
#define __VAS_GAUGE_THREAD_H__

#include <QMutex>
#include <QString>
#include <QThread>
#include <QWaitCondition>

#include <windows.h>
#include <iostream>

class QEvent;
class QApplication;
class FMCCDUStyleBase;
class FMCFCUStyleBase;
class FMCConsole;
class FMCNavdisplay;
class FMCPFD;
class FMCECAM;
class VasWidget;

class VasGaugeThread : public QThread
// This thread runs all of the vasFMC code. Communication between the main
// thread of MSFS (which calls the gauge callbacks) and this thread is
// achieved mainly through queued signals and events (to forward mouse and
// keyboard input to vasFMC) and bitmaps with the gauge contents (which are
// passed back to MSFS via VasWidget).
{
    Q_OBJECT

public:
    VasGaugeThread(const QString& style);

    virtual void run();
        // Main gauge routine (QThread override)

    void waitUntilInitialized();
        // Blocks and does not return until vasFMC has finished initializing

    FMCConsole *fmcConsole();
        // Returns a pointer to the console

    FMCCDUStyleBase *fmcCDUStyleBase();
    // Returns a pointer to the MCDU widget

    FMCFCUStyleBase *fmcFCUStyleBase();
        // Returns a pointer to the FCU widget

    FMCPFD *fmcPFD();
        // Returns a pointer to the PFD widget

    FMCNavdisplay *fmcNavdisplay();
        // Returns a pointer to the ND widget

    FMCECAM *fmcUECAM();
        // Returns a pointer to the UECAM widget

    HANDLE hThread();
        // Returns a Win32 handle for this thread

    static bool canStart();
        // Returns true if it is permissible to start the vasFMC thread by
        // calling start(). If canStart() returns false, the vasFMC thread of
        // the previously loaded aircraft is still running. In this case, do
        // not start a new thread; call canStart() periodically until it
        // returns true, then start the new thread.
    
    static HMODULE GaugeDllModuleHandle();
        // Returns a module handle for the gauge DLL.

private:
    // Mutex for protecting m_initialized and m_condInitialized
    QMutex         m_mutex;

    // Wait condition for waiting until thread has been initialized
    QWaitCondition m_condInitialized;
    bool           m_initialized;

    HANDLE         m_hThread;
    QApplication   *m_pApp;
    FMCConsole     *m_pConsole;

    QString        m_style;
};

#endif // __VAS_GAUGE_THREAD_H__
