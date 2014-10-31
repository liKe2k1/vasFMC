/*! \file    msfs.fsaccess.cpp
    \author  Alexander Wemmer, alex@wemmer.at
*/

#include "fsaccess_msfs.h"
#include "code_timer.h"
#include "bithandling.h"

#define CFG_MSFS_REFRESH_PERIOD_MS "msfs_refresh_period_ms"
#define CFG_MSFS_TCAS_REFRESH_PERIOD_MS "msfs_tcas_refresh_period_ms"

#define TCAS_SLOTS 96
MSFS_TCAS_DATA tcas_data[TCAS_SLOTS];

#define HDG_CONV (65536.0 / 360.0)
#define ALT_CONV (65536.0 / 3.2808)

#define AVIONICS_NAV1_GOOD 2
#define AVIONICS_NAV2_GOOD 4
#define AVIONICS_ADF1_GOOD 8
#define AVIONICS_ADF2_GOOD 2048
#define AVIONICS_NAV1_DME 16
#define AVIONICS_NAV2_DME 32
#define AVIONICS_NAV1_ILS 64
#define AVIONICS_NAV2_ILS 4096
#define AVIONICS_LOC_GOOD 256
#define AVIONICS_GS_GOOD 512

#define FSCTRL_OFFSET 0x6DD0 // offsets including 0x6DEF are reserved for vasFMC
#define FSCTRL_SIZE 10

#define FMCSTATUS_OFFSET 0x7A60
#define FMCSTATUS_SIZE 20

#define ACFT_TYPE_STRING_LEN 256

char fs_control_zero[FSCTRL_SIZE];


/////////////////////////////////////////////////////////////////////////////

FSAccessMsFs::FSAccessMsFs(ConfigWidgetProvider* config_widget_provider, 
                           const QString& cfg_file,
                           FlightStatus* flightstatus) :
    FSAccess(flightstatus), 
    m_cfg(cfg_file),
    m_fsuipc(new FSUIPC),
    m_flaps_inc_per_notch(0)
{
    MYASSERT(config_widget_provider != 0);
    MYASSERT(m_fsuipc != 0);

    MYASSERT(sizeof(char) == 1);
    MYASSERT(sizeof(short) == 2);
    MYASSERT(sizeof(int) == 4);
    MYASSERT(sizeof(long) == 4);
    MYASSERT(sizeof(long long) == 8);
    MYASSERT(sizeof(double) == 8);

    // setup config

    m_cfg.setValue(CFG_MSFS_REFRESH_PERIOD_MS, 50);
    m_cfg.setValue(CFG_MSFS_TCAS_REFRESH_PERIOD_MS, 1000);
    m_cfg.loadfromFile();
    m_cfg.saveToFile();
    config_widget_provider->registerConfigWidget("FS Access", &m_cfg);

    MYASSERT(connect(&m_cfg, SIGNAL(signalChanged()), this, SLOT(slotConfigChanged())));

    // setup data refresh timer
    MYASSERT(connect(&m_refresh_timer, SIGNAL(timeout()), this, SLOT(slotRequestData())));
    setupRefreshTimer(false);
    m_tcas_refresh_timer.start();
    m_axes_disco_310a = 0;
    m_axes_disco_341a = 0;

    memset(fs_control_zero, 0, FSCTRL_SIZE);
}

/////////////////////////////////////////////////////////////////////////////

FSAccessMsFs::~FSAccessMsFs()
{
    freeThrottleAxes();
    freeControlAxes();
    m_fsuipc->process();
    m_cfg.saveToFile();
    delete m_fsuipc;
}

/////////////////////////////////////////////////////////////////////////////

