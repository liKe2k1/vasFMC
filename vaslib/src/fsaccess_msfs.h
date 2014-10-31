/*! \file    msfs.fsaccess.h
    \author  Alexander Wemmer, alex@wemmer.at
*/

#ifndef MSFS_FSACCESS_H
#define MSFS_FSACCESS_H

#include <QTime>

#include "assert.h"
#include "config.h"

#include "fsuipc.h"
#include "fsaccess.h"


/////////////////////////////////////////////////////////////////////////////

typedef struct _MSFS_TCAS_DATA
{
    // 0 = empty, otherwise this is an FS-generated ID.
    //(Do not use this for anything other than checking if the slot is empty
    // or used it may be re-used for other things at a later date).
    qint32 id; 

    // 32-bit float, in degrees, -ve = South
    float lat;

    // 32-bit float, in degrees, -ve = West 
    float lon;

    // 32-bit float, in feet
    float alt;

    // 16-bits. Heading. Usual 360 degrees == 65536 format.
    // Note that this is degrees TRUE, not MAG
    qint16 hdg;

    // 16-bits. Knots Ground Speed
    qint16 gs;

    // 16-bits, signed feet per minute V/S
    qint16 vs;

    // Zero terminated string identifying the aircraft. By default this is:
    // Airline & Flight Number, or Tail number
    // For Tail number, if more than 14 chars you get the *LAST* 14
    // Airline name is truncated to allow whole flight number to be included
    char idATC[15];

    // Zero in FS2002, a status indication in FS2004�see list below.
    qint8 bState;

    // the COM1 frequency set in the AI aircraft�s radio. (0Xaabb as in 1aa.bb)
    qint16 com1;

    // Constructor
    _MSFS_TCAS_DATA() : id(0), lat(0.0), lon(0.0), alt(0), hdg(0), gs(0), vs(0), bState(0), com1(0) {};

    //convert
    void writeToTcasEntry(TcasEntry& entry)
    {
        entry.m_valid = true;
        entry.m_id = id;
        entry.m_position = Waypoint("TCAS", QString::null, lat, lon);
        entry.m_altitude_ft = (int)(alt + 0.5);
        entry.m_true_heading = hdg;
        entry.m_groundspeed_kts = gs;
        entry.m_vs_fpm = vs;
    };

} MSFS_TCAS_DATA;

/////////////////////////////////////////////////////////////////////////////

//! MSFS Flightsim Access
class FSAccessMsFs : public FSAccess
{
    Q_OBJECT

public:
    //! Standard Constructor
    FSAccessMsFs(ConfigWidgetProvider* config_widget_provider, 
                 const QString& cfg_file, FlightStatus* flightstatus);

    //! Destructor
    virtual ~FSAccessMsFs();

    virtual Config* config() { return &m_cfg; }

    //----- write access

    //! sets the NAV frequency, index 0 = NAV1, index 1 = NAV2
    virtual bool setNavFrequency(int freq, uint nav_index);
    //! sets the ADF frequency, index 0 = ADF1, index 1 = ADF2
    virtual bool setAdfFrequency(int freq, uint adf_index);
    //! sets the OBS angle, index 0 = NAV1, index 1 = NAV2
    virtual bool setNavOBS(int degrees, uint nav_index);

    virtual bool setAutothrustArm(bool armed);
    virtual bool setAutothrustSpeedHold(bool on);
    virtual bool setAutothrustMachHold(bool on);

    virtual bool setFDOnOff(bool on);
    virtual bool setAPOnOff(bool on);
    virtual bool setAPHeading(double heading);
    virtual bool setAPVs(int vs_ft_min);
    virtual bool setAPAlt(unsigned int alt);
    virtual bool setAPAirspeed(unsigned short speed_kts);
    virtual bool setAPMach(double mach);
    virtual bool setAPHeadingHold(bool on);
    virtual bool setAPAltHold(bool on);

    virtual bool setUTCTime(const QTime& utc_time);
    virtual bool setUTCDate(const QDate& utc_date);

    virtual bool freeThrottleAxes();
    virtual bool setThrottle(double percent);

    virtual bool setSBoxTransponder(bool on);
    virtual bool setSBoxIdent();
    virtual bool setFlaps(uint notch);

    virtual bool freeControlAxes();
    virtual bool freeAileronAxis();
    virtual bool freeElevatorAxis();
    virtual bool setAileron(double percent);
    virtual bool setElevator(double percent);
    virtual bool setElevatorTrimPercent(double percent);
    virtual bool setElevatorTrimDegrees(double degrees);

    virtual bool freeSpoilerAxis();
    virtual bool setSpoiler(double percent);

    virtual bool setNAV1Arm(bool on);
    virtual bool setAPPArm(bool on);

    virtual bool setAltimeterHpa(const double& hpa);

    virtual bool setPushback(PushBack status);
    
    virtual void writeFMCStatusToSim(const FMCStatusData& fmc_status_data);

protected slots:

    //! request data from the FS
    void slotRequestData();

    //! adapts the update timers
    void slotConfigChanged() { setupRefreshTimer(false); }

protected:

    void run();
    bool checkLink();
    void setupRefreshTimer(bool pause_mode);

protected:

    //! MSFS fsaccess configuration
    Config m_cfg;

    //! MSFS access
    FSUIPC *m_fsuipc;

    //! used to trigger to read data from the flightsim
    QTimer m_refresh_timer;

    //! used to time tcas data fetches
    QTime m_tcas_refresh_timer;

    //! flaps raw value increase per flap notch
    uint m_flaps_inc_per_notch;

    short m_axes_disco_310a;
    char m_axes_disco_341a;

    QTime m_refresh_analyse_timer;

private:
    //! Hidden copy-constructor
    FSAccessMsFs(const FSAccessMsFs&);
    //! Hidden assignment operator
    const FSAccessMsFs& operator = (const FSAccessMsFs&);
};

#endif /* MSFS_FSACCESS_H */
