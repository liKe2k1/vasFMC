///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2007, 2008 K. Hoercher 
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
#include "fsaccess_fgfs_base.h"
#include "fsaccess_fgfs_state.h"
#include "fsaccess_fgfs_tcas.h"
#include "fsaccess_fgfs_io.h"
#include "fsaccess_fgfs.h"
#include "navcalc.h"
#include "assert.h"


/*!the Interface as needed by FlightSimAccess.
 *
 * It sets up sane default config values, the IO-helper objects etcetc
 * and is kept in the event loop by various signals/slots 
 */
FSAccessFgFs::FSAccessFgFs (ConfigWidgetProvider* config_widget_provider, //!< use common config stuff
                            const QString& cfg_file,                      //!< with our config file
                            FlightStatus* flightstatus                    //!< the stuff to set
    ) 
    : FSAccess(flightstatus),
      m_cfg(cfg_file),
      m_state(new FGFSstate(flightstatus))
    //m_tcas(new FGTcas(flightstatus))
{

    MYASSERT(config_widget_provider != 0);
    m_cfg.setValue(FSAccessFgFs::CfgHostAddress, QHostAddress(QHostAddress::LocalHost).toString());
    m_cfg.setValue(FSAccessFgFs::CfgReadPort, 12000);
    m_cfg.setValue(FSAccessFgFs::CfgWritePort, 5401);
    m_cfg.setValue(FSAccessFgFs::CfgProtFile, QString("vasfmc"));
    m_cfg.setValue(FSAccessFgFs::CfgSetCmd, QString("set"));
    m_cfg.setValue(FSAccessFgFs::CfgApHdgProp, QString("/autopilot/settings/heading-bug-deg"));
    m_cfg.setValue(FSAccessFgFs::CfgApVsProp, QString("/autopilot/settings/vertical-speed-fpm"));
    m_cfg.setValue(FSAccessFgFs::CfgApAltProp, QString("/autopilot/settings/target-altitude-ft"));
    m_cfg.setValue(FSAccessFgFs::CfgApIasProp, QString("/autopilot/settings/target-speed-kt"));
    m_cfg.setValue(FSAccessFgFs::CfgApMachProp, QString("/autopilot/settings/target-mach"));
    m_cfg.setValue(FSAccessFgFs::CfgAltHpaProp, QString("/instrumentation/altimeter/setting-inhg"));
    m_cfg.setValue(FSAccessFgFs::CfgNavCount, 2);
    m_cfg.setValue(FSAccessFgFs::CfgNavStub
                   + QString::number(1) +
                   FSAccessFgFs::CfgFreqStub,
                   QString("/instrumentation/nav/frequencies/selected-mhz"));
    m_cfg.setValue(FSAccessFgFs::CfgNavStub
                   + QString::number(1) + 
                   FSAccessFgFs::CfgObsStub,
                   QString("/instrumentation/nav/radials/selected-deg"));
    m_cfg.setValue(FSAccessFgFs::CfgNavStub
                   + QString::number(2) +
                   FSAccessFgFs::CfgFreqStub,
                   QString("/instrumentation/nav[1]/frequencies/selected-mhz"));
    m_cfg.setValue(FSAccessFgFs::CfgNavStub
                   + QString::number(2) + 
                   FSAccessFgFs::CfgObsStub,
                   QString("/instrumentation/nav[1]/radials/selected-deg"));
    m_cfg.setValue(FSAccessFgFs::CfgAdfCount, 1);
    m_cfg.setValue(FSAccessFgFs::CfgAdfStub
                   + QString::number(1) +
                   FSAccessFgFs::CfgFreqStub,
                   QString("/instrumentation/adf/frequencies/selected-khz"));
  
    m_cfg.loadfromFile();
    m_cfg.saveToFile();

    m_cfg.setValue(FSAccessFgFs::CfgCmdSep, QString(' ')); //\bug config file cannot save that

    config_widget_provider->registerConfigWidget("Flightgear Access", &m_cfg);
    MYASSERT(connect(&m_cfg, SIGNAL(signalChanged()), this, SLOT(slotConfigChanged())));

    // for now read means (udp, bulk, ro, fixed-format via fg xml file) 
    //         write means ("interactive" telnet-like implemented by plib/net* rw)
    QHostAddress read_hostaddress, write_hostaddress;
    quint16 qreadport(m_cfg.getIntValue(FSAccessFgFs::CfgReadPort));  
    quint16 qwriteport(m_cfg.getIntValue(FSAccessFgFs::CfgWritePort));
    read_hostaddress.setAddress(m_cfg.getValue(FSAccessFgFs::CfgHostAddress));
    write_hostaddress.setAddress(m_cfg.getValue(FSAccessFgFs::CfgHostAddress));
  
    //\deprecated m_state = new FGFSstate(flightstatus, current_navdata);
    m_state->initread(read_hostaddress, qreadport, 
                      m_cfg.getValue(FSAccessFgFs::CfgProtFile));

    //MYASSERT(connect (m_state, SIGNAL(sigStatusUpdate(const newFGmsg)),
    //           m_state, SLOT(slotStatusUpdate(const newFGmsg))));
 	     
    MYASSERT(connect(m_state, SIGNAL(sigSetValid(bool)),
                     m_state, SLOT(slotSetValid(bool))));

    m_state->initwrite(write_hostaddress, qwriteport);
    MYASSERT(connect(this, SIGNAL(sigSetACParam(const QByteArray&)),
                     m_state, SLOT(slotIoWrite(const QByteArray&))));

    dialog=new FGFSTelnetDialog(this); //sort of mockup for messing with telnet, 
    //might even be useful for general debugging??
    MYASSERT(connect (dialog, SIGNAL(connect_checked(bool)),
                      m_state, SLOT(slotChangeConnection(bool))));
    MYASSERT(connect (m_state, SIGNAL(connected()),
                      dialog, SLOT(slotEnable())));
    MYASSERT(connect (m_state, SIGNAL(disconnected()),
                      dialog, SLOT(slotDisable())));
    MYASSERT(connect (dialog, SIGNAL(sigInput(const QByteArray&)),
                      m_state, SLOT(slotIoWrite(const QByteArray&))));
    MYASSERT(connect (m_state, SIGNAL(recvd(const QByteArray&)),
                      dialog, SLOT(slotDisplay(const QByteArray&))));
    //show what vasfmc sends in our debug dialog	   
    MYASSERT(connect (m_state, SIGNAL(IoWrite(const QByteArray&)),
                      dialog, SLOT(slotDisplay(const QByteArray&))));

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowFlags(Qt::SubWindow);
    dialog->show();
  
    // m_tcas = new FGTcas(read_hostaddress, 13000);
    //m_tcas->initread(QString("/home/wbh/.fgfs/vas-tcas.data"), QString("vasradar"));
  
    //  connect(m_tcas->tcasread, SIGNAL(sigStatusUpdate(FGmsg*)),
    //	  m_tcas, SLOT(slotStatusUpdate(FGmsg*)));

    //aicraft setting debug...
    //connect(this, SIGNAL(sigSetACParam(QByteArray*)),
    //	  this, SLOT(slotDebugmsg(QByteArray*)));
    if (false) {
        setNavFrequency(113800,1);
        setAdfFrequency(415000);
        setAdfFrequency(375000,1);
        setNavOBS(284);
        setAPHeading(284.843);
        setAPVs(1200);
        setAPAlt(16000);
        setAPAirspeed(220);
        setAPMach(0.6324);
    }
}

