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
#include "fsaccess_fgfs_base.h"
#include "flightstatus.h"
#include "fsaccess_fgfs_flightstatusmodel.h"
#include <QModelIndex>

/*! The source of string "name"->int id.
 * still think of a better albeit not slower way to pull that in.
 */
const QStringList VasFlightStatusModel::names=(QStringList()
  << "ias"               //index 0
  << "tas"
  << "barber_pole"
  << "alt"
  << "ground_alt"
  << "vs"
  << "lat"
  << "lon"
  << "pitch"
  << "bank"
  << "true_heading"	  //index 10
  << "magvar"
  << "wind_speed_kts"
  << "wind_dir_deg_true" 
  << "view_dir_deg"
  << "paused"
  << "fs_utc_dtg"
  << "slipskid" 
  << "tat"
  << "sat"
  << "oat-degc"               //index 20
  << "dewpoint-degc"
  << "qnh-hpa"
  << "delta"
  << "theta"
  << "onground"
  << "ap_available"
  << "ap_disabled"        // beware !
  << "ap_hdg_lock"
  << "ap_hdg"
  << "ap_alt_lock"       //index 30
  << "ap_alt"
  << "ap_vs_lock"
  << "ap_vs"            
  << "ap_ias"
  << "ap_mach"
  << "fd_available"
  << "fd_pitch"
  << "fd_bank"
  << "navaid_freq-mul1000"       //avoid redundant code, for those consider
  << "navaid_radial-deg"         //further parsing of the protocol file  //index 40
  << "navaid_dme_valid"          //sort of generic has-dme && dme-inrange
  << "navaid_hasloc"
  << "navaid_hasgs"              //probably unneeded
  << "navaid_inrange"            //unneeded for now
  << "navaid_id"
  << "navaid_heading-deg"
  << "navaid_hneedle_defl"
  << "navaid_fromflag"   
  << "navaid_toflag"
  << "navaid_gsneedle_defl" //index 50
  << "extfmcvoltage"
  << "avionics_switch"
  << "n1"
  << "rpm"
  << "egt-degc"             //damned, no ootb converison in protocol
  << "n2"
  << "ff-kgph"
  << "antiice"
  << "lights_landing"
  << "lights_strobe"        //index 60
  << "lights_beacon"
  << "gear_nose"
  << "ground_speed-kts"
  << "zero_fuel_weight-kg"
  << "total_weight-kg"
  << "flaps-deg"
  << "flaps-incpernotch"
  << "flaps-inc"
  << "power_set"
  << "lights_taxi"          // index 70
  << "lights_nav"
  << "spoiler_lever_percent"
  << "spoiler_percent"
  << "navaid_dme_range-nm"					       
  << "aircraft_type"
);


/*! The only to-be-used ctor.
 * more or less c&p from Qt doc
 */
VasFlightStatusModel::VasFlightStatusModel(FlightStatus* status, //!< pointer to the interesting stuff
					   QObject* parent       //!< follow suit
					   ) 
  : QAbstractItemModel(parent),
    m_flightstatus(status),
    m_rootitem(new VasFlightStatusItem()), 
    nav1_tofrom(new VorFlagBuffer(status->obs1_to_from)),
    nav2_tofrom(new VorFlagBuffer(status->obs2_to_from)),
    dme1(new DmeBuffer(status->nav1_distance_nm)),
    dme2(new DmeBuffer(status->nav2_distance_nm))
{
  setupModelData(m_rootitem);
};
#if 0
VasFlightStatusModel& VasFlightStatusModel::operator= (const VasFlightStatusModel& statusmodel) {
  FlightStatus* tmp = new FlightStatus(*statusmodel.m_flightstatus);
  delete m_flightstatus;
  m_flightstatus=tmp;
  return *this;
}
#endif

/*!nothing special here.
 * only cleanup the stuff we introduced
 */
VasFlightStatusModel::~VasFlightStatusModel() { 
  delete m_flightstatus; 
  delete m_rootitem;
  delete nav1_tofrom;
  delete nav2_tofrom;
};
/*!needed for mvc.
 * not optimized in any way. think about caching or so
 */
