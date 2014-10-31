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

/*! \file    vas_gauge_thread.cpp
    \author  Martin Böhme
*/

#include <signal.h>

#include <windows.h>
#include <psapi.h>

#include "vas_gauge_thread.h"

#include "assert.h"
#include "defines.h"
#include "logger.h"
#include "vas_path.h"

#include "fmc_cdu.h"
#include "fmc_fcu.h"
#include "fmc_console.h"
#include "fmc_navdisplay.h"
#include "fmc_pfd.h"
#include "fmc_ecam.h"

#include <QApplication>

// Dummy arguments for QApplication
static int argc=1;
static char *argv[1]={ "" };

// Is a thread currently running?
static bool s_running=false;

// Mutex for protecting s_running
static QMutex s_mutex;

// Dummy variable used to get module handle
static int s_dummy;

/////////////////////////////////////////////////////////////////////////////

// qt message logger
void myGaugeMessageOutput(QtMsgType type, const char *msg)
{
    switch (type) {
        case QtDebugMsg:
            Logger::log(QString("QtDebug: %1").arg(msg));
            break;
        case QtWarningMsg:
            Logger::log(QString("QtWarning: %1").arg(msg));
            break;
        case QtCriticalMsg:
            Logger::log(QString("QtCritical: %1").arg(msg));
            break;
        case QtFatalMsg:
            Logger::log(QString("QtFatal: %1").arg(msg));
            break;
        default:
            Logger::log(QString("Qtunknown: %1").arg(msg));
            break;             
    }
}

/////////////////////////////////////////////////////////////////////////////

static DWORD WINAPI DLLUnloadThreadProc(LPVOID lpParameter)
{
    VasGaugeThread *pThread=(VasGaugeThread *)lpParameter;
    HMODULE        hModule;

    // Wait for the vasFMC thread to terminate
    WaitForSingleObject(pThread->hThread(), INFINITE);

    // Close handle of thread
    CloseHandle(pThread->hThread());

    // Delete the thread object
    delete pThread;

    // Remember that thread is now terminated
    s_mutex.lock();
    s_running=false;
    s_mutex.unlock();

    // Get module handle for this DLL
    hModule=VasGaugeThread::GaugeDllModuleHandle();

    // Reduce DLL reference count to zero and exit this thread
    FreeLibraryAndExitThread(hModule, 0);
}

/////////////////////////////////////////////////////////////////////////////

static HMODULE GetModuleHandleByAddress(HANDLE hProc, LPVOID lpAddress)
// Alternative for GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
// ...) that works on older versions of Windows than XP. From the MSDN page
// for GetModuleHandleEx.
{
    HANDLE hHeap = GetProcessHeap();
    HMODULE * hmodules = NULL;
    DWORD cb = 0;
 
    for(;;)
    {
        DWORD cbNeeded = 0;
 
        if(!EnumProcessModules(hProc, hmodules, cb, &cbNeeded))
            return NULL;
 
        if(cb >= cbNeeded)
        {
            cb = cbNeeded;
            break;
        }
 
        PVOID p = HeapAlloc(hHeap, 0, cbNeeded);
        HeapFree(hHeap, 0, hmodules);
 
        if(p == NULL)
            return NULL;
 
        hmodules = (HMODULE *)p;
        cb = cbNeeded;
    }
 
    DWORD i = 0;
    DWORD ccModules = cb / sizeof(HMODULE);
    HMODULE hm = NULL;
 
    for(; i < ccModules; ++ i)
    {
        MODULEINFO modinfo;
 
        if(!GetModuleInformation(hProc, hmodules[i], &modinfo,
            sizeof(modinfo)))
            continue;
 
        if(lpAddress < modinfo.lpBaseOfDll)
            continue;
 
        if((ULONG_PTR)lpAddress >=
            ((ULONG_PTR)modinfo.lpBaseOfDll + modinfo.SizeOfImage))
            continue;
 
        hm = (HMODULE)modinfo.lpBaseOfDll;
        break;
    }

    if(i == ccModules && hm == NULL)
        SetLastError(ERROR_MOD_NOT_FOUND);
 
    HeapFree(hHeap, 0, hmodules);
 
    return hm;
}

/////////////////////////////////////////////////////////////////////////////

HMODULE ReferenceModuleByHandle(HMODULE hModule)
// Alternative way of incrementing the reference count on a DLL that doesn't
// use GetModuleHandleEx().
{
    TCHAR szModuleFileName[MAX_PATH + 1];

    if(GetModuleFileName(hModule, szModuleFileName,
        sizeof(szModuleFileName)/sizeof(TCHAR) - 1))
        return LoadLibrary(szModuleFileName);
    else
        return NULL;
}

/////////////////////////////////////////////////////////////////////////////

VasGaugeThread::VasGaugeThread(const QString& style)
    : m_initialized(false), m_style(style)
{
}

