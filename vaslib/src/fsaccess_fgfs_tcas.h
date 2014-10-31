/*! start own tcas implemantation
 * does nothing now, (hopefully mimicks xplane/fsifasewrapper behaviour;
 * my understanding being, to return just a dummy list)
 */
#include "fsaccess_fgfs_base.h" //<- <QObject>
#include <QTimer>

class TcasEntry;
class FGFSIo;
class Waypoint;

class FGTcasItem {
 public:
  FGTcasItem();
  virtual ~FGTcasItem();
  bool m_valid;
  int m_id;
  Waypoint* m_position;
  int m_altitude_ft;
  int m_true_heading;
  int m_groundspeed_kts;
  int m_vs_fpm;
 private:
  QString m_callsign;
};
//! dummy to check xml and io modules
class VasTcasModel {
 public:
  VasTcasModel();
  virtual ~VasTcasModel(){};
};

class TcasEntryContainer : public QObject
{ Q_OBJECT
public:
TcasEntryContainer(bool valid=false, bool airborne=false);
TcasEntryContainer(TcasEntry* blob, bool valid=false, bool airborne=false);
~TcasEntryContainer() {};

TcasEntry getFSTcasEntry();
const bool isValid() { return valid; };
const bool isAirborne() { return airborne; };
const bool check();

float fvalue;
double dvalue;
qint32 q32value;
qint16 q16value;
quint8 qu8value;
QString qsvalue;

public slots:
void setValid(bool set) { valid=set; };
void setAirborne(bool set) { airborne=set; };
void setInRange(bool set) { fginrange=set; };
bool setId();
bool setLat();
bool setLon();
bool setAlt();
bool setHdg();
bool setGs();
bool setVs();
bool setIdATC(); //we will bother with that here, not in the conversions
bool setBState(); // same here, data needs more inspection I think
bool setCom1();

private:
TcasEntry* blob;
bool valid;
bool airborne;
bool fginrange;

};
class FlightStatus;
class FGTcas : public QObject
{ Q_OBJECT 
public:
  FGTcas(FlightStatus* flightstatus);
  virtual ~FGTcas();

  bool initread (const QString& readfile, const QString& protocol);
  bool initread (const QHostAddress& hostaddress, const quint16 port, const QString& protocol);

  QList<TcasEntry>& getTcasEntryValueList() {return m_tcas_entry_list;};
  void requestTcasData(){};
  
  FGFSIo* getIo() { return tcasread;}
 
 public slots:
 void slotDummy(int);
  void slotStatusUpdate(const FGmsg&);
  void slotSetValid(bool);

  void slotDie();
  void slotDieAdd(int);
  void slotDieRemove(int);
  void slotResetTimer();
 signals:
  void sigResetTimer();

 private:
  FGTcas();
  FGFSIo* tcasread;
  TcasEntryContainer tmp;  
  QList<TcasEntry> m_tcas_entry_list;
  QList<int>tcas_entry_die_list;
  int oldest_entry;
  QTimer timer;
  int entry_timeout;
     
};
