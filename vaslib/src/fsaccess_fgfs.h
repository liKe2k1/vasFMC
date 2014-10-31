///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2007 K. Hoercher
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
#ifndef FGACCESS_H
#define FGACCESS_H

#include "fsaccess_fgfs_base.h"
#include "fsaccess.h"

class FGFSTelnetDialog;
class FGFSIo;
class FGFSstate;
class FGTcas;

/*! FGFS Flightsim Access.
 * Just abit of preface:
 * it rotted into some sort of spaghetti code, but I had some thoughts when I started:
 * - use flexible fgfs generic protocol 
 * - easily adapt to new flightstatus.h "interface"
 * - somewhat robust against error in transmission
 *
 * lately:
 * - differentiate more between data used for modelling reality and that derived
 *   from that reality (error prone, systemic deviation, not available)
 * - consider possible 2-step conversion 1. different language types 2. different units 
 *   (as some conversions are not feasible in the protocol xml)
 * - provide for n-way data of same semantic type (radio settings, engine settings...)
 * - despite not being problematic up to now, the fixed transmission of all or nothing
 *   including stuff like name of ac (the worst example) makes one wince
 * - must be overcome for tcas anyways...
 *
 * blahblah
 */
class FSAccessFgFs : public FSAccess
{
    Q_OBJECT
    
public:
    FSAccessFgFs(ConfigWidgetProvider* config_widget_provider, 
                 const QString& cfg_file, 
                 FlightStatus* flightstatus);
    
    //! Destructor
    virtual ~FSAccessFgFs();
    
    virtual Config* config() { return &m_cfg; };
     
    //! sets the NAV frequency, index 0 = NAV1, index 1 = NAV2
    virtual bool setNavFrequency(int freq, uint nav_index = 0);
    //! sets the ADF frequency, index 0 = ADF1, index 1 = ADF2
    virtual bool setAdfFrequency(int freq, uint adf_index = 0);
    //! sets the OBS angle, index 0 = NAV1, index 1 = NAV2
    virtual bool setNavOBS(int degrees, uint nav_index = 0);
    virtual bool setAutothrustArm(bool) { return false;};
    virtual bool setAutothrustSpeedHold(bool on) { return false;};
    virtual bool setAutothrustMachHold(bool on) { return false;};
    virtual bool setFDOnOff(bool on) { return false;};
    virtual bool setAPOnOff(bool on) { return false;};
    //! sets the heading bug for AP
    virtual bool setAPHeading(double heading);
    //! sets the desired vs in ft/min for AP
    virtual bool setAPVs(int vs);
    //! sets desired altitude in feet (pressure?) for AP
    virtual bool setAPAlt(unsigned int alt);
    //! sets desired airspeed (IAS?) in kts for AP
    virtual bool setAPAirspeed(unsigned short speed_kts);
    //! sets desired airspeed in Mach for AP
    virtual bool setAPMach(double mach);
    virtual bool setAPHeadingHold(bool on) {return false;};
    virtual bool setAPAltHold(bool on) {return false;};
    virtual bool setNAV1Arm(bool on) {return false;};
    virtual bool setAPPArm(bool on) {return false;};
    virtual bool setUTCTime(const QTime& utc_time) { return false; };
    virtual bool setUTCDate(const QDate& utc_date) { return false; };
    virtual bool freeThrottleAxes() { return false;};
    virtual bool setThrottle(double) { return false;};
    virtual bool setSBoxTransponder(bool) { return false;};
    virtual bool setFlaps(uint notch) { return false; };
    virtual bool setAileron(double percent) { return false; };
    virtual bool setElevator(double percent) { return false; };
    virtual bool setElevatorTrim(double percent) { return false; };
    //! set Kollsman Window in hPa
    virtual bool setAltimeterHpa(const double&);
  
protected slots:

    //!noop request data from the FS.
    //! but we (must) use a internal state machine
    void slotRequestData() {};
  
    //!noop adapts the update timers.
    //\see setupRefreshTimer()
    // perhaps we could use it for something different...
    void slotConfigChanged() {};

protected:

    //!noop.
    // we don't poll, but get fed
    void setupRefreshTimer(bool) {};
    bool checkLink() {return true;};

protected:
    //! FGFS fsaccess configuration.
    Config m_cfg;
    //! used to trigger to read data from the flightsim.
    QTimer m_refresh_timer;
    //! used to time tcas data fetches.
    // unused?
    QTime m_tcas_refresh_timer;

    //! internal implementation.
    FGFSstate* m_state;
    //! mockup for write access.
    FGFSTelnetDialog* dialog;
    //! internal implementation.
    FGTcas* m_tcas;
    signals:
    //! get data to FG via telnet-like netChat.
    void sigSetACParam(const QByteArray&);
  
private:
    //! Hidden copy-constructor 
    FSAccessFgFs(const FSAccessFgFs&);
    //! Hidden assignment operator
    const FSAccessFgFs& operator = (const FSAccessFgFs&);
  
    //! avoid evil #define's *g*
    static const QString CfgHostAddress;
    static const QString CfgReadPort;
    static const QString CfgWritePort;
    static const QString CfgProtFile;
    static const QString CfgSetCmd;
    static const QString CfgApHdgProp;
    static const QString CfgApVsProp;
    static const QString CfgApAltProp;
    static const QString CfgApIasProp;
    static const QString CfgApMachProp;
    static const QString CfgAltHpaProp;
    static const QString CfgNavCount;
    static const QString CfgNavStub;
    static const QString CfgAdfCount;
    static const QString CfgAdfStub;
    static const QString CfgFreqStub;
    static const QString CfgObsStub;
    static const QString CfgCmdSep;
};

#endif  //FGACCESS_H
