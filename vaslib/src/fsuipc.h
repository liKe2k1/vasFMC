///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2005-2006 Alexander Wemmer 
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

#ifndef FSUIPC_HEADER__
#define FSUIPC_HEADER__

#include <windows.h>
#include <stdio.h>

#include "logger.h"

#include <QString>
#include <QMap>

#include "FSUIPC_User.h"

/////////////////////////////////////////////////////////////////////////////

class FSUIPC
{
public:
    
    FSUIPC();
    
    virtual ~FSUIPC();
    
    bool openLink();
    void closeLink();
        
    bool isLinkOk() { return m_link_ok; }

    QString getErrorText(int err_nr);
    
    inline bool read(DWORD offset, DWORD size, void* data)
	{
		if (!m_link_ok) m_link_ok = openLink();
		if (!m_link_ok) return false;
		DWORD error = 0;
		bool ret = FSUIPC_Read(offset, size, data, &error);
		if (!ret)
		{
            Logger::log(QString("FSUIPC:read: Error: %1").arg(getErrorText(error)));
            fflush(stdout);
			closeLink();
		}
		return ret;
	}

    inline bool process()
	{  
		if (!m_link_ok) m_link_ok = openLink();
		if (!m_link_ok) return false;
		DWORD error = 0;
		bool ret = FSUIPC_Process(&error);
		if (!ret) 
		{
			Logger::log(QString("FSUIPC:process: Error: %1").arg(getErrorText(error)));
            fflush(stdout);
			closeLink();
		}
		return ret;
	}
	
    bool write(DWORD offset, DWORD size, void* data);

protected:
    
    bool m_link_ok;

private:

    BYTE *m_buffer;
};

#endif

// eof