/////////////////////////////////////////////////////////////////////////////

void signalHandler(int signal_nr)
{
    Logger::log(QString("signalHandler: (%1) ----- SIGNAL HANDLER -----").arg(signal_nr));
    //TODOqApp->quit();
}

/////////////////////////////////////////////////////////////////////////////

void VasGaugeThread::run()
{
    //TODOHMODULE hModule;

    // Remember that a vasFMC thread is now running. Only one vasFMC thread
    // may be running at a time.
    s_mutex.lock();
    MYASSERT(!s_running);
    s_running=true;
    s_mutex.unlock();

    // Create the application object
    m_pApp=new QApplication(argc, argv);

    // Add the vasFMC directory to the list of paths used for loading DLLs.
    // This is needed to find the plugin DLLs that are used for loading
    // images.
    QCoreApplication::addLibraryPath(VasPath::getPath());

    // Initialize the logger
    Logger::getLogger()->setLogFile(VasPath::prependPath(CFG_LOGFILE_NAME));
    Logger::log("     ----- Thread Startup -----");

    // init Qt logging
    qInstallMsgHandler(myGaugeMessageOutput);

    // exit signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGILL, signalHandler);
    signal(SIGFPE, signalHandler);
    signal(SIGSEGV, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGBREAK, signalHandler);
    signal(SIGABRT, signalHandler);

    // Setup the GUI
    m_pConsole=new FMCConsole(0, 0, m_style);
    MYASSERT(m_pConsole != 0);

    // Increment the reference count on this module so that it doesn't get
    // unloaded until we want it to.
    // The reason we have to do this is because otherwise, MSFS would try to
    // unload the DLL while this thread is still running, causing a crash. By
    // incrementing the reference count, we force the DLL to stay loaded until
    // we decrement the reference count in DllUnloadThreadProc().
    ReferenceModuleByHandle(VasGaugeThread::GaugeDllModuleHandle());

    // Remember thread ID of this thread
    m_hThread=OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());

    // Create another thread that will unload the DLL when this thread exits
    // We have to do this in such a roundabout way because
    // FreeLibraryAndExitThread() cannot be called from a QThread.
    CreateThread(NULL, 0, DLLUnloadThreadProc, this, 0, NULL);

    // Tell the main thread that we've finished initializing
    m_mutex.lock();
    m_initialized=true;
    m_condInitialized.wakeAll();
    m_mutex.unlock();

    // Start the message loop
    m_pApp->setQuitOnLastWindowClosed(false);
    m_pApp->exec();

    // Delete the GUI console
    Logger::log("     ----- Thread Shutting down -----");
    delete m_pConsole;

    // Shut down the logger
    Logger::log("     ----- Thread Shutdown finished -----");
    Logger::finish();

    // Delete the application
    delete m_pApp;
}

/////////////////////////////////////////////////////////////////////////////

void VasGaugeThread::waitUntilInitialized()
{
    QMutexLocker locker(&m_mutex);

    if(!m_initialized)
        m_condInitialized.wait(&m_mutex);
}

/////////////////////////////////////////////////////////////////////////////

FMCConsole *VasGaugeThread::fmcConsole()
{
    return m_pConsole;
}

/////////////////////////////////////////////////////////////////////////////

FMCCDUStyleBase *VasGaugeThread::fmcCDUStyleBase()
{
    return m_pConsole->fmcCduLeftHandler()->fmcCduBase();
}

/////////////////////////////////////////////////////////////////////////////

FMCFCUStyleBase *VasGaugeThread::fmcFCUStyleBase()
{
    return m_pConsole->fmcFcuHandler()->fmcFcuBase();
}

/////////////////////////////////////////////////////////////////////////////

FMCPFD *VasGaugeThread::fmcPFD()
{
    return m_pConsole->fmcPFDLeftHandler()->fmcPFD();
}

/////////////////////////////////////////////////////////////////////////////

FMCNavdisplay *VasGaugeThread::fmcNavdisplay()
{
    return m_pConsole->fmcNavdisplayLeftHandler()->fmcNavdisplay();
}

/////////////////////////////////////////////////////////////////////////////

FMCECAM *VasGaugeThread::fmcUECAM()
{
    return m_pConsole->fmcUpperEcamHandler()->fmcECAM();
}

/////////////////////////////////////////////////////////////////////////////

HANDLE VasGaugeThread::hThread()
{
    return m_hThread;
}

/////////////////////////////////////////////////////////////////////////////

/* static */ bool VasGaugeThread::canStart()
{
    QMutexLocker locker(&s_mutex);

    return !s_running;
}

/* static */ HMODULE VasGaugeThread::GaugeDllModuleHandle()
{
#if 0
    HMODULE hModule;

    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCTSTR)&s_dummy, &hModule);

    return hModule;
#else
    return GetModuleHandleByAddress(GetCurrentProcess(), &s_dummy);
#endif
}