void FSAccessMsFs::slotRequestData()
{
//     if (m_refresh_analyse_timer.elapsed() > 10)
//         Logger::log(QString("FSAccessMsFs:slotRequestData: elapsed since last call = %1ms").arg(m_refresh_analyse_timer.elapsed()));
//     m_refresh_analyse_timer.start();

    if (!checkLink()) 
    {
        if (m_flightstatus->isValid())
            Logger::log("FSAccessMsFs:slotRequestData: FLINK to FS is down");

        m_flightstatus->invalidate();
        return;
    }

    bool was_paused = m_flightstatus->paused;

    //-----

    int ground_alt = 0;
    m_fsuipc->read(0x20, 4, &ground_alt);

    char readblock23B[0x3d5];
    m_fsuipc->read(0x23B, 0x3cf, readblock23B); // 0x23b - 0x610
    
    char readblock764[0x10e];
    m_fsuipc->read(0x764, 0x10e, readblock764); // 0x764 - 0x872

    char readblock88C[0x261];
    m_fsuipc->read(0x88c, 0x261, readblock88C); // 0x88c - 0xaed

    char readblockB74[0x168];
    m_fsuipc->read(0xb74, 0x168, readblockB74); // 0xb74 - 0xcdc

    char readblockD0C[0x1e4];
    m_fsuipc->read(0xd0c, 0x1e4, readblockD0C); // 0xd0c - 0xef0

    short qnh = 0;
    m_fsuipc->read(0xF48, 2, &qnh);

//TODO
//     char time_of_day;
//     m_fsuipc->read(0x115e, 1, &time_of_day);

    short mach;
    m_fsuipc->read(0x11c6, 2, &mach);

    short tat = 0;
    m_fsuipc->read(0x11D0, 2, &tat);

    long gps_enabled = 0;
    m_fsuipc->read(0x132C, 4, &gps_enabled);

    char readblock2000[0x400];
    m_fsuipc->read(0x2000, 0x400, readblock2000);

    char readblock281C[0xd4];
    m_fsuipc->read(0x281c, 0xd4, readblock281C);

    char readblock2E80[0x78];
    m_fsuipc->read(0x2e80, 0x78, readblock2E80);

    char readblock3000[0xc00];
    char* readblock3328 = readblock3000 + 0x328;
    m_fsuipc->read(0x3000, 0xc00, readblock3000);

    //-----
    char aircraft_type[ACFT_TYPE_STRING_LEN];
    m_fsuipc->read(0x3D00, 256, aircraft_type);

    double magnetic_track;
    m_fsuipc->read(0x6040, 8, &magnetic_track);

    // read TCAS data

    bool update_tcas = false;
    if (m_tcas_refresh_timer.elapsed() >= m_cfg.getIntValue(CFG_MSFS_TCAS_REFRESH_PERIOD_MS))
    {
        update_tcas = true;
        m_tcas_refresh_timer.start();
        m_flightstatus->clearTcasEntryList();
        m_fsuipc->read(0xf080, sizeof(MSFS_TCAS_DATA)*TCAS_SLOTS, tcas_data);
    }

    // read and clear FS controls
    char fs_control[FSCTRL_SIZE];
    m_fsuipc->read(FSCTRL_OFFSET, FSCTRL_SIZE, fs_control);

    //----- process the requests

    if (!m_fsuipc->process())
    {
        m_flightstatus->invalidate();
        Logger::log("Could not process FSUIPC requests");
        return;
    }
    
    //----- assign received values

    int fs_version = (char&)readblock3000[0x308];
    
    m_flightstatus->speed_vs0_kts = Navcalc::round((double&)readblock23B[0x2fd] * 3600.0 * Navcalc::FEET_TO_NM);
    m_flightstatus->speed_vs1_kts = Navcalc::round((double&)readblock23B[0x305] * 3600.0 * Navcalc::FEET_TO_NM);
    m_flightstatus->speed_vc_kts = Navcalc::round((double&)readblock23B[0x30d] * 3600.0 * Navcalc::FEET_TO_NM);
    m_flightstatus->speed_min_drag_kts = Navcalc::round((double&)readblock23B[0x315] * 3600.0 * Navcalc::FEET_TO_NM);

    m_flightstatus->lat = ((long long&)readblock23B[0x325] * 90.0) / (10001750.0 * 65536.0 * 65536.0);
    m_flightstatus->lon = ((long long&)readblock23B[0x32d] * 360.0) / (65536.0 * 65536.0 * 65536.0 * 65536.0);
    m_flightstatus->setTrueHeading(Navcalc::trimHeading(((long&)readblock23B[0x345] * 360.0)/(65536.0*65536.0)));
    m_flightstatus->setMagneticTrack(Navcalc::toDeg(magnetic_track));

    m_flightstatus->obs1 = (short&)readblockB74[0xda];
    m_flightstatus->obs1_loc_needle = (char&)readblockB74[0xd4]; // TODO loc alive??
    m_flightstatus->obs1_gs_needle = ((char&)readblockB74[0xd8] != 0) ? (char&)readblockB74[0xd5] :  -1000;

    m_flightstatus->obs2 = (short&)readblockB74[0xea];
    m_flightstatus->obs2_loc_needle = (char&)readblockB74[0xe5];
    m_flightstatus->obs2_gs_needle = ((char&)readblockB74[0xdb] != 0) ? (char&)readblockB74[0xfa]:  -1000;

    m_flightstatus->outer_marker = (short&)readblockB74[0x3c] != 0;
    m_flightstatus->middle_marker = (short&)readblockB74[0x3a] != 0;
    m_flightstatus->inner_marker = (short&)readblockB74[0x38] != 0;

    m_flightstatus->aoa_degrees = Navcalc::toDeg((double&)readblock2E80[0x50]);
    m_flightstatus->slip_percent = ((char&)readblock23B[0x133] / 128.0) * 100.0;

    if (fs_control[0] != 0) m_flightstatus->fsctrl_nd_left_list.append(fs_control[0]);
    if (fs_control[1] != 0) m_flightstatus->fsctrl_pfd_left_list.append(fs_control[1]);
    if (fs_control[2] != 0) m_flightstatus->fsctrl_cdu_left_list.append(fs_control[2]);
    if (fs_control[3] != 0) m_flightstatus->fsctrl_fmc_list.append(fs_control[3]);
    if (fs_control[4] != 0) m_flightstatus->fsctrl_ecam_list.append(fs_control[4]);
    if (fs_control[5] != 0) m_flightstatus->fsctrl_fcu_list.append(fs_control[5]);
    if (fs_control[6] != 0) m_flightstatus->fsctrl_nd_right_list.append(fs_control[6]);
    if (fs_control[7] != 0) m_flightstatus->fsctrl_pfd_right_list.append(fs_control[7]);
    if (fs_control[8] != 0) m_flightstatus->fsctrl_cdu_right_list.append(fs_control[8]);

    m_flightstatus->smoothed_altimeter_readout = 
        ((int&)readblockB74[0xa4] != 2) ? 
        (long&)readblock3000[0x324] : (long&)readblock3000[0x324] * Navcalc::METER_TO_FEET;

    m_flightstatus->alt_ft = 
        ((int&)readblock23B[0x339] + ((uint&)readblock23B[0x335]/pow(2,32))) * Navcalc::METER_TO_FEET;
    m_flightstatus->ground_alt_ft = ground_alt * Navcalc::METER_TO_FEET / 256.0;
    m_flightstatus->smoothed_vs = (int&)readblock23B[0x8D] * Navcalc::METER_TO_FEET * 60.0 / 256.0;

    m_flightstatus->ground_speed_kts = (long&)readblock23B[0x79] / 65535.0 * Navcalc::METER_TO_NM * 3600.0;
    m_flightstatus->tas = (long&)readblock23B[0x7d] / 128.0;

    m_flightstatus->smoothed_ias = qMax(30.0, (long&)readblock23B[0x81] / 128.0);
    m_flightstatus->barber_pole_speed = (int&)readblock23B[0x89] / 128.0;
    m_flightstatus->mach = mach / 20480.0;

    m_flightstatus->pitch = (long&)readblock23B[0x33d] * 360.0 / (65536.0 * 65536.0);
    m_flightstatus->bank  = (long&)readblock23B[0x341] * 360.0 / (65536.0 * 65536.0);

    m_flightstatus->magvar = (short&)readblock23B[0x65] * (360.0 / 65536.0);
    if (m_flightstatus->magvar > 180.0) m_flightstatus->magvar -= 360;

    m_flightstatus->paused = ((short&)readblock23B[0x29] != 0) || ((char&)readblock3000[0x364] != 0);
    m_flightstatus->onground = (short&)readblock23B[0x12b];

    m_flightstatus->slew = (short&)readblock23B[0x3a1];
    m_flightstatus->view_dir_deg = (int)( 0.5 + (((int&)readblock23B[0x395]/65536.0)*360.0));

    m_flightstatus->fs_utc_time = QTime((char&)readblock23B[0x0], (char&)readblock23B[0x1]);
    m_flightstatus->fs_utc_date = QDate((short&)readblock23B[0x5], 1, 1);
    m_flightstatus->fs_utc_date = m_flightstatus->fs_utc_date.addDays((short&)readblock23B[0x3]-1);

    m_flightstatus->tat = tat / 256.0;
    m_flightstatus->oat = (short&)readblockD0C[0x180] / 256.0;

    m_flightstatus->wind_speed_kts = (short&)readblockD0C[0x184];
    m_flightstatus->wind_dir_deg_true = Navcalc::trimHeading((short&)readblockD0C[0x186] * (360.0 / 65536.0));
    if (m_flightstatus->alt_ft < (short&)readblockD0C[0x1e2]) m_flightstatus->wind_dir_deg_true += m_flightstatus->magvar;

    m_flightstatus->dew = (short&)readblock23B[0x28d] / 256.0;
    m_flightstatus->qnh = qnh / 16.0;

    m_flightstatus->ap_available = (int&)readblock764[0x0];
    m_flightstatus->ap_enabled = (int&)readblock764[0x58];
    m_flightstatus->ap_hdg_lock = (int&)readblock764[0x64];

    m_flightstatus->setAPHdgExternal(Navcalc::trimHeading(Navcalc::round((short&)readblock764[0x68] / HDG_CONV)));
    m_flightstatus->ap_alt_lock = (int&)readblock764[0x6c];
    m_flightstatus->setAPAltExternal(1 + (unsigned int)(((int&)readblock764[0x70] / ALT_CONV) + 0.5));
    m_flightstatus->ap_speed_lock = (int&)readblock764[0x78];
    m_flightstatus->setAPSpdExternal((short&)readblock764[0x7e]);
    m_flightstatus->ap_mach_lock = (int&)readblock764[0x80];
    m_flightstatus->setAPMachExternal((unsigned short&)readblock764[0x084] / 65536.0);

    m_flightstatus->ap_vs_lock = (int&)readblock764[0x88];
    short ap_vsvalue = (short&)readblock764[0x8e];
    if (ap_vsvalue > 17000) ap_vsvalue -= 65536;
    m_flightstatus->setAPVsExternal(ap_vsvalue);

    m_flightstatus->ap_nav1_lock = (int&)readblock764[0x60];
    m_flightstatus->ap_gs_lock = (int&)readblock764[0x98];
    m_flightstatus->ap_app_lock = (int&)readblock764[0x9c];
    m_flightstatus->ap_app_bc_lock = (int&)readblock764[0xA0];
    m_flightstatus->at_toga = (int&)readblock764[0xa8];
    m_flightstatus->at_arm = (int&)readblock764[0xac];

    m_flightstatus->gps_enabled = gps_enabled;
    m_flightstatus->avionics_status = (short&)readblock3000[0x300];

    aircraft_type[ACFT_TYPE_STRING_LEN-1] = 0;
    m_flightstatus->aircraft_type = aircraft_type;

    m_flightstatus->setAltPressureSettingHpaExternal((short&)readblock23B[0xf5] / 16.0);

    if (update_tcas)
    {
        for(int index=0; index<TCAS_SLOTS; ++index)
        {
            if (tcas_data[index].id != 0)
            {
                TcasEntry tcas_entry = TcasEntry();
                tcas_data[index].writeToTcasEntry(tcas_entry);
                m_flightstatus->addTcasEntry(tcas_entry);
            }
        }
    }

    // nav1

    char* dme1_dist = readblockB74 + 0xb5;
    dme1_dist[4] = 0;
    m_flightstatus->nav1_freq = (10000 + convertBCDToInt((short&)readblock23B[0x115])) * 10;

    if ((m_flightstatus->avionics_status & AVIONICS_NAV1_GOOD))
    {
        m_flightstatus->nav1.setId(QString(readblock3000));
        m_flightstatus->nav1.setLat(((int&)readblock764[0xf8]*90.0)/10001750.0);
        m_flightstatus->nav1.setLon(((int&)readblock764[0x100]*360.0)/(65536.0 * 65536.0));
        m_flightstatus->nav1_has_loc = m_flightstatus->avionics_status & AVIONICS_NAV1_ILS;
        m_flightstatus->nav1_loc_mag_heading = 
            (int)Navcalc::trimHeading( (((double)(short&)readblock764[0x10c])*360.0/65535.0) - 180.0);

        if (dme1_dist[2] == '.') dme1_dist[4] = 0;
        else if (dme1_dist[3] == '.') dme1_dist[3] = 0;
        char* eff_dme1_dist = dme1_dist;
        while (eff_dme1_dist[0] != 0 && eff_dme1_dist[0] == '0') ++eff_dme1_dist;

        if (m_flightstatus->avionics_status & AVIONICS_NAV1_DME)
            m_flightstatus->nav1_distance_nm = QString(eff_dme1_dist);
        else
            m_flightstatus->nav1_distance_nm = QString::null;

        m_flightstatus->nav1_bearing =
            -Navcalc::trimHeading(
                m_flightstatus->smoothedTrueHeading() - 
                Navcalc::getTrackBetweenWaypoints(m_flightstatus->current_position_smoothed, m_flightstatus->nav1));
    }
    else
    {
        m_flightstatus->nav1 = Waypoint();
        m_flightstatus->nav1_has_loc = false;
        m_flightstatus->nav1_distance_nm = QString::null;
        m_flightstatus->nav1_bearing.clear();
    }
    
    m_flightstatus->obs1_to_from = readblockB74[0xd7];

    // vor2

    char* dme2_dist = readblockB74 + 0xbf;
    dme2_dist[4] = 0;
    m_flightstatus->nav2_freq = (10000 + convertBCDToInt((short&)readblock23B[0x117])) * 10;
        
    if ((m_flightstatus->avionics_status & AVIONICS_NAV2_GOOD))
    {
        m_flightstatus->nav2.setId(QString(readblock3000 + 0x1f));
        m_flightstatus->nav2.setLat(((long&)readblock764[0xf4]*90.0)/10001750.0);
        m_flightstatus->nav2.setLon(((long&)readblock764[0xfc]*360.0)/(65536.0 * 65536.0));
        m_flightstatus->nav2_has_loc = m_flightstatus->avionics_status & AVIONICS_NAV2_ILS;

        if (dme2_dist[2] == '.') dme2_dist[4] = 0;
        else if (dme2_dist[3] == '.') dme2_dist[3] = 0;
        char* eff_dme2_dist = dme2_dist;
        while (eff_dme2_dist[0] != 0 && eff_dme2_dist[0] == '0') ++eff_dme2_dist;

        if (m_flightstatus->avionics_status & AVIONICS_NAV2_DME)
            m_flightstatus->nav2_distance_nm = QString(eff_dme2_dist);
        else
            m_flightstatus->nav2_distance_nm = QString::null;

        m_flightstatus->nav2_bearing =
            -Navcalc::trimHeading(
                m_flightstatus->smoothedTrueHeading() - 
                Navcalc::getTrackBetweenWaypoints(m_flightstatus->current_position_smoothed, m_flightstatus->nav2));
    }
    else
    {
        m_flightstatus->nav2 = Waypoint();
        m_flightstatus->nav2_has_loc = false;
        m_flightstatus->nav2_distance_nm = QString::null;
        m_flightstatus->nav2_bearing.clear();
    }

    m_flightstatus->obs2_to_from = readblockB74[0xe7];

    // adf1

    int adf_freq = 
        ((convertBCDToInt((short&)readblock23B[0x111]) + 
          (10*convertBCDToInt((short&)readblock23B[0x11b] & 0xFF00))) * 1000) +
        (convertBCDToInt((short&)readblock23B[0x11b] & 0x00FF) * 100);

    if (m_flightstatus->avionics_status & AVIONICS_ADF1_GOOD)
    {
        m_flightstatus->adf1 = Ndb(readblock3000 + 0x3e, QString::null, 0.0, 0.0, adf_freq, 0, 0, QString::null);
        m_flightstatus->adf1_bearing = Navcalc::trimHeading((short&)readblockB74[0xf6] * 360.0 / 65536.0);
    }
    else
    {
        m_flightstatus->adf1 = Ndb(QString::null, QString::null, 0.0, 0.0, adf_freq, 0, 0, QString::null);
        m_flightstatus->adf1_bearing.clear();
    }

    // adf2

    adf_freq = 
        ((convertBCDToInt((short&)readblock23B[0x99]) + (10*convertBCDToInt((short&)readblock23B[0x9B] & 0xFF00))) * 1000) +
        (convertBCDToInt((short&)readblock23B[0x9B] & 0x00FF) * 100);

    if (m_flightstatus->avionics_status & AVIONICS_ADF2_GOOD)
    {
        m_flightstatus->adf2 = Ndb(readblock23B + 0xa1, QString::null, 0.0, 0.0, adf_freq, 0, 0, QString::null);
        m_flightstatus->adf2_bearing = Navcalc::trimHeading((short&)readblock23B[0x9d] * 360.0 / 65536.0);
    }
    else
    {
        m_flightstatus->adf2 = Ndb(QString::null, QString::null, 0.0, 0.0, adf_freq, 0, 0, QString::null);
        m_flightstatus->adf2_bearing.clear();
    }

    //-----

    m_flightstatus->engine_data[1].smoothed_n1 = (double&)readblock2000[0x000];
    m_flightstatus->engine_data[2].smoothed_n1 = (double&)readblock2000[0x100];
    m_flightstatus->engine_data[3].smoothed_n1 = (double&)readblock2000[0x200];
    m_flightstatus->engine_data[4].smoothed_n1 = (double&)readblock2000[0x300];

    m_flightstatus->engine_data[1].n2_percent = (double&)readblock2000[0x008];
    m_flightstatus->engine_data[2].n2_percent = (double&)readblock2000[0x108];
    m_flightstatus->engine_data[3].n2_percent = (double&)readblock2000[0x208];
    m_flightstatus->engine_data[4].n2_percent = (double&)readblock2000[0x308];

    m_flightstatus->engine_data[1].ff_kg_per_hour = (double&)readblock88C[0x08c] * Navcalc::POUNDS_TO_KG;
    m_flightstatus->engine_data[2].ff_kg_per_hour = (double&)readblock88C[0x124] * Navcalc::POUNDS_TO_KG;
    m_flightstatus->engine_data[3].ff_kg_per_hour = (double&)readblock88C[0x16c] * Navcalc::POUNDS_TO_KG;
    m_flightstatus->engine_data[4].ff_kg_per_hour = (double&)readblock88C[0x254] * Navcalc::POUNDS_TO_KG;

    // 16384 = 860 degree celsius
    m_flightstatus->engine_data[1].egt_degrees = (short&)readblock88C[0x032] * 0.05249;
    m_flightstatus->engine_data[2].egt_degrees = (short&)readblock88C[0x0ca] * 0.05249;
    m_flightstatus->engine_data[3].egt_degrees = (short&)readblock88C[0x162] * 0.05249;
    m_flightstatus->engine_data[4].egt_degrees = (short&)readblock88C[0x1fa] * 0.05249;

    m_flightstatus->engine_data[1].anti_ice_on = (short&)readblock88C[0x026];
    m_flightstatus->engine_data[2].anti_ice_on = (short&)readblock88C[0x0be];
    m_flightstatus->engine_data[3].anti_ice_on = (short&)readblock88C[0x156];
    m_flightstatus->engine_data[4].anti_ice_on = (short&)readblock88C[0x1ee];

    if (!m_separate_throttle_lever_mode)
    {
        m_flightstatus->setAllThrottleLeversInputPercent((((short&)readblock3328[0x6]) + 16384) / 327.68);
    }
    else
    {
        m_flightstatus->engine_data[1].throttle_input_percent = (((short&)readblock3328[0x8]) + 16384) / 327.68;
        m_flightstatus->engine_data[2].throttle_input_percent = (((short&)readblock3328[0xa]) + 16384) / 327.68;
        m_flightstatus->engine_data[3].throttle_input_percent = (((short&)readblock3328[0xc]) + 16384) / 327.68;
        m_flightstatus->engine_data[4].throttle_input_percent = (((short&)readblock3328[0xe]) + 16384) / 327.68;
    }

    m_flightstatus->elevator_input_percent = ((short&)readblock3328[0x0]) / 163.84;
    m_flightstatus->aileron_input_percent = ((short&)readblock3328[0x2]) / 163.84;
    m_flightstatus->rudder_input_percent = ((short&)readblock3328[0x4]) / 163.84;

    m_flightstatus->engine_data[1].reverser_percent = (double&)readblock2000[0x07c];
    m_flightstatus->engine_data[2].reverser_percent = (double&)readblock2000[0x17c];
    m_flightstatus->engine_data[3].reverser_percent = (double&)readblock2000[0x27c];
    m_flightstatus->engine_data[4].reverser_percent = (double&)readblock2000[0x37c];

    //----- engine data

    m_flightstatus->nr_of_engines = (short&)readblock88C[0x260];

    switch((char&)readblock23B[0x3ce])
    {
        case(0): {
            m_flightstatus->engine_type = FlightStatus::ENGINE_TYPE_PISTON; 
            m_flightstatus->engine_ignition_on = 
                (((short&)readblock88C[0x6]) == 4) ||
                (((short&)readblock88C[0x9e]) == 4) ||
                (((short&)readblock88C[0x136]) == 4) ||
                (((short&)readblock88C[0x1ce]) == 4);
            break;
        }
        case(1): {
            m_flightstatus->engine_type = FlightStatus::ENGINE_TYPE_JET; 
            m_flightstatus->engine_ignition_on = 
                (((short&)readblock88C[0x6]) == 1) ||
                (((short&)readblock88C[0x9e]) == 1) ||
                (((short&)readblock88C[0x136]) == 1) ||
                (((short&)readblock88C[0x1ce]) == 1);
            ;
        }
        case(5): {
            m_flightstatus->engine_type = FlightStatus::ENGINE_TYPE_TURBOPROP; 
            m_flightstatus->engine_ignition_on = 
                (((short&)readblock88C[0x6]) == 1) ||
                (((short&)readblock88C[0x9e]) == 1) ||
                (((short&)readblock88C[0x136]) == 1) ||
                (((short&)readblock88C[0x1ce]) == 1);
            break;
        }
        default: {
            m_flightstatus->engine_type = FlightStatus::ENGINE_TYPE_INVALID; 
            m_flightstatus->engine_ignition_on = false;
            break;
        }
    }

    //-4096 to +16384
    m_flightstatus->engine_data[1].throttle_lever_percent = ((short&)readblock88C[0x0]) / 163.84;
    m_flightstatus->engine_data[2].throttle_lever_percent = ((short&)readblock88C[0x98]) / 163.84;
    m_flightstatus->engine_data[3].throttle_lever_percent = ((short&)readblock88C[0x130]) / 163.84;
    m_flightstatus->engine_data[4].throttle_lever_percent = ((short&)readblock88C[0x1c8]) / 163.84;

    m_flightstatus->elevator_percent = ((short&)readblockB74[0x3e]) / 163.83;
    m_flightstatus->aileron_percent = ((short&)readblockB74[0x42]) / 163.83;
    m_flightstatus->rudder_percent = ((short&)readblockB74[0x46]) / 163.83;
    m_flightstatus->elevator_trim_percent = ((short&)readblockB74[0x4c]) / 163.83;
    
    //TODO
//     m_flightstatus->engine_generator_1 = 
//         m_flightstatus->engine_generator_2 = 
//         m_flightstatus->engine_generator_3 = 
//         m_flightstatus->engine_generator_4 = false;

    //-----

    m_flightstatus->total_fuel_capacity_kg = 
        Navcalc::USGALLONS_TO_LITERS * 
        Navcalc::JETA1_LITRES_TO_KILOS *
        ((int&)readblockB74[0x04] +  // center tank
         (int&)readblockB74[0x0c] +  // left main
         (int&)readblockB74[0x14] +  // left aux
         (int&)readblockB74[0x1c] +  // left tip
         (int&)readblockB74[0x24] +  // right main
         (int&)readblockB74[0x2C] +  // right aux
         (int&)readblockB74[0x34]);  // right tip
    
    m_flightstatus->zero_fuel_weight_kg = Navcalc::round((int&)readblock3000[0xbfc] * Navcalc::POUNDS_TO_KG / 256.0);
    m_flightstatus->total_weight_kg = Navcalc::round((double&)readblock3000[0xc0] * Navcalc::POUNDS_TO_KG);

    const uint& flaps_raw = (uint&)readblockB74[0x68];

    m_flightstatus->flaps_percent_left = (uint&)readblockB74[0x6c] / 163.83;
    m_flightstatus->flaps_percent_right = (uint&)readblockB74[0x70] / 163.83;

    m_flaps_inc_per_notch = (short&)readblock3000[0xbfa];
    
    if (m_flaps_inc_per_notch > 0)
    {
        m_flightstatus->current_flap_lever_notch = Navcalc::round(((double)flaps_raw) / m_flaps_inc_per_notch); 
        m_flightstatus->flaps_lever_notch_count = (16383 / m_flaps_inc_per_notch) + 1;
    }
    else
    {
        m_flightstatus->current_flap_lever_notch = 0;
        m_flightstatus->flaps_lever_notch_count = 1;
    }

    m_flightstatus->flaps_degrees = ((short&)readblock3000[0xf0]) / 256.0;
    m_flightstatus->slats_degrees = ((short&)readblock3000[0xf8]) / 256.0;

    //-----

    m_flightstatus->velocity_pitch_deg_s = Navcalc::toDeg((double&)readblock3000[0xa8]);
    m_flightstatus->velocity_roll_deg_s = Navcalc::toDeg((double&)readblock3000[0xb0]);
    m_flightstatus->velocity_yaw_deg_s = Navcalc::toDeg((double&)readblock3000[0xb8]);

    m_flightstatus->acceleration_pitch_deg_s2 = Navcalc::toDeg((double&)readblock3000[0x78]);
    m_flightstatus->acceleration_roll_deg_s2 = Navcalc::toDeg((double&)readblock3000[0x80]);
    m_flightstatus->acceleration_yaw_deg_s2 = Navcalc::toDeg((double&)readblock3000[0x88]);

    m_flightstatus->brake_left_percent = (short&)readblockB74[0x50] / 163.83;
    m_flightstatus->brake_right_percent = (short&)readblockB74[0x52] / 163.83;
    m_flightstatus->parking_brake_set = (short&)readblockB74[0x54] > 0;

    m_flightstatus->spoilers_armed = (int&)readblockB74[0x58] > 0;
    m_flightstatus->spoiler_lever_percent = (int&)readblockB74[0x5c] / 163.83;
    m_flightstatus->spoiler_left_percent = (int&)readblockB74[0x60] / 163.83;
    m_flightstatus->spoiler_right_percent = (int&)readblockB74[0x64] / 163.83;

    m_flightstatus->lights_navigation = (short&)readblockD0C[0] & 1;
    m_flightstatus->lights_beacon = (short&)readblockD0C[0]     & 2;
    m_flightstatus->lights_landing = (short&)readblockD0C[0]    & 4;
    m_flightstatus->lights_taxi = (short&)readblockD0C[0]       & 8;
    m_flightstatus->lights_strobe = (short&)readblockD0C[0]     & 16;
    m_flightstatus->lights_instruments = (short&)readblockD0C[0] & 32;

    m_flightstatus->gear_nose_position_percent = (int&)readblockB74[0x74] / 163.83;
    m_flightstatus->gear_left_position_percent = (int&)readblockB74[0x78] / 163.83;
    m_flightstatus->gear_right_position_percent = (int&)readblockB74[0x7C] / 163.83;

    m_flightstatus->delta = (double&)readblock281C[0xcc];
    m_flightstatus->theta = (double&)readblock281C[0xc4];
    m_flightstatus->sat = ((double&)readblock281C[0xb4] - 32.0) * 5.0/9.0;
    
    m_flightstatus->battery_on = ((int&)readblock281C[0x0] == 1) || ((char&)readblock3000[0x364] != 0);
    m_flightstatus->avionics_on = ((int&)readblock2E80[0x0] == 1) || ((char&)readblock3000[0x364] != 0);

    m_flightstatus->elevator_trim_degrees = Navcalc::toDeg((double&)readblock2E80[0x20]);

    m_flightstatus->fd_active = (int&)readblock2E80[0x60] == 1;
    m_flightstatus->setFlightDirectorPitchExternal((double&)readblock2E80[0x68]);
    m_flightstatus->setFlightDirectorBankExternal((double&)readblock2E80[0x70]);

    m_flightstatus->stall = (char&)readblock23B[0x131] != 0;

    m_flightstatus->doors_open = (char&)readblock3000[0x367];
    m_flightstatus->pitot_heat_on = (char&)readblock23B[0x61] != 0;
    m_flightstatus->pushback_status = (int&)readblock3000[0x1f4];

//TODO
//     switch(time_of_day)
//     {
//         case(2): m_flightstatus->time_of_day = FlightStatus::TIME_OF_DAY_DUSK_OR_DAWN; break;
//         case(4): m_flightstatus->time_of_day = FlightStatus::TIME_OF_DAY_NIGHT; break;
//         default: m_flightstatus->time_of_day = FlightStatus::TIME_OF_DAY_DAY; break;
//     }

    //----- FSX

    if (fs_version >= 8)
    {
        m_flightstatus->no_smoking_sign = (char&)readblock3000[0x41C];
        m_flightstatus->seat_belt_sign = (char&)readblock3000[0x41D];
    }

    //-----

    if (m_mode == MODE_MASTER)
    {
        long test = 0;
        for(int index=0; index < FSCTRL_SIZE; ++index) test += fs_control[index];
        if (test != 0) m_fsuipc->write(FSCTRL_OFFSET, FSCTRL_SIZE, fs_control_zero);
    }

    if (m_flightstatus->paused && !was_paused)      setupRefreshTimer(true);
    else if (!m_flightstatus->paused && was_paused) setupRefreshTimer(false);

    m_flightstatus->recalcAndSetValid();

//     if (m_refresh_analyse_timer.elapsed() > 20)
//         Logger::log(QString("FSAccessMsFs:slotRequestData: request duration = %1ms").arg(m_refresh_analyse_timer.elapsed()));
//    m_refresh_analyse_timer.start();
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setAutothrustArm(bool armed)
{
    if (!armed)
    {
        int not_locked = 0;
        m_fsuipc->write(0x7dc, 4, &not_locked);
        m_fsuipc->write(0x7e4, 4, &not_locked);
    }

    int arm = armed;
    m_fsuipc->write(0x810, 4, &arm);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setAutothrustSpeedHold(bool on)
{
    int set = on ? 1 : 0;
    m_fsuipc->write(0x7dc, 4, &set);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setAutothrustMachHold(bool on)
{
    int set = on ? 1 : 0;
    m_fsuipc->write(0x7e4, 4, &set);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setNavFrequency(int freq, uint nav_index)
{
    MYASSERT(nav_index <= 1);

    short bcd_freq = convertIntToBCD((freq % 100000) / 10);

    m_fsuipc->write(0x350 + (2 * nav_index), 2, &bcd_freq);
    char vor_rescan = 2;
    m_fsuipc->write(0x388, 1, &vor_rescan);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setAdfFrequency(int freq, uint adf_index)
{
    MYASSERT(adf_index <= 1);

    short bcd_freq1 =  convertIntToBCD((freq % 1000000) / 1000);
    short bcd_freq2 = (convertIntToBCD((freq / 1000000)) << 8) + convertIntToBCD((freq % 1000) / 100);

    if (adf_index == 0)
    {
        m_fsuipc->write(0x34c, 2, &bcd_freq1);
        m_fsuipc->write(0x356, 2, &bcd_freq2);
    }
    else
    {
        m_fsuipc->write(0x2d4, 2, &bcd_freq1);
        m_fsuipc->write(0x2d6, 2, &bcd_freq2);
    }

    char ndb_rescan = 2;
    m_fsuipc->write(0x389, 1, &ndb_rescan);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setNavOBS(int degrees, uint nav_index)
{
    MYASSERT(nav_index <= 1);
    degrees = Navcalc::trimHeading(degrees);
    m_fsuipc->write(0xC4E + (0x10 * nav_index), 2, &degrees);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setFDOnOff(bool on)
{
    Logger::log(QString("FSAccessMsFs:setFDOnOff: val=%1").arg(on));
    int value = on ? 1 : 0;
    m_fsuipc->write(0x2ee0, 4, &value);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setAPOnOff(bool on)
{
    int value = on ? 1 : 0;
    m_fsuipc->write(0x7bc, 4, &value);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setAPHeading(double hdg)
{
    hdg = Navcalc::trimHeading(hdg);
    unsigned short fs_hdg = (int) ((hdg * (65536.0/360.0)) + 0.5);
    m_fsuipc->write(0x7CC, 2, &fs_hdg);
    m_flightstatus->setAPHdgInternal(Navcalc::round(Navcalc::trimHeading(hdg)));
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setAPVs(int vs_ft_min)
{
    if (vs_ft_min < -6000) vs_ft_min = -6000;
    if (vs_ft_min > 6000) vs_ft_min = 6000;
    vs_ft_min = vs_ft_min / 100 * 100;
    m_fsuipc->write(0x7f2, 2, &vs_ft_min);
    m_flightstatus->setAPVsInternal(vs_ft_min);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setAPAlt(unsigned int alt)
{
    if (alt > 99999) alt = 99999;
    unsigned int fs_alt = Navcalc::round(alt * 65535.0 * Navcalc::FEET_TO_METER);
    m_fsuipc->write(0x7d4, 4, &fs_alt);
    m_flightstatus->setAPAltInternal(alt);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setAPAirspeed(unsigned short speed_kts)
{
    m_flightstatus->setAPSpdInternal(speed_kts);
    m_fsuipc->write(0x7e2, 2, &speed_kts);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setAPMach(double mach)
{
    int mach_int = Navcalc::round(qMax(0.0, mach) * 65535.0);
    m_flightstatus->setAPMachInternal(mach);
    m_fsuipc->write(0x7e8, 4, &mach_int);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setAPHeadingHold(bool on)
{
    int value = on ? 1:0;
    m_fsuipc->write(0x7c8, 2, &value);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setAPAltHold(bool on)
{
    int value = on ? 1:0;
    m_fsuipc->write(0x7d0, 4, &value);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setUTCTime(const QTime& utc_time)
{
    Logger::log(QString("FSAccessMsFs::setUTCTime: %1").arg(utc_time.toString()));
    char utc_hour = utc_time.hour();
    char utc_minute = utc_time.minute();
    m_fsuipc->write(0x23B, 1, &utc_hour);
    m_fsuipc->write(0x23C, 1, &utc_minute);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setUTCDate(const QDate& utc_date)
{
    Logger::log(QString("FSAccessMsFs::setUTCDate: %1").arg(utc_date.toString()));
    short utc_year = utc_date.year();
    short utc_day_nr = utc_date.dayOfYear();
    m_fsuipc->write(0x23E, 2, &utc_day_nr);
    m_fsuipc->write(0x240, 2, &utc_year);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::freeThrottleAxes()
{
    if (m_axes_disco_310a & 8) m_axes_disco_310a -= 8;
    m_fsuipc->write(0x310a, 2, &m_axes_disco_310a);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setThrottle(double percent)
{
    percent = qMax(0.0, qMin(100.0, percent));
    short fs_percent = Navcalc::round(16384 * percent / 100.0);;

    // block all throttle axes
    if ((m_axes_disco_310a & 8) == 0) m_axes_disco_310a += 8;
    m_fsuipc->write(0x310a, 2, &m_axes_disco_310a);
    
    m_fsuipc->write(0x88C, 2, &fs_percent);
    m_fsuipc->write(0x924, 2, &fs_percent);
    m_fsuipc->write(0x9bc, 2, &fs_percent);
    m_fsuipc->write(0xa54, 2, &fs_percent);

    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setSBoxTransponder(bool on)
{
    short value = (on ? 0 : 1);
    m_fsuipc->write(0x7b91, 2, &value);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setSBoxIdent()
{
    short value = 1;
    m_fsuipc->write(0x7b93, 2, &value);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setFlaps(uint notch)
{
    if (notch >= m_flightstatus->flaps_lever_notch_count) return false;
    int value = m_flaps_inc_per_notch * notch;
    m_fsuipc->write(0xbdc, 4, &value);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setAileron(double percent)
{
    percent = LIMIT(percent, 100.0);
    short fs_percent = Navcalc::round(16384 * percent / 100.0);

    // block aileron axis
    if ((m_axes_disco_310a & 2) == 0) m_axes_disco_310a += 2;
    m_fsuipc->write(0x310a, 2, &m_axes_disco_310a);
    
    m_fsuipc->write(0xbb6, 2, &fs_percent);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setElevator(double percent)
{
    percent = LIMIT(percent, 100.0);
    short fs_percent = Navcalc::round(16384 * percent / 100.0);

    // block aileron axis
    if ((m_axes_disco_310a & 1) == 0) m_axes_disco_310a += 1;
    m_fsuipc->write(0x310a, 2, &m_axes_disco_310a);

    m_fsuipc->write(0xbb2, 2, &fs_percent);

    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::freeControlAxes()
{
    if (m_axes_disco_310a & 1) m_axes_disco_310a -= 1;
    if (m_axes_disco_310a & 2) m_axes_disco_310a -= 2;
    m_fsuipc->write(0x310a, 2, &m_axes_disco_310a);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::freeAileronAxis()
{
    if (m_axes_disco_310a & 2) m_axes_disco_310a -= 2;
    m_fsuipc->write(0x310a, 2, &m_axes_disco_310a);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::freeElevatorAxis()
{
    if (m_axes_disco_310a & 1) m_axes_disco_310a -= 1;
    m_fsuipc->write(0x310a, 2, &m_axes_disco_310a);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setElevatorTrimPercent(double percent)
{
    percent = qMax(-100.0, qMin(100.0, percent));
    short value = (int) (16383.0 * percent / 100.0);
    m_fsuipc->write(0xbc0, 2, &value);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setElevatorTrimDegrees(double degrees)
{
    double rad = Navcalc::toRad(degrees);
    m_fsuipc->write(0x2ea0, 8, &rad);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::freeSpoilerAxis()
{
    if (m_axes_disco_341a & 4) m_axes_disco_341a -= 8;
    m_fsuipc->write(0x341a, 1, &m_axes_disco_341a);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setSpoiler(double percent)
{
    percent = LIMIT(percent, 100.0);

    short fs_percent = 4800 + Navcalc::round((16383-4800) * percent / 100.0);
    if (percent <= 1.0) fs_percent = 0;

    // block spoiler axis
    if ((m_axes_disco_341a & 4) == 0) m_axes_disco_341a += 8;
    m_fsuipc->write(0x341a, 1, &m_axes_disco_341a);
    
    m_fsuipc->write(0xbd0, 2, &fs_percent);

    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setNAV1Arm(bool on)
{
    int value = on ? 1 : 0;
    m_fsuipc->write(0x7c4, 4, &value);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setAPPArm(bool on)
{
    int value = on ? 1 : 0;
    m_fsuipc->write(0x7FC, 4, &value);
    if (!on || m_flightstatus->ap_gs_lock) m_fsuipc->write(0x800, 4, &value);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setAltimeterHpa(const double& hpa)
{
    short value = Navcalc::round(hpa * 16.0);
    m_fsuipc->write(0x330, 2, &value);
    m_flightstatus->setAltPressureSettingHpaInternal(hpa);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::checkLink()
{
    if (m_fsuipc->isLinkOk()) return true;
    m_fsuipc->openLink();
    return false;
}

/////////////////////////////////////////////////////////////////////////////

void FSAccessMsFs::setupRefreshTimer(bool pause_mode)
{
    if (pause_mode) 
    {
        Logger::log("FSAccessMsFs:setupRefreshTimer: timer set to 1000ms");
        m_refresh_timer.start(1000);
    }
    else            
    {
        Logger::log(QString("FSAccessMsFs:setupRefreshTimer: timer set to %1ms").
                    arg(m_cfg.getIntValue(CFG_MSFS_REFRESH_PERIOD_MS)));
        m_refresh_timer.start(m_cfg.getIntValue(CFG_MSFS_REFRESH_PERIOD_MS));
    }
}

/////////////////////////////////////////////////////////////////////////////

bool FSAccessMsFs::setPushback(PushBack status)
{
    int value = status;
    m_fsuipc->write(0x31f4, 4, &value);
    return true;
}

/////////////////////////////////////////////////////////////////////////////

void FSAccessMsFs::writeFMCStatusToSim(const FMCStatusData& fmc_status_data)
{
    //----- 1st byte

    unsigned char value = BitHandling::getByteFromBits(fmc_status_data.fd_engaged,
                                                       fmc_status_data.ap_engaged,
                                                       fmc_status_data.athr_armed,
                                                       fmc_status_data.athr_engaged,
                                                       false,
                                                       false,
                                                       false,
                                                       false);
    m_fsuipc->write(FMCSTATUS_OFFSET, 1, &value);

    //----- 2nd byte

    value = BitHandling::getByteFromBits(fmc_status_data.athr_speed_mode,
                                         fmc_status_data.athr_mach_mode,
                                         fmc_status_data.athr_n1_mode,
                                         false,
                                         false,
                                         false,
                                         false,
                                         false);

    m_fsuipc->write(FMCSTATUS_OFFSET+1, 1, &value);

    //----- 3rd byte

    value = BitHandling::getByteFromBits(fmc_status_data.ap_horiz_hdg_mode,
                                         fmc_status_data.ap_horiz_lnav_mode,
                                         fmc_status_data.ap_horiz_loc_mode,
                                         false,
                                         false,
                                         false,
                                         false,
                                         false);

    m_fsuipc->write(FMCSTATUS_OFFSET+2, 1, &value);

    //----- 4th byte

    value = BitHandling::getByteFromBits(fmc_status_data.ap_vert_vs_mode,
                                         fmc_status_data.ap_vert_flch_mode,
                                         fmc_status_data.ap_vert_vnav_mode,
                                         fmc_status_data.ap_vert_alt_hold,
                                         fmc_status_data.ap_both_app_mode,
                                         fmc_status_data.ap_vert_fpa_mode,
                                         false,
                                         false);

    m_fsuipc->write(FMCSTATUS_OFFSET+3, 1, &value);
}

/////////////////////////////////////////////////////////////////////////////