/////////////////////////////////////////////////////////////////////////////
FSAccessFgFs::~FSAccessFgFs()
{
    m_cfg.saveToFile();
    delete m_state;
    //delete m_tcas;
    //delete dialog;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessFgFs::setNavFrequency(const int freq, uint nav_index) 
{
    if (m_cfg.getIntValue(FSAccessFgFs::CfgNavCount)<nav_index+1) {
        DEBUGP(LOGWARNING, "NAV" << nav_index+1 << " freq set parameter out of configured range");
        return false;
    }
    if (!m_state->isValid()) {
        DEBUGP(LOGWARNING, "invalid flightstatus, not setting NAV" << nav_index+1 << " frq");
//    return false;
    }
    if (m_cfg.contains(FSAccessFgFs::CfgNavStub
                       + QString::number(nav_index+1) +
                       FSAccessFgFs::CfgFreqStub)) {
        QString set_nav_freq = m_cfg.getValue(FSAccessFgFs::CfgSetCmd)
                               + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                               + m_cfg.getValue(FSAccessFgFs::CfgNavStub
                                                + QString::number(nav_index+1) +
                                                FSAccessFgFs::CfgFreqStub)
                               + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                               + QString::number((freq/1000.0), *"f", 2);
        emit sigSetACParam(set_nav_freq.toLatin1());
    } else {
        DEBUGP(LOGWARNING, "NAV" << nav_index+1 << " freq set parameter not configured");
        return false;
    }
    return true;
}
bool FSAccessFgFs::setAdfFrequency(const int freq, uint adf_index) 
{
    if (m_cfg.getIntValue(FSAccessFgFs::CfgAdfCount)<adf_index+1) {
        DEBUGP(LOGWARNING, "ADF" << adf_index+1 << " freq set parameter out of configured range");
        return false;
    }
    if (!m_state->isValid()) {
        DEBUGP(LOGWARNING, "invalid flightstatus, not setting ADF" << adf_index+1 << " frq");
//    return false;
    }
    if (m_cfg.contains(FSAccessFgFs::CfgAdfStub
                       + QString::number(adf_index+1) +
                       FSAccessFgFs::CfgFreqStub)) {
        QString set_adf_freq = m_cfg.getValue(FSAccessFgFs::CfgSetCmd)
                               + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                               + m_cfg.getValue(FSAccessFgFs::CfgAdfStub
                                                + QString::number(adf_index+1) + 
                                                FSAccessFgFs::CfgFreqStub)
                               + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                               + QString::number((freq/1000.0), *"f", 2);
        emit sigSetACParam(set_adf_freq.toLatin1());
    } else {
        DEBUGP(LOGWARNING, "ADF" << adf_index+1 << " freq set parameter not configured");
        return false;
    }
    return true;
}
bool FSAccessFgFs::setNavOBS(const int radial, uint nav_index)
{
    if (m_cfg.getIntValue(FSAccessFgFs::CfgNavCount)<nav_index+1) {
        DEBUGP(LOGWARNING, "NAV" << nav_index+1 << " obs set parameter out of configured range");
        return false;
    }
    if (!m_state->isValid()) {
        DEBUGP(LOGWARNING, "invalid flightstatus, not setting NAV" << nav_index+1 << " radial");
//    return false;
    }
    if (m_cfg.contains(FSAccessFgFs::CfgNavStub
                       + QString::number(nav_index+1) + 
                       FSAccessFgFs::CfgObsStub)) {
        QString set_nav_obs = m_cfg.getValue(FSAccessFgFs::CfgSetCmd)
                              + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                              + m_cfg.getValue(FSAccessFgFs::CfgNavStub
                                               + QString::number(nav_index+1) +
                                               FSAccessFgFs::CfgObsStub)
                              + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                              + QString::number(radial, *"f", 0);
        emit sigSetACParam(set_nav_obs.toLatin1());
    } else {
        DEBUGP(LOGWARNING, "NAV" << nav_index+1 << " obs set parameter not configured");
        return false;
    }
    return true;
}
bool FSAccessFgFs::setAPHeading(double heading)
{ 
    if (!m_state->isValid()) {
        DEBUGP(LOGWARNING, "invalid flightstatus, not setting hdg bug");
//    return false;
    }
    QString set_aphdg = m_cfg.getValue(FSAccessFgFs::CfgSetCmd)
                        + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                        + m_cfg.getValue(FSAccessFgFs::CfgApHdgProp)
                        + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                        + QString::number(heading, *"f", 0);// (no need for decimals)

    if (qAbs(m_flightstatus->APHdg()-heading)>0.5) // don't get too nervous
    {
        //DEBUGP(LOGDEBUG, "set heading bug:" << set_aphdg);
        emit sigSetACParam(set_aphdg.toLatin1());
    }
    return true;
}
bool FSAccessFgFs::setAPVs(int vs)
{ 
    if (!m_state->isValid()) {
        DEBUGP(LOGWARNING, "invalid flightstatus, not setting vs");
//    return false;
    }
    QString set_apvs = m_cfg.getValue(FSAccessFgFs::CfgSetCmd)
                       + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                       + m_cfg.getValue(FSAccessFgFs::CfgApVsProp)
                       + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                       + QString::number(vs, *"f", 0);// (no need for decimals)
    //DEBUGP(LOGDEBUG, "set heading bug:" << set_aphdg);
    emit sigSetACParam(set_apvs.toLatin1());
    return true;
}
bool FSAccessFgFs::setAPAlt(unsigned int alt)
{ 
    if (!m_state->isValid()) {
        DEBUGP(LOGWARNING, "invalid flightstatus, not setting vs");
//    return false;
    }
    QString set_apalt = m_cfg.getValue(FSAccessFgFs::CfgSetCmd)
                        + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                        + m_cfg.getValue(FSAccessFgFs::CfgApAltProp)
                        + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                        + QString::number(alt, *"f", 0);// (no need for decimals)
    //DEBUGP(LOGDEBUG, "set heading bug:" << set_aphdg);
    emit sigSetACParam(set_apalt.toLatin1());
    return true;
}
bool FSAccessFgFs::setAPAirspeed(unsigned short speed_kts)
{ 
    if (!m_state->isValid()) {
        DEBUGP(LOGWARNING, "invalid flightstatus, not setting vs");
//    return false;
    }
    QString set_apairspeed = m_cfg.getValue(FSAccessFgFs::CfgSetCmd)
                             + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                             + m_cfg.getValue(FSAccessFgFs::CfgApIasProp)
                             + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                             + QString::number(speed_kts, *"f", 0);// (no need for decimals)
    //DEBUGP(LOGDEBUG, "set heading bug:" << set_aphdg);
    emit sigSetACParam(set_apairspeed.toLatin1());
    return true;
}
bool FSAccessFgFs::setAPMach(double mach)
{ 
    if (!m_state->isValid()) {
        DEBUGP(LOGWARNING, "invalid flightstatus, not setting vs");
//    return false;
    }
    QString set_apmach = m_cfg.getValue(FSAccessFgFs::CfgSetCmd)
                         + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                         + m_cfg.getValue(FSAccessFgFs::CfgApMachProp)
                         + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                         + QString::number(mach, *"f", 3);// (3 decimals)
    //DEBUGP(LOGDEBUG, "set heading bug:" << set_aphdg);
    emit sigSetACParam(set_apmach.toLatin1());
    return true;
}

bool FSAccessFgFs::setAltimeterHpa(const double& altimeter_setting)
{ 
    if (!m_state->isValid()) {
        DEBUGP(LOGWARNING, "invalid flightstatus, not setting vs");
//    return false;
    }
    QString set_altimeter_setting = m_cfg.getValue(FSAccessFgFs::CfgSetCmd)
                                    + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                                    + m_cfg.getValue(FSAccessFgFs::CfgAltHpaProp)
                                    + m_cfg.getValue(FSAccessFgFs::CfgCmdSep)
                                    + QString::number(Navcalc::getInchesFromHpa(altimeter_setting), *"f", 2);// (2 decimals)
    emit sigSetACParam(set_altimeter_setting.toLatin1());
    return true;
}
const QString FSAccessFgFs::CfgHostAddress="fgfs_hostaddress";
const QString FSAccessFgFs::CfgReadPort="fgfs_readport";
const QString FSAccessFgFs::CfgWritePort="fgfs_writeport";
const QString FSAccessFgFs::CfgProtFile="fgfs_protocolfile";
const QString FSAccessFgFs::CfgSetCmd="fgfs_set_command";
const QString FSAccessFgFs::CfgApHdgProp="fgfs_aphdg_prop";
const QString FSAccessFgFs::CfgApVsProp="fgfs_apvs_prop";
const QString FSAccessFgFs::CfgApAltProp="fgfs_apalt_prop";
const QString FSAccessFgFs::CfgApIasProp="fgfs_apias_prop";
const QString FSAccessFgFs::CfgApMachProp="fgfs_apmach_prop";
const QString FSAccessFgFs::CfgAltHpaProp="fgfs_altshpa_prop";
const QString FSAccessFgFs::CfgNavCount="fgfs_nav_count";
const QString FSAccessFgFs::CfgNavStub="fgfs_nav_";
const QString FSAccessFgFs::CfgAdfCount="fgfs_adf_count";
const QString FSAccessFgFs::CfgAdfStub="fgfs_adf_";
const QString FSAccessFgFs::CfgFreqStub="_freq_prop";
const QString FSAccessFgFs::CfgObsStub="_obs_prop";
const QString FSAccessFgFs::CfgCmdSep="fgfs_command_sep";

