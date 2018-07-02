/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Name: Bart de Haan
 *
 *
 */
#ifndef LEACH_PACKET_H
#define LEACH_PACKET_H

#include <iostream>
#include "ns3/header.h"
#include "ns3/enum.h"
#include "ns3/ipv4-address.h"
#include <map>
#include "ns3/nstime.h"

namespace ns3 {
namespace leach {


enum MessageType
{
  LEACHTYPE_AD   = 1,
  LEACHTYPE_AD_REP = 2,
  LEACHTYPE_TT = 3,
  LEACHTYPE_MSG = 4
};

/**
 * This class is adapted from aodv-packet.h
 * Link: https://www.nsnam.org/doxygen/aodv-packet_8h_source.html
 */
class TypeHeader : public Header
{
public:
  TypeHeader (MessageType t = LEACHTYPE_AD);

  /**
   * Get the type ID.
   */
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;


  MessageType Get () const
  {
    return m_type;
  }

  /**
   * Check that type if valid
   */
  bool IsValid () const
  {
    return m_valid;
  }
  /**
   * \brief Comparison operator
   * \param o header to compare
   * \return true if the headers are equal
   */
  bool operator== (TypeHeader const & o) const;
private:
  MessageType m_type; ///< type of the message
  bool m_valid; ///< Indicates if the message is valid
};

/**
  * \brief Stream output operator
  * \param os output stream
  * \return updated stream
  */
std::ostream & operator<< (std::ostream & os, TypeHeader const & h);

/**
* \ingroup leach
* \brief   Cluster head advertism
  \verbatim
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |       Type    |            Reserved                           |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                    Originator IP Address                      |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                  Originator Sequence Number                   |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                  x-coord                                      |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                  y-coords                                     |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  \endverbatim
*/
class AdHeader : public Header
{
public:
   AdHeader (Ipv4Address origin = Ipv4Address (), uint32_t originSeqNo = 0,
              u_int32_t x = 0, u_int32_t y = 0);

  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

  void SetPos(u_int32_t x, u_int32_t y)
  {
      m_x = x;
      m_y = y;
  }

  u_int32_t GetX() const
  {
      return m_x;
  }

  u_int32_t GetY() const
  {
      return m_y;
  }

  void SetOrigin (Ipv4Address a)
  {
    m_origin = a;
  }

  Ipv4Address GetOrigin () const
  {
    return m_origin;
  }

  void SetOriginSeqno (uint32_t s)
  {
    m_originSeqNo = s;
  }

  uint32_t GetOriginSeqno () const
  {
    return m_originSeqNo;
  }

  bool operator== (AdHeader const & o) const;
private:
  Ipv4Address    m_origin;      /* Originator IP Address */
  uint32_t       m_originSeqNo; /* Source Sequence Number */
  uint32_t       m_x;           /* Source Sequence Number */
  uint32_t       m_y;           /* Source Sequence Number */
};


/**
*
* Cluster head reply advertism header
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |       Type    |            Reserved                           |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                  Originator IP Address                        |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                  Destination IP Address                       |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
class AdRepHeader : public Header
{
public:

   AdRepHeader(Ipv4Address origin = Ipv4Address (),
               Ipv4Address destination = Ipv4Address());

  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

  void SetOrigin (Ipv4Address a)
  {
    m_origin = a;
  }

  Ipv4Address GetOrigin () const
  {
    return m_origin;
  }

  void SetDestination (Ipv4Address a)
  {
    m_destination = a;
  }

  Ipv4Address GetDestination() const
  {
    return m_destination;
  }

  bool operator== (AdRepHeader const & o) const;
private:
  Ipv4Address    m_origin;      /* Originator IP Address */
  Ipv4Address    m_destination; /* Destination IP Address */
};


/**
 * Time table header.
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |       Type    |            Reserved                           |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                  Originator IP Address                        |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                  Destination IP Address                       |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                  Time slot                                    |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                  Time duration                                |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  \endverbatim
*/
class TimeTableHeader : public Header
{
public:

   TimeTableHeader(Ipv4Address origin = Ipv4Address (),
                   Ipv4Address destination = Ipv4Address(),
                   Time slot = Time(),
                   Time duration = Time());

  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;


  void SetOrigin (Ipv4Address a)
  {
    m_origin = a;
  }

  Ipv4Address GetOrigin () const
  {
    return m_origin;
  }

  void SetDestination (Ipv4Address a)
  {
    m_destination = a;
  }

  Ipv4Address GetDestination() const
  {
    return m_destination;
  }

  void SetTimeSlot(Time a)
  {
    m_timeSlot = a;
  }

  Time GetTimeSlot() const
  {
    return m_timeSlot;
  }

  void SetTimeDuration(Time a)
  {
    m_duration = a;
  }

  Time GetTimeDuration() const
  {
    return m_duration;
  }

  bool operator== (TimeTableHeader const & o) const;
private:
  Ipv4Address    m_origin;      /* Originator IP Address */
  Ipv4Address    m_destination; /* Destination IP Address */
  Time           m_timeSlot;    /* Start of the time slot. */
  Time           m_duration;    /* Duration of the time slot. */
};


/**
 *  Leach MSG to sink header.
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |       Type    |            Reserved                           |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                    Originator IP Address                      |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                  Originator Sequence Number                   |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  \endverbatim
*/
class MsgHeader : public Header
{
public:

  MsgHeader (Ipv4Address origin = Ipv4Address (), uint32_t originSeqNo = 0);

  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

  void SetOrigin (Ipv4Address a)
  {
    m_origin = a;
  }

  Ipv4Address GetOrigin () const
  {
    return m_origin;
  }

  void SetOriginSeqno (uint32_t s)
  {
    m_originSeqNo = s;
  }

  uint32_t GetOriginSeqno () const
  {
    return m_originSeqNo;
  }

  bool operator== (MsgHeader const & o) const;
private:
  Ipv4Address    m_origin;         /* Originator IP Address */
  uint32_t       m_originSeqNo;    /* Source Sequence Number */
};


std::ostream & operator<< (std::ostream & os, MsgHeader const &);

}  /* namespace leach */
}  /* namespace ns3 */

#endif /* LEACH_PACKET_H */
