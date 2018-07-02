/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Name: Bart de Haan
 * This file contains the source code for a LEACH packet.
 */


#include "leach-packet.h"
#include "ns3/address-utils.h"
#include "ns3/packet.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LeachRoutingProtocolPacket");

namespace leach {

NS_OBJECT_ENSURE_REGISTERED (TypeHeader);

/**
 * NOTE: the TypeHeader is adapted from the aodv-packet.cc
 * Link: https://www.nsnam.org/doxygen/aodv-packet_8cc.html
 */

TypeHeader::TypeHeader (MessageType t)
  : m_type (t),
    m_valid (true)
{
}

TypeId
TypeHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::leach::TypeHeader")
    .SetParent<Header> ()
    .SetGroupName ("Leach")
    .AddConstructor<TypeHeader> ()
  ;
  return tid;
}

TypeId
TypeHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
TypeHeader::GetSerializedSize () const
{
  return 1;
}

void
TypeHeader::Serialize (Buffer::Iterator i) const
{
  i.WriteU8 ((uint8_t) m_type);
}

uint32_t
TypeHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  uint8_t type = i.ReadU8 ();
  m_valid = true;
  switch (type)
    {
    case LEACHTYPE_AD:
    case LEACHTYPE_AD_REP:
    case LEACHTYPE_TT:
    case LEACHTYPE_MSG:
      {
        m_type = (MessageType) type;
        break;
      }
    default:
      m_valid = false;
    }
  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
TypeHeader::Print (std::ostream &os) const
{
  switch (m_type)
    {
    case LEACHTYPE_AD:
    {
      os << "LEACH_AD";
      break;
    }
    case LEACHTYPE_AD_REP:
    {
      os << "LEACH_AD_REP";
      break;
    }
    case LEACHTYPE_MSG:
    {
      os << "LEACH_MSG";
      break;
    }
    default:
      os << "UNKNOWN_TYPE";
    }
}

bool
TypeHeader::operator== (TypeHeader const & o) const
{
  return (m_type == o.m_type && m_valid == o.m_valid);
}

std::ostream &
operator<< (std::ostream & os, TypeHeader const & h)
{
  h.Print (os);
  return os;
}


/**
 * Advertisment phase header.
 */

AdHeader::AdHeader (Ipv4Address origin, uint32_t originSeqNo, u_int32_t x, u_int32_t y)
  : m_origin (origin),
    m_originSeqNo (originSeqNo),
    m_x(x),
    m_y(y)
{
}

NS_OBJECT_ENSURE_REGISTERED (AdHeader);

TypeId
AdHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::leach::AdHeader")
    .SetParent<Header> ()
    .SetGroupName ("Leach")
    .AddConstructor<AdHeader> ()
  ;
  return tid;
}

TypeId
AdHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
AdHeader::GetSerializedSize () const
{
  return 19;
}

void
AdHeader::Serialize (Buffer::Iterator i) const
{
  i.WriteU16((u_int16_t) 0);
  i.WriteU8((u_int8_t) 0);
  WriteTo (i, m_origin);
  i.WriteHtonU32 (m_originSeqNo);
  i.WriteU32(m_x);
  i.WriteU32(m_y);
}

uint32_t
AdHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  i.ReadU16();
  i.ReadU8();
  ReadFrom (i, m_origin);
  m_originSeqNo = i.ReadNtohU32 ();
  m_x = i.ReadU32();
  m_y = i.ReadU32();

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
AdHeader::Print (std::ostream &os) const
{
  os << " source: ipv4 " << m_origin << " sequence number " << m_originSeqNo
     << " x: " << m_x << " y: " << m_y;
}

bool
AdHeader::operator== (AdHeader const & o) const
{
  return (m_origin == o.m_origin && m_originSeqNo == o.m_originSeqNo
          && m_x == o.m_x && m_y == o.m_y);
}


std::ostream &
operator<< (std::ostream & os, AdHeader const & h)
{
    h.Print (os);
    return os;
}

/**
 * Advertisment reply header.
 */
AdRepHeader::AdRepHeader (Ipv4Address origin, Ipv4Address destination)
  : m_origin(origin),
    m_destination(destination)
{
}

NS_OBJECT_ENSURE_REGISTERED (AdRepHeader);

TypeId
AdRepHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::leach::AdRepHeader")
    .SetParent<Header> ()
    .SetGroupName ("Leach")
    .AddConstructor<AdRepHeader> ()
  ;
  return tid;
}

TypeId
AdRepHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
AdRepHeader::GetSerializedSize () const
{
  return 11;
}

