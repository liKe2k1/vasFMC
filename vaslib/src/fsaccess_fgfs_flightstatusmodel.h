///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2008 K. Hoercher
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

#ifndef FGACCESS_MODEL_H
#define FGACCESS_MODEL_H

#include <QObject>
#include <QAbstractItemModel>
#include <QList>

//class QAbstractListModel;
class FlightStatus;
class VasFlightStatusItem;
class VorFlagBuffer;
class DmeBuffer;

static inline bool translateBool (const QVariant in) { return (in.toDouble()!=0.0);};
/*! Interface to the somewhat developing vasfmc-internal flightstatus.
 * To more easily adapt to internals I wanted a layer predominantly allowing
 * the XML-defined protocol stream from FG to be mapped to flightstatus.h.
 * Hopefully it is cheap enough at runtime and picks up any changes at compile time.
 * 
 * As it is quite a hack onto the model/view concept of Qt it causes tremendous slowdown
 * when using the view part, although I found that very handy when adding new functionality.
 *
 * Some helper classes exist to represent different semantics of vasfmc and FG.
 * \TODO make it really self-contained wrt radionavigation
 */
class VasFlightStatusModel : public QAbstractItemModel
{ Q_OBJECT
static const QStringList names;
public:
  VasFlightStatusModel(FlightStatus* status, QObject *parent=0);
  virtual ~VasFlightStatusModel(); 

  //inline virtual const VasFlightStatusModel* asVasFlightStatusModel() const { return this;}; 

  int rowCount(const QModelIndex &parent = QModelIndex()) const;
  QVariant data(const QModelIndex &index, int role) const;
  QVariant headerData(int section, 
                      Qt::Orientation orientation,
		      int role = Qt::DisplayRole) const;
  Qt::ItemFlags flags(const QModelIndex &index) const;
  bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
  QModelIndex index(int, int, const QModelIndex&) const;
  QModelIndex parent(const QModelIndex&) const;
  int columnCount(const QModelIndex&) const;
private:
  //! hidden default ctor
  VasFlightStatusModel();
  //! hidden, as QAbstractItemModel apparently disables copy anyhow
  VasFlightStatusModel(const VasFlightStatusModel&);
  //! hidden assignment
  VasFlightStatusModel& operator= (const VasFlightStatusModel&);
  
  FlightStatus* m_flightstatus;    //!< the underlying code we want to represent
  VasFlightStatusItem* m_rootitem; //!< serves as reference "anchor"
  VorFlagBuffer* nav1_tofrom;      //!< state-rememebering for the 1st NAV
  VorFlagBuffer* nav2_tofrom;      //!< state-rememebering for the 2nd NAV
  DmeBuffer* dme1;
  DmeBuffer* dme2;
  void setupModelData(VasFlightStatusItem* parent);
};
class VasFlightStatusItem
{
public:
  VasFlightStatusItem(VasFlightStatusItem* parent=0);
  ~VasFlightStatusItem();

  void appendChild(VasFlightStatusItem *child);

  VasFlightStatusItem *child(const int row);
  int childCount() const;
  int columnCount() const;
  int row() const;
  VasFlightStatusItem *parent();
private:
  QList<VasFlightStatusItem*> m_childItems;
  VasFlightStatusItem *m_parentItem;
};

/*! Helper for mangling the meaning of FG instrumentation convention.
 * The default FG NAV instrument uses separate properties for TO and FROM
 * flags. If both are FALSE they construe the meaning of an somehow invalid 
 * signal with respect to the flag.
 *
 * As those properties get transmitted at arbitrary times we must wait for both 
 * of them being set to determine the setting of the corresponding flightstatus item(s).
 * 
 * Note that depending on the order of those elements in defining protocol file or failures
 * during transmit, the two values might stem from different "frames". Despite that, the 
 * following frames (transmitted at an reasonable frequnecy) should correct such an eventual
 * error; even more so as the values should only change after "large" intervals (from couple 
 * of seconds to minutes or hours)
 */
class VorFlagBuffer {
 public:
  VorFlagBuffer(int& ref);
  virtual ~VorFlagBuffer(){};
  bool recvTo (const bool to);
  bool recvFrom (const bool from);
 private:
  inline void doit(); //!< check, if feasible set referenced item 
  int& m_ref; //!< reference to the flightstatus item to set
  bool m_prior_to,         //!< TO already recvieved?
       m_prior_to_value,   //!< recvieved TO flag value
       m_prior_from,       //!< FROM already recieved? 
       m_prior_from_value; //!< recvieved FROM flag value
  
};
/*! Helper for DME Instrument data
 * Somewhat combined solution for 2 different problems.
 *
 * Default FG dme Instrument keeps the indicated distance even if/after
 * it lost a valid signal. So filter those out, depending on the in-range
 * property.
 * 
 * Cope with a nice, fractional, easy-to-understand value for a distance
 * measurement being mangled into some representation encumbered (Q)String.
 * Unsure whom to thank for it, MSFS or FSUIPC authors *g*
 * see Note for VorFlagBuffer.
 */
class DmeBuffer {
 public:
  DmeBuffer(QString& ref);
  virtual ~DmeBuffer(){};
  bool recvValid(const bool valid);
  bool recvDist(const double distance);
 private:
  inline void doit(); //!< check, if feasible, set referenced item; uh will probably not be inlined
  QString& m_ref; //!< reference to the flightstatus item to set
  bool m_prior_valid;
  bool m_prior_range;
  bool m_prior_valid_value;
  double m_prior_range_value;
};
#endif  //FGACCESS_MODEL_H