int VasFlightStatusModel::rowCount(const QModelIndex &parent) const {
  VasFlightStatusItem* parentItem;
  if (parent.column() > 0)
    return 0;
  if (!parent.isValid())
    parentItem = m_rootitem;
  else
    parentItem = static_cast<VasFlightStatusItem*>(parent.internalPointer());
  
  return parentItem->childCount();

}
int VasFlightStatusModel::columnCount(const QModelIndex& parent) const {
  if (parent.isValid())
    return static_cast<VasFlightStatusItem*>(parent.internalPointer())->columnCount(); //internal pointer??
  else
    return m_rootitem->columnCount();
}
QModelIndex VasFlightStatusModel::parent(const QModelIndex& index) const {
  if (!index.isValid())
    return QModelIndex();

  VasFlightStatusItem *childItem = static_cast<VasFlightStatusItem*>(index.internalPointer());
  VasFlightStatusItem *parentItem = childItem->parent();

  if (parentItem == m_rootitem)
         return QModelIndex();

  return createIndex(parentItem->row(), 0, parentItem);
}
QModelIndex VasFlightStatusModel::index(int row, int col, const QModelIndex& parent) const {
   if (!hasIndex(row, col, parent))
    return QModelIndex();
  
  VasFlightStatusItem *parentItem;
  
  if (!parent.isValid())
    parentItem = m_rootitem;
  else
    parentItem = static_cast<VasFlightStatusItem*>(parent.internalPointer());
  
  VasFlightStatusItem *childItem = parentItem->child(row);
  if (childItem)
    return createIndex(row, col, childItem);
  else
    return QModelIndex(); 
}
QVariant VasFlightStatusModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid())
    return QVariant();
  
  if (role == Qt::ToolTipRole) {
    return names.at(index.row());
  }
  if (role == Qt::DisplayRole) {
    switch ( index.parent().row()) {
    case 39: //navaid_freq
      switch (index.row()) {
      case 0: 
	return m_flightstatus->nav1_freq/1000.0;
	break;
      case 1: 
	return m_flightstatus->nav2_freq/1000.0;
	break;
      case 2: 
	return m_flightstatus->adf1.freqString();
	break;
      case 3: 
	return m_flightstatus->adf2.freqString();
	break;
      }
      break;
    case 40: //navaid_radial 
      switch (index.row()) {
      case 0: 
	return m_flightstatus->obs1;
	break;
      case 1: 
	return m_flightstatus->obs2;
	break;
      }
      break;
    case 41: //navaid_dme_valid (see 74)
      switch (index.row()) {
      case 0:
	return QVariant();
	break;
      case 1:
	return QVariant();
	break;
      }
    case 42: //navaid_hasloc
      switch (index.row()) {
      case 0:
	return m_flightstatus->nav1_has_loc;
	break;
      case 1:
	return m_flightstatus->nav2_has_loc;
	break;
      }
    case 43: //navaid_hasgs 
      switch (index.row()) {
      case 0: 
	return QVariant();
	break;
      case 1: 
	return QVariant();
	break;
      }
    case 45: //navaid_id
      switch (index.row()) {
      case 0: 
	return m_flightstatus->nav1.id();
	break;
      case 1: 
	return m_flightstatus->nav2.id();
	break;
      case 2: 
	return m_flightstatus->adf1.id();
	break;
      case 3: 
	return m_flightstatus->adf2.id();
	break;
      }
      break;
    case 46: //navaid_heading (despite name)
      switch (index.row()) {
      case 0: 
	return m_flightstatus->nav1_bearing.value();
	break;
      case 1: 
	return m_flightstatus->nav2_bearing.value();
	break;
      case 2: 
	return m_flightstatus->adf1_bearing.value();
	break;
      case 3: 
	return m_flightstatus->adf2_bearing.value();
	break;
      }
      break; 
    case 47: //navaid_hneedle_defl
      switch (index.row()) {
      case 0: 
	return m_flightstatus->obs1_loc_needle;
	break;
      case 1: //not used yet
	return m_flightstatus->obs2_loc_needle;
	break;
      }
      break;
    case 48: //navaid_fromflag
    case 49: //navaid_toflag 
      // in vas it's only one property, but to keep the mvc consistent
      switch (index.row()) {
      case 0: 
	return m_flightstatus->obs1_to_from;
	break;
      case 1: //not used yet
	return m_flightstatus->obs2_to_from;
	break;
      }
      break;
    case 50: //navaid_gsneedle_defl
      switch (index.row()) {
      case 0:
	return m_flightstatus->obs1_gs_needle;
	break;
      case 1: //hmhm not used now
	return m_flightstatus->obs2_gs_needle;
	break;
      }
      break;
    case 53:
    case 54:
      switch (index.row()) {
      case 0: 
	return m_flightstatus->smoothedN1_1();
	break;
      case 1: 
	return m_flightstatus->smoothedN1_2();
	break;
      case 2: 
	return m_flightstatus->smoothedN1_3();
	break;
      case 3: 
	return m_flightstatus->smoothedN1_4();
	break;
      }
    case 55:
      switch (index.row()) {
      case 0: 
	return m_flightstatus->egt_1;
	break;
      case 1: 
	return m_flightstatus->egt_2;
	break;
      case 2: 
	return m_flightstatus->egt_3;
	break;
      case 3: 
	return m_flightstatus->egt_4;
	break;
     
      }
      break;
    case 57:
      switch (index.row()) {
      case 0:
	return m_flightstatus->ff_1;
        break;
      case 1: 
        return m_flightstatus->ff_2;
        break;
      case 2: 
        return m_flightstatus->ff_3;
	break;
      case 3: 
        return m_flightstatus->ff_4;
        break;
      }    
    case 56: 
      switch (index.row()) {
      case 0:
	return m_flightstatus->n2_1;
        break;
      case 1: 
        return m_flightstatus->n2_2;
        break;
      case 2: 
        return m_flightstatus->n2_3;
	break;
      case 3: 
        return m_flightstatus->n2_4;
        break;
      }    
      break;
    case 58: 
      switch (index.row()) {
      case 0:
	return m_flightstatus->engine_anti_ice_1;
        break;
      case 1: 
        return m_flightstatus->engine_anti_ice_2;
        break;
      case 2: 
        return m_flightstatus->engine_anti_ice_3;
	break;
      case 3: 
        return m_flightstatus->engine_anti_ice_4;
        break;
      }    
      break;
    case 69:
      switch (index.row()) {
      case 0: 
	return m_flightstatus->throttle_1_percent;
	break;
      case 1: 
	return m_flightstatus->throttle_2_percent;
	break;
      case 2: 
	return m_flightstatus->throttle_3_percent;
	break;
      case 3: 
	return m_flightstatus->throttle_4_percent;
	break;
      }
      break;
    case 74:
      switch (index.row()) {
      case 0: 
	return m_flightstatus->nav1_distance_nm;
	break;
      case 1: 
	return m_flightstatus->nav2_distance_nm;
	break;
      }
    default:
      switch (index.row()) {
      case 0: //ias B (not used)??
	return m_flightstatus->smoothedIAS();
	break;
      case 1:  //tas B
	return m_flightstatus->tas;
	break;
      case 2: // barber_pole (Vne/mo)
	return m_flightstatus->barber_pole_speed;
	break;
      case 3:  //alt A
	//return m_flightstatus->alt_ft; //divined ideal value
	return m_flightstatus->smoothedAltimeterReadout(); //shall go away?
	break;
      case 4:  //ground_alt 
	return m_flightstatus->ground_alt_ft;
	break;
      case 5:  //vs A
	return m_flightstatus->smoothedVS();
	break;
      case 6:  //lat B
	return m_flightstatus->lat;
	break;
      case 7:  //lon B
	return m_flightstatus->lon;
	break;
      case 8:  //pitch
	return m_flightstatus->smoothedPitch();
	break;
      case 9:  //bank
	return m_flightstatus->smoothedBank();
	break;
      case 10:  //true_heading B gnarf only accessor
	return m_flightstatus->smoothedTrueHeading();
	break;
      case 11:  //magvar A
	return m_flightstatus->magvar;
	break;
      case 12:  //wind_speed_kts A
	return m_flightstatus->wind_speed_kts;
	break;
      case 13:  //wind_dir_deg_true A
	return m_flightstatus->wind_dir_deg_true;
	break;
      case 14:  //view_dir A (not used anywhere)
	return QVariant();
	break;
      case 15: //paused A
	return m_flightstatus->paused;
	break;
      case 16:  //fs_utc_time A  2007-03-21T00:07:57
	return QDateTime(m_flightstatus->fs_utc_date, m_flightstatus->fs_utc_time, Qt::UTC);
	break;
      case 17:  //slipskid
	return m_flightstatus->slip;
	break;
      case 18:  //tat A (not used anywhere)
	return QVariant();
	break;
      case 19: //sat A (not used anywhere)
	return QVariant();
	break;
      case 20:  //oat A 
	return m_flightstatus->oat;
	break;
      case 21:  //dew A (not used anywhere)
	return m_flightstatus->dew;
	break;
      case 22:  //qnh A
	return m_flightstatus->AltPressureSettingHpa();
	break;
      case 23:  //delta and theta A for alt_density which in turn (not used)
	return QVariant();
	break;
      case 24:  //theta and theta A,  see above
	return QVariant();
	break;
      case 25: //onground A
	return m_flightstatus->onground;
	break;
      case 26:  //ap_available A (not used anywhere)
	return QVariant();
	break;
      case 27:  //ap_disabled A
	return m_flightstatus->ap_enabled;
	break;
      case 28:  //ap_hdg_lock A (not used anywhere)
	return QVariant();
	break;
      case 29: //ap_hdg A
	return m_flightstatus->APHdg();
	break;
      case 30:  //ap_alt_lock A (not used anywhere)
	return QVariant();
	break;
      case 31: //ap_alt A
	return m_flightstatus->APAlt();
	break;
      case 32:  //ap_vs_lock A (not used anywhere)
	return QVariant();
	break;
      case 33: //ap_vs A 
	return m_flightstatus->APVs();
	break;
      case 34: //ap_ias 
	return m_flightstatus->APSpd();
	break;
      case 35: //ap_mach  
	return m_flightstatus->APMach();
	break;
	// ap_ias_lock ap_mach_lock
      case 36: //fd_available
	return m_flightstatus->fd_active;
	break;
      case 37: //fd_pitch 
	return m_flightstatus->smoothedFlightDirectorPitch();
	break;
      case 38: //fd_bank
	return m_flightstatus->smoothedFlightDirectorBank();
	break;
      case 51: //extfmcvoltage
	return m_flightstatus->battery_on;
	break;
      case 52: //avionics_switch
	return m_flightstatus->avionics_on;
	break;
      case 58: //antiice
	return m_flightstatus->engine_anti_ice_1;
	break;
      case 59: //lights_landing
	return m_flightstatus->lights_landing;
	break;
      case 60: //lights_strobe
	return m_flightstatus->lights_strobe;
	break;
      case 61: //lights_beacon
	return m_flightstatus->lights_beacon;
	break;
      case 62: //gear_down
	// for now use it for all three gears
	return m_flightstatus->isGearDown();
	//return m_flightstatus->gear_left_position_percent;
	//return m_flightstatus->gear_right_position_percent;
	break;
      case 63: //ground_speed_kts
	return m_flightstatus->ground_speed_kts;
	break;
      case 64: //actually 65-64 for no other reason
      case 65:
	return m_flightstatus->fuelOnBoard();
	break;    
      case 66:
	return m_flightstatus->flaps_degrees;
	break;   
      case 67:
	return m_flightstatus->flaps_inc_per_notch;
        break;
      case 68:
	return m_flightstatus->flaps_raw;
        break;
      case 70:
	return m_flightstatus->lights_taxi;
	break;
      case 71:
	return m_flightstatus->lights_navigation;
	break;
      case 72:
	return m_flightstatus->spoiler_lever_percent;
	break;
      case 73:
	return m_flightstatus->spoiler_left_percent;
	//return m_flightstatus->spoiler_right_percent;
	break;
      case 75: //aircraft_type
	return m_flightstatus->aircraft_type;
	break;
      }
    }
    return QVariant(); //should-not-happen default  
  }
  return QVariant(); //should-not-happen default (really *g*)
};
Qt::ItemFlags VasFlightStatusModel::flags(const QModelIndex &index) const {
  if (!index.isValid()) {
    //DEBUGP(LOGWARNING, "NOT valid:" <<index.row());
    return 0; //Qt::ItemIsEnabled;
    }
  //DEBUGP(LOGWARNING, "valid:" << index.row());
  
  return QAbstractItemModel::flags(index) | (index.row()==3 ? Qt::ItemIsEditable : QFlags<Qt::ItemFlag>(0));
}
bool VasFlightStatusModel::setData(const QModelIndex &index, const QVariant &value, int role) {
  if ( index.isValid() && role == Qt::EditRole ) {
    switch ( index.parent().row()) {
    case 39: //navaid_freq
      switch (index.row()) {
      case 0: 
	m_flightstatus->nav1_freq=value.toInt();
	break;
      case 1: 
	m_flightstatus->nav2_freq=value.toInt();
	break;
      case 2: 
	m_flightstatus->adf1.setFreq(value.toInt());
	break;
      case 3: 
	m_flightstatus->adf2.setFreq(value.toInt());
	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    case 40: //navaid_radial 
      switch (index.row()) {
      case 0: 
	m_flightstatus->obs1=value.toInt();
	break;
      case 1: 
	m_flightstatus->obs2=value.toInt();
	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
   case 41: //navaid_dme_valid
      switch (index.row()) {
      case 0: 
	if (!dme1->recvValid(translateBool(value))) return true; //no update yet, so no signal
	break;
      case 1: //not used yet
	if (!dme2->recvValid(translateBool(value))) return true;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    case 42: //navaid_hasloc
      switch (index.row()) {
      case 0:
	m_flightstatus->nav1_has_loc=translateBool(value);
	break;
      case 1:
	m_flightstatus->nav2_has_loc=translateBool(value);
	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    case 43: //navaid_hasgs
      switch (index.row()) {
      case 0: 
	//m_flightstatus->obs1=value.toInt();
	break;
      case 1: 
	//m_flightstatus->obs2=value.toInt();
	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    case 45: //navaid_id
      switch (index.row()) {
      case 0: 
	m_flightstatus->nav1.setId(value.toString());
	break;
      case 1: 
	m_flightstatus->nav2.setId(value.toString());
	break;
      case 2: 
	m_flightstatus->adf1.setId(value.toString());
	break;
      case 3: 
	m_flightstatus->adf2.setId(value.toString());
	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    case 46: //navaid_heading (or bearing for adf)
      switch (index.row()) {
      case 0: 
	m_flightstatus->nav1_bearing=value.toDouble()-m_flightstatus->smoothedTrueHeading();
	break;
      case 1: 
	m_flightstatus->nav2_bearing=value.toDouble()-m_flightstatus->smoothedTrueHeading();
	break;
      case 2: 
	m_flightstatus->adf1_bearing=value.toDouble();
	break;
      case 3: 
	m_flightstatus->adf2_bearing=value.toDouble();
	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    case 47: //navaid_hneedle_defl
      switch (index.row()) {
      case 0: 
	m_flightstatus->obs1_loc_needle=value.toDouble()*127.0/10.0;
	break;
      case 1: //not used yet
	m_flightstatus->obs2_loc_needle=value.toDouble()*127.0/10.0;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    //!bug  blech, horrible kludge I fear
    case 48: //navaid_fromflag
      switch (index.row()) {
      case 0: 
	if (!nav1_tofrom->recvFrom(translateBool(value))) return true; //no update yet, so no signal
	break;
      case 1: //not used yet
	if (!nav2_tofrom->recvFrom(translateBool(value))) return true;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    case 49: //navaid_toflag 
      switch (index.row()) {
      case 0: 
	if (!nav1_tofrom->recvTo(translateBool(value))) return true;
	break;
      case 1: //not used yet
	if (!nav2_tofrom->recvTo(translateBool(value))) return true;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    case 50: //navaid_gsneedle_defl
      // [-118,118] fmc_navdisplay_style_b l. 1582
      // 5*gs_error in fg, we expect a intelligible value on the wire
      // so factor out the 5 in the protocol, peg at 0.7 deg deflection,
      // whereas http://www.smartcockpit.com/site/pdf/download.php?file=plane/airbus/A320/systems/A320-Indicating_and_Recording_Systems.pdf
      // talks about +/- 0.4° per dot (p. 61). I'll stick to that for now.
      // 
      // fmc_navdisplay_style_a.cpp l. 1808
      // fmc_pfd_glwidget_style_a.cpp l. 999
      // construed as implying the full range of value travel between +/- 127
      // therefore: (further thinking of config parameteres/static constants pending...)
      switch (index.row()) {
      case 0:
	m_flightstatus->obs1_gs_needle=qBound(-127, qRound(value.toDouble()*-1*127.0/(2*0.40)), +127);
	break;
      case 1: //hmhm not used now
	m_flightstatus->obs2_gs_needle=qBound(-127, qRound(value.toDouble()*-1*127.0/(2*0.40)), +127);
	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    case 53:
      switch (index.row()) {
      case 0: 
	m_flightstatus->smoothed_n1_1=value.toDouble();
	break;
      case 1: 
	m_flightstatus->smoothed_n1_2=value.toDouble();
	break;
      case 2: 
	m_flightstatus->smoothed_n1_3=value.toDouble();
	break;
      case 3: 
	m_flightstatus->smoothed_n1_4=value.toDouble();
	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    case 54: //rpm arbitrarily scaled to 100% FIXME
      switch (index.row()) {
      case 0: 
	m_flightstatus->smoothed_n1_1=value.toDouble()*0.05714;
	break;
      case 1: 
	m_flightstatus->smoothed_n1_2=value.toDouble()*0.05714;
	break;
      case 2: 
	m_flightstatus->smoothed_n1_3=value.toDouble()*0.05714;
	break;
      case 3: 
	m_flightstatus->smoothed_n1_4=value.toDouble()*0.05714;
	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break; 
    case 55:
      switch (index.row()) {
      case 0: 
	m_flightstatus->egt_1=value.toDouble();
	break;
      case 1: 
	m_flightstatus->egt_2=value.toDouble();
	break;
      case 2: 
	m_flightstatus->egt_3=value.toDouble();
	break;
      case 3: 
	m_flightstatus->egt_4=value.toDouble();
	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    case 56:
      switch (index.row()) {
      case 0: 
	m_flightstatus->n2_1=value.toDouble();
	break;
      case 1: 
	m_flightstatus->n2_2=value.toDouble();
	break;
      case 2: 
	m_flightstatus->n2_3=value.toDouble();
	break;
      case 3: 
	m_flightstatus->n2_4=value.toDouble();
	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    case 57:
      switch (index.row()) {
      case 0: 
	m_flightstatus->ff_1=value.toDouble();
	break;
      case 1: 
	m_flightstatus->ff_2=value.toDouble();
	break;
      case 2: 
	m_flightstatus->ff_3=value.toDouble();
	break;
      case 3: 
	m_flightstatus->ff_4=value.toDouble();
	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    case 58:
      switch (index.row()) {
      case 0: 
	m_flightstatus->engine_anti_ice_1=translateBool(value);
	break;
      case 1: 
	m_flightstatus->engine_anti_ice_2=translateBool(value);
	break;
      case 2: 
	m_flightstatus->engine_anti_ice_3=translateBool(value);
	break;
      case 3: 
	m_flightstatus->engine_anti_ice_4=translateBool(value);
	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    case 69:
      switch (index.row()) {
      case 0: 
	m_flightstatus->throttle_1_percent=value.toDouble();
       	break;
      case 1: 
	m_flightstatus->throttle_2_percent=value.toDouble();
       	break;
      case 2: 
	m_flightstatus->throttle_3_percent=value.toDouble();
	break;
      case 3: 
	m_flightstatus->throttle_4_percent=value.toDouble();
	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;
    case 74:
      switch (index.row()) {
      case 0: 
	if (!dme1->recvDist(value.toDouble())) return true;
       	break;
      case 1: 
	if (!dme2->recvDist(value.toDouble())) return true;
       	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index);
      return true;
      break;

    default: //should be -1 perhaps?
      switch (index.row()) {
      case 0:
	m_flightstatus->smoothed_ias=value.toDouble();
	break;
      case 1:
	m_flightstatus->tas=value.toDouble();
	break;
      case 2:
	m_flightstatus->barber_pole_speed=value.toDouble();
	break;
      case 3:
	m_flightstatus->alt_ft=value.toDouble();
	m_flightstatus->altimeter_readout=value.toDouble(); //TODO find correct disambiguation
	m_flightstatus->smoothed_altimeter_readout=value.toDouble();
	break;  
      case 4:
	m_flightstatus->ground_alt_ft=value.toDouble();
	break;
      case 5:
	m_flightstatus->smoothed_vs=value.toDouble();
	break;
      case 6:
	m_flightstatus->lat=value.toDouble();
	break;
      case 7:
	m_flightstatus->lon=value.toDouble();
	break;
      case 8:
	m_flightstatus->pitch=value.toDouble();
	break;
      case 9:
	m_flightstatus->bank=value.toDouble();
	break;
      case 10:
	m_flightstatus->setTrueHeading(value.toDouble());
	break;
      case 11:
	m_flightstatus->magvar=value.toDouble();
	break;
      case 12:
	m_flightstatus->wind_speed_kts=value.toDouble();
	break;
      case 13:
	m_flightstatus->wind_dir_deg_true=value.toDouble();
	break;
      case 14: //view_dir A (not used anywhere)
	//m_flightstatus->view_dir_deg=value.toInt()
	break;
      case 15:
	m_flightstatus->paused=translateBool(value);
	break;
      case 16:
	m_flightstatus->fs_utc_time=QDateTime::fromString(value.toString(), "yyyy-MM-ddThh:mm:ss").time();
	m_flightstatus->fs_utc_date=QDateTime::fromString(value.toString(), "yyyy-MM-ddThh:mm:ss").date();
	break;
      case 17:
	m_flightstatus->slip=value.toInt();
	break;    
      case 18:  //tat A (not used anywhere)
	m_flightstatus->tat=value.toDouble();
	break;
      case 19: //sat A (not used anywhere)
	m_flightstatus->sat=value.toDouble();
	break;
      case 20:
	m_flightstatus->oat=value.toDouble();
	break;
      case 21:  
        m_flightstatus->dew=value.toDouble();
	break;
      case 22:  
	m_flightstatus->setAltPressureSettingHpaExternal(value.toDouble());
	break;
      case 23:  //delta and theta A for alt_density which in turn (not used)
	m_flightstatus->delta=value.toDouble();
	break;
      case 24:  //theta and theta A,  see above
	m_flightstatus->theta=value.toDouble();
	break;
      case 25: 
	m_flightstatus->onground=translateBool(value); //uh expensive?
	break; 
      case 26:  //ap_available A (not used anywhere)
	DEBUGP(LOGBULK, "ap_available");
	break;
      case 27:
	m_flightstatus->ap_enabled=!translateBool(value); //actually asks for NOT passive-mode, hmhm
	break;
      case 28:  //ap_hdg_lock A (not used anywhere)
	DEBUGP(LOGBULK, "ap_hdg_lock");
	break;
      case 29:
	m_flightstatus->setAPHdgExternal(value.toUInt());
	break;
      case 30:  //ap_alt_lock A (not used anywhere)
	DEBUGP(LOGBULK, "ap_alt_lock");
	break;
      case 31: 
	m_flightstatus->setAPAltExternal(value.toUInt());
	break;
      case 32:  //ap_vs_lock A (not used anywhere)
	DEBUGP(LOGBULK, "ap_vs_lock");
	break;
      case 33:
	m_flightstatus->setAPVsExternal(value.toInt());
	break;
      case 34:
	m_flightstatus->setAPSpdExternal(value.toUInt());
	break;
      case 35:
	m_flightstatus->setAPMachExternal(value.toDouble());
	break;
      case 36:
	m_flightstatus->fd_active=translateBool(value);
	break;
      case 37:
	m_flightstatus->fd_pitch=value.toDouble();
	break;
      case 38: 
	m_flightstatus->fd_bank=value.toDouble();
	break;

      case 51:
	m_flightstatus->battery_on=translateBool(value);
	break;
      case 52:
	m_flightstatus->avionics_on=translateBool(value);
	break;

      case 59:
	m_flightstatus->lights_landing=translateBool(value);
	break;
      case 60:
	m_flightstatus->lights_strobe=translateBool(value);
	break;
      case 61:
	m_flightstatus->lights_beacon=translateBool(value);
	break;
      case 62: //gear_nose
        // for now use it for all three gears
        m_flightstatus->gear_nose_position_percent=value.toDouble();
	m_flightstatus->gear_left_position_percent=value.toDouble();
	m_flightstatus->gear_right_position_percent=value.toDouble();
	break;
      case 63:
	m_flightstatus->ground_speed_kts=value.toDouble();
	break;
      case 64:
	m_flightstatus->zero_fuel_weight_kg=value.toUInt();
	break;
      case 65:
	m_flightstatus->total_weight_kg=value.toUInt();
	break;
      case 66:
	m_flightstatus->flaps_degrees=value.toUInt();
	break;
      case 67:
	m_flightstatus->flaps_inc_per_notch=value.toUInt();
        break; // for now ignore the totally bogus normalizing to 0-16383
      case 68:
	m_flightstatus->flaps_raw=value.toUInt();
        break;
      case 70:
	m_flightstatus->lights_taxi=translateBool(value);
	break;
      case 71:
	m_flightstatus->lights_navigation=translateBool(value);
	break;
      case 72:
	m_flightstatus->spoiler_lever_percent=value.toDouble();
	break;
      case 73:
	m_flightstatus->spoiler_left_percent=value.toDouble();
	m_flightstatus->spoiler_right_percent=value.toDouble();
	break;
      case 75:
	m_flightstatus->aircraft_type=value.toString();
	break;
      default:
	return false;
	break;
      }
      emit dataChanged(index, index); //sensible only if views are actually used, debug stuff
      return true;
      break;
    } 
  } 
  return false;
}

QVariant VasFlightStatusModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (role != Qt::DisplayRole)
    return QVariant();
  if (orientation == Qt::Horizontal)
    return QString("Column %1").arg(section);
  else
    return QString("Row %1").arg(section);
};

VasFlightStatusItem::VasFlightStatusItem(VasFlightStatusItem *parent)
  : m_parentItem(parent)
  {};
VasFlightStatusItem::~VasFlightStatusItem() {
qDeleteAll(m_childItems);
}

void VasFlightStatusItem::appendChild(VasFlightStatusItem *child) {
  m_childItems.append(child);
}
VasFlightStatusItem* VasFlightStatusItem::child(const int row) {
  return m_childItems.value(row);
}
VasFlightStatusItem* VasFlightStatusItem::parent() {
  return m_parentItem;
}
int VasFlightStatusItem::childCount() const {
  return m_childItems.count();
}
int VasFlightStatusItem::columnCount() const {
  return 1; //uh
}
int VasFlightStatusItem::row() const {
  if (m_parentItem)
    return m_parentItem->m_childItems.indexOf(const_cast<VasFlightStatusItem*>(this));
  return 0;
}

/*!crude hack (basically another[tm] internal, skeletal mapping of flightstatus to
 * make mvc happy. 
 * 
 * to be extended/mangled later on, perhaps, hmhmhm
 */
void VasFlightStatusModel::setupModelData(VasFlightStatusItem* parent) {
  
  QList<VasFlightStatusItem*> parents;
  parents << parent;
  int number=0;
  
  while (number<VasFlightStatusModel::names.size()) {
   
    parents.last()->appendChild(new VasFlightStatusItem(m_rootitem));
    if ( names.at(number)=="ff-kgph" 
	 or names.at(number)=="n2"
	 or names.at(number)=="n1"
	 or names.at(number)=="egt-degc"
	 or names.at(number)=="rpm"
	 or names.at(number)=="navaid_freq-mul1000" //uh
	 or names.at(number)=="navaid_id"
	 or names.at(number)=="navaid_heading-deg"
	 or names.at(number)=="power_set"
	 or names.at(number)=="antiice"
	 ) {
      int i;
      for (i=0; i<4; i++) {
	parents.last()->child(parents.last()->childCount()-1)
	  ->appendChild(new VasFlightStatusItem(parents.last()->child(parents.last()->childCount()-1)));
      }
    }
    if ( names.at(number)=="navaid_radial-deg" 
	 or names.at(number)=="navaid_hneedle_defl"
	 or names.at(number)=="navaid_fromflag"
	 or names.at(number)=="navaid_toflag"
	 or names.at(number)=="navaid_hasloc"
	 or names.at(number)=="navaid_gsneedle_defl"
	 or names.at(number)=="navaid_hasgs"
	 or names.at(number)=="navaid_dme_valid"
	 or names.at(number)=="navaid_dme_range-nm"
	 ) {
      int i;
      for (i=0; i<2; i++) {
	parents.last()->child(parents.last()->childCount()-1)
	  ->appendChild(new VasFlightStatusItem(parents.last()->child(parents.last()->childCount()-1)));
      }
    }
    number++;
  }
}

VorFlagBuffer::VorFlagBuffer(int& ref)
  : m_ref(ref),
    m_prior_to(false),
    m_prior_to_value(false),
    m_prior_from(false),
    m_prior_from_value(false)
  {};
bool VorFlagBuffer::recvTo(const bool to) {
  m_prior_to_value=to;
  if (m_prior_from) {
    m_prior_from=false;
    doit();
    return true;
    }
  m_prior_to=true;
  return false;
};
bool VorFlagBuffer::recvFrom (const bool from) {
  m_prior_from_value=from;
  if (m_prior_to) {
    m_prior_to=false;
    doit();
    return true;
  }
  m_prior_from=true;
  return  false;
};
void VorFlagBuffer::doit() {
    if (m_prior_to_value ^ m_prior_from_value) {
      m_ref=(m_prior_to_value ? FlightStatus::TO : FlightStatus::FROM);
    } else {
      m_ref=0;
    };
};

DmeBuffer::DmeBuffer(QString& ref)
  : m_ref(ref),
    m_prior_valid(false),
    m_prior_range(false),
    m_prior_valid_value(false),
    m_prior_range_value(-1.0)
{};

bool DmeBuffer::recvValid(const bool valid) {
  m_prior_valid_value=valid;
  if (m_prior_range) {
    m_prior_range=false;
    doit();
    return true;
  } 
  m_prior_valid=true;
  return false;
};
bool DmeBuffer::recvDist(const double distance) {
  m_prior_range_value=distance;
  if (m_prior_valid) {
    m_prior_valid=false;
    doit();
    return true;
  }
  m_prior_range=true;
  return false;
};
void DmeBuffer::doit() {
  if (m_prior_valid_value) {
    // hmm "nn.n" or "nnn" padded with leading "0" 
    // can't really get my head around that in msfs access, 
    // think(/hope/guess) it should lead to 5 segment display
    m_ref=QString("%1").arg(m_prior_range_value, 5, 'f', 1, QChar('0'));
  } else {
      m_ref=QString::null;
  };
};