void
AdRepHeader::Serialize (Buffer::Iterator i) const
{
  /* Write the reserved field. */
  i.WriteU16((u_int16_t) 0);
  i.WriteU8((u_int8_t) 0);

  /* Write the origin and destination ipv4 addresses. */
  WriteTo (i, m_origin);
  WriteTo (i, m_destination);
}

uint32_t
AdRepHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;

  /* Read the reserved field */
  i.ReadU16();
  i.ReadU8();

  /* Read the origin and destination field */
  ReadFrom (i, m_origin);
  ReadFrom (i, m_destination);

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
AdRepHeader::Print (std::ostream &os) const
{
  os << " source: ipv4 " << m_origin << " CH ipv4: " << m_destination;
}

bool
AdRepHeader::operator== (AdRepHeader const & o) const
{
  return (m_origin == o.m_origin && m_destination == o.m_destination);
}


std::ostream &
operator<< (std::ostream & os, AdRepHeader const & h)
{
    h.Print (os);
    return os;
}

/**
 * Time table header.
 */
TimeTableHeader::TimeTableHeader(Ipv4Address origin, Ipv4Address destination,
                                 Time slot, Time duration)
  : m_origin(origin),
    m_destination(destination),
    m_timeSlot(slot),
    m_duration(duration)
{
}

NS_OBJECT_ENSURE_REGISTERED(TimeTableHeader);

TypeId
TimeTableHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::leach::TimeTableHeader")
    .SetParent<Header> ()
    .SetGroupName ("Leach")
    .AddConstructor<TimeTableHeader>()
  ;
  return tid;
}

TypeId
TimeTableHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
TimeTableHeader::GetSerializedSize () const
{
  return 19;
}

void
TimeTableHeader::Serialize (Buffer::Iterator i) const
{
  /* Write the reserved field. */
  i.WriteU16((u_int16_t) 0);
  i.WriteU8((u_int8_t) 0);

  /* Write the origin and destination ipv4 addresses. */
  WriteTo (i, m_origin);
  WriteTo (i, m_destination);
  i.WriteU32((u_int32_t) m_timeSlot.GetMilliSeconds());
  i.WriteU32((u_int32_t) m_duration.GetMilliSeconds());

  NS_LOG_INFO("Serialize: " << (u_int32_t) m_timeSlot.GetMilliSeconds());
}

uint32_t
TimeTableHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;

  /* Read the reserved field */
  i.ReadU16();
  i.ReadU8();

  /* Read the origin and destination field */
  ReadFrom (i, m_origin);
  ReadFrom (i, m_destination);
  m_timeSlot = Time(MilliSeconds(i.ReadU32()));
  m_duration = Time(MilliSeconds(i.ReadU32()));

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
TimeTableHeader::Print (std::ostream &os) const
{
  os << " source: ipv4 " << m_origin << " CH ipv4: " << m_destination <<
        " timeSlot: " << m_timeSlot.GetSeconds() << " duration: " << m_duration;
}

bool
TimeTableHeader::operator== (TimeTableHeader const & o) const
{
  return (m_origin == o.m_origin && m_destination == o.m_destination &&
          m_timeSlot == o.m_timeSlot &&  m_duration == o.m_duration);
}


std::ostream &
operator<< (std::ostream & os, TimeTableHeader const & h)
{
    h.Print (os);
    return os;
}


/**
 * LEACH message header.
 */
MsgHeader::MsgHeader (Ipv4Address origin, uint32_t originSeqNo)
  : m_origin (origin),
    m_originSeqNo (originSeqNo)
{
}

NS_OBJECT_ENSURE_REGISTERED (MsgHeader);

TypeId
MsgHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::leach::MsgHeader")
    .SetParent<Header> ()
    .SetGroupName ("Leach")
    .AddConstructor<MsgHeader> ()
  ;
  return tid;
}

TypeId
MsgHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
MsgHeader::GetSerializedSize () const
{
  return 11;
}

void
MsgHeader::Serialize (Buffer::Iterator i) const
{
  i.WriteU16((u_int16_t) 0);
  i.WriteU8((u_int8_t) 0);
  WriteTo (i, m_origin);
  i.WriteHtonU32 (m_originSeqNo);
}

uint32_t
MsgHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  i.ReadU16();
  i.ReadU8();
  ReadFrom (i, m_origin);
  m_originSeqNo = i.ReadNtohU32 ();

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
MsgHeader::Print (std::ostream &os) const
{
  os << " source: ipv4 " << m_origin << " sequence number " << m_originSeqNo;
}

bool
MsgHeader::operator== (MsgHeader const & o) const
{
  return (m_origin == o.m_origin && m_originSeqNo == o.m_originSeqNo);
}


std::ostream &
operator<< (std::ostream & os, MsgHeader const & h)
{
    h.Print (os);
    return os;
}

}
}
