/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 IITP RAS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Based on
 *      NS-2 AODV model developed by the CMU/MONARCH group and optimized and
 *      tuned by Samir Das and Mahesh Marina, University of Cincinnati;
 *
 *      AODV-UU implementation by Erik Nordström of Uppsala University
 *      http://core.it.uu.se/core/index.php/AODV-UU
 *
 * Authors: Elena Buchatskaia <borovkovaes@iitp.ru>
 *          Pavel Boyko <boyko@iitp.ru>
 */
#include "leach-packet.h"
#include "ns3/address-utils.h"
#include "ns3/packet.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LeachRoutingProtocolPacket");

namespace leach {

NS_OBJECT_ENSURE_REGISTERED (TypeHeader);

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
    case AODVTYPE_RREQ:
    case AODVTYPE_RREP:
    case AODVTYPE_RERR:
    case AODVTYPE_RREP_ACK:
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
    case AODVTYPE_RREQ:
      {
        os << "RREQ";
        break;
      }
    case AODVTYPE_RREP:
      {
        os << "RREP";
        break;
      }
    case AODVTYPE_RERR:
      {
        os << "RERR";
        break;
      }
    case AODVTYPE_RREP_ACK:
      {
        os << "RREP_ACK";
        break;
      }
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

//-----------------------------------------------------------------------------
// RREQ
//-----------------------------------------------------------------------------
RreqHeader::RreqHeader (uint8_t flags, uint8_t reserved, uint8_t hopCount, uint32_t requestID, Ipv4Address dst,
                        uint32_t dstSeqNo, Ipv4Address origin, uint32_t originSeqNo)
  : m_flags (flags),
    m_reserved (reserved),
    m_hopCount (hopCount),
    m_requestID (requestID),
    m_dst (dst),
    m_dstSeqNo (dstSeqNo),
    m_origin (origin),
    m_originSeqNo (originSeqNo)
{
}

NS_OBJECT_ENSURE_REGISTERED (RreqHeader);

TypeId
RreqHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::leach::RreqHeader")
    .SetParent<Header> ()
    .SetGroupName ("Leach")
    .AddConstructor<RreqHeader> ()
  ;
  return tid;
}

TypeId
RreqHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
RreqHeader::GetSerializedSize () const
{
  return 23;
}

void
RreqHeader::Serialize (Buffer::Iterator i) const
{
  i.WriteU8 (m_flags);
  i.WriteU8 (m_reserved);
  i.WriteU8 (m_hopCount);
  i.WriteHtonU32 (m_requestID);
  WriteTo (i, m_dst);
  i.WriteHtonU32 (m_dstSeqNo);
  WriteTo (i, m_origin);
  i.WriteHtonU32 (m_originSeqNo);
}

uint32_t
RreqHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  m_flags = i.ReadU8 ();
  m_reserved = i.ReadU8 ();
  m_hopCount = i.ReadU8 ();
  m_requestID = i.ReadNtohU32 ();
  ReadFrom (i, m_dst);
  m_dstSeqNo = i.ReadNtohU32 ();
  ReadFrom (i, m_origin);
  m_originSeqNo = i.ReadNtohU32 ();

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
RreqHeader::Print (std::ostream &os) const
{
  os << "RREQ ID " << m_requestID << " destination: ipv4 " << m_dst
     << " sequence number " << m_dstSeqNo << " source: ipv4 "
     << m_origin << " sequence number " << m_originSeqNo
     << " flags:" << " Gratuitous RREP " << (*this).GetGratuitousRrep ()
     << " Destination only " << (*this).GetDestinationOnly ()
     << " Unknown sequence number " << (*this).GetUnknownSeqno ();
}

std::ostream &
operator<< (std::ostream & os, RreqHeader const & h)
{
  h.Print (os);
  return os;
}

void
RreqHeader::SetGratuitousRrep (bool f)
{
  if (f)
    {
      m_flags |= (1 << 5);
    }
  else
    {
      m_flags &= ~(1 << 5);
    }
}

bool
RreqHeader::GetGratuitousRrep () const
{
  return (m_flags & (1 << 5));
}

void
RreqHeader::SetDestinationOnly (bool f)
{
  if (f)
    {
      m_flags |= (1 << 4);
    }
  else
    {
      m_flags &= ~(1 << 4);
    }
}

bool
RreqHeader::GetDestinationOnly () const
{
  return (m_flags & (1 << 4));
}

void
RreqHeader::SetUnknownSeqno (bool f)
{
  if (f)
    {
      m_flags |= (1 << 3);
    }
  else
    {
      m_flags &= ~(1 << 3);
    }
}

bool
RreqHeader::GetUnknownSeqno () const
{
  return (m_flags & (1 << 3));
}

bool
RreqHeader::operator== (RreqHeader const & o) const
{
  return (m_flags == o.m_flags && m_reserved == o.m_reserved
          && m_hopCount == o.m_hopCount && m_requestID == o.m_requestID
          && m_dst == o.m_dst && m_dstSeqNo == o.m_dstSeqNo
          && m_origin == o.m_origin && m_originSeqNo == o.m_originSeqNo);
}

//-----------------------------------------------------------------------------
// RREP
//-----------------------------------------------------------------------------

RrepHeader::RrepHeader (uint8_t prefixSize, uint8_t hopCount, Ipv4Address dst,
                        uint32_t dstSeqNo, Ipv4Address origin, Time lifeTime)
  : m_flags (0),
    m_prefixSize (prefixSize),
    m_hopCount (hopCount),
    m_dst (dst),
    m_dstSeqNo (dstSeqNo),
    m_origin (origin)
{
  m_lifeTime = uint32_t (lifeTime.GetMilliSeconds ());
}

NS_OBJECT_ENSURE_REGISTERED (RrepHeader);

TypeId
RrepHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::leach::RrepHeader")
    .SetParent<Header> ()
    .SetGroupName ("Leach")
    .AddConstructor<RrepHeader> ()
  ;
  return tid;
}

TypeId
RrepHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
RrepHeader::GetSerializedSize () const
{
  return 19;
}

void
RrepHeader::Serialize (Buffer::Iterator i) const
{
  i.WriteU8 (m_flags);
  i.WriteU8 (m_prefixSize);
  i.WriteU8 (m_hopCount);
  WriteTo (i, m_dst);
  i.WriteHtonU32 (m_dstSeqNo);
  WriteTo (i, m_origin);
  i.WriteHtonU32 (m_lifeTime);
}

uint32_t
RrepHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;

  m_flags = i.ReadU8 ();
  m_prefixSize = i.ReadU8 ();
  m_hopCount = i.ReadU8 ();
  ReadFrom (i, m_dst);
  m_dstSeqNo = i.ReadNtohU32 ();
  ReadFrom (i, m_origin);
  m_lifeTime = i.ReadNtohU32 ();

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
RrepHeader::Print (std::ostream &os) const
{
  os << "destination: ipv4 " << m_dst << " sequence number " << m_dstSeqNo;
  if (m_prefixSize != 0)
    {
      os << " prefix size " << m_prefixSize;
    }
  os << " source ipv4 " << m_origin << " lifetime " << m_lifeTime
     << " acknowledgment required flag " << (*this).GetAckRequired ();
}

void
RrepHeader::SetLifeTime (Time t)
{
  m_lifeTime = t.GetMilliSeconds ();
}

Time
RrepHeader::GetLifeTime () const
{
  Time t (MilliSeconds (m_lifeTime));
  return t;
}

void
RrepHeader::SetAckRequired (bool f)
{
  if (f)
    {
      m_flags |= (1 << 6);
    }
  else
    {
      m_flags &= ~(1 << 6);
    }
}

bool
RrepHeader::GetAckRequired () const
{
  return (m_flags & (1 << 6));
}

void
RrepHeader::SetPrefixSize (uint8_t sz)
{
  m_prefixSize = sz;
}

uint8_t
RrepHeader::GetPrefixSize () const
{
  return m_prefixSize;
}

bool
RrepHeader::operator== (RrepHeader const & o) const
{
  return (m_flags == o.m_flags && m_prefixSize == o.m_prefixSize
          && m_hopCount == o.m_hopCount && m_dst == o.m_dst && m_dstSeqNo == o.m_dstSeqNo
          && m_origin == o.m_origin && m_lifeTime == o.m_lifeTime);
}

void
RrepHeader::SetHello (Ipv4Address origin, uint32_t srcSeqNo, Time lifetime)
{
  m_flags = 0;
  m_prefixSize = 0;
  m_hopCount = 0;
  m_dst = origin;
  m_dstSeqNo = srcSeqNo;
  m_origin = origin;
  m_lifeTime = lifetime.GetMilliSeconds ();
}

std::ostream &
operator<< (std::ostream & os, RrepHeader const & h)
{
  h.Print (os);
  return os;
}

//-----------------------------------------------------------------------------
// RREP-ACK
//-----------------------------------------------------------------------------

RrepAckHeader::RrepAckHeader ()
  : m_reserved (0)
{
}

NS_OBJECT_ENSURE_REGISTERED (RrepAckHeader);

TypeId
RrepAckHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::leach::RrepAckHeader")
    .SetParent<Header> ()
    .SetGroupName ("Leach")
    .AddConstructor<RrepAckHeader> ()
  ;
  return tid;
}

TypeId
RrepAckHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
RrepAckHeader::GetSerializedSize () const
{
  return 1;
}

void
RrepAckHeader::Serialize (Buffer::Iterator i ) const
{
  i.WriteU8 (m_reserved);
}

uint32_t
RrepAckHeader::Deserialize (Buffer::Iterator start )
{
  Buffer::Iterator i = start;
  m_reserved = i.ReadU8 ();
  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
RrepAckHeader::Print (std::ostream &os ) const
{
}

bool
RrepAckHeader::operator== (RrepAckHeader const & o ) const
{
  return m_reserved == o.m_reserved;
}

std::ostream &
operator<< (std::ostream & os, RrepAckHeader const & h )
{
  h.Print (os);
  return os;
}

//-----------------------------------------------------------------------------
// RERR
//-----------------------------------------------------------------------------
RerrHeader::RerrHeader ()
  : m_flag (0),
    m_reserved (0)
{
}

NS_OBJECT_ENSURE_REGISTERED (RerrHeader);

TypeId
RerrHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::leach::RerrHeader")
    .SetParent<Header> ()
    .SetGroupName ("Leach")
    .AddConstructor<RerrHeader> ()
  ;
  return tid;
}

TypeId
RerrHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
RerrHeader::GetSerializedSize () const
{
  return (3 + 8 * GetDestCount ());
}

void
RerrHeader::Serialize (Buffer::Iterator i ) const
{
  i.WriteU8 (m_flag);
  i.WriteU8 (m_reserved);
  i.WriteU8 (GetDestCount ());
  std::map<Ipv4Address, uint32_t>::const_iterator j;
  for (j = m_unreachableDstSeqNo.begin (); j != m_unreachableDstSeqNo.end (); ++j)
    {
      WriteTo (i, (*j).first);
      i.WriteHtonU32 ((*j).second);
    }
}

uint32_t
RerrHeader::Deserialize (Buffer::Iterator start )
{
  Buffer::Iterator i = start;
  m_flag = i.ReadU8 ();
  m_reserved = i.ReadU8 ();
  uint8_t dest = i.ReadU8 ();
  m_unreachableDstSeqNo.clear ();
  Ipv4Address address;
  uint32_t seqNo;
  for (uint8_t k = 0; k < dest; ++k)
    {
      ReadFrom (i, address);
      seqNo = i.ReadNtohU32 ();
      m_unreachableDstSeqNo.insert (std::make_pair (address, seqNo));
    }

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
RerrHeader::Print (std::ostream &os ) const
{
  os << "Unreachable destination (ipv4 address, seq. number):";
  std::map<Ipv4Address, uint32_t>::const_iterator j;
  for (j = m_unreachableDstSeqNo.begin (); j != m_unreachableDstSeqNo.end (); ++j)
    {
      os << (*j).first << ", " << (*j).second;
    }
  os << "No delete flag " << (*this).GetNoDelete ();
}

void
RerrHeader::SetNoDelete (bool f )
{
  if (f)
    {
      m_flag |= (1 << 0);
    }
  else
    {
      m_flag &= ~(1 << 0);
    }
}

bool
RerrHeader::GetNoDelete () const
{
  return (m_flag & (1 << 0));
}

bool
RerrHeader::AddUnDestination (Ipv4Address dst, uint32_t seqNo )
{
  if (m_unreachableDstSeqNo.find (dst) != m_unreachableDstSeqNo.end ())
    {
      return true;
    }

  NS_ASSERT (GetDestCount () < 255); // can't support more than 255 destinations in single RERR
  m_unreachableDstSeqNo.insert (std::make_pair (dst, seqNo));
  return true;
}

bool
RerrHeader::RemoveUnDestination (std::pair<Ipv4Address, uint32_t> & un )
{
  if (m_unreachableDstSeqNo.empty ())
    {
      return false;
    }
  std::map<Ipv4Address, uint32_t>::iterator i = m_unreachableDstSeqNo.begin ();
  un = *i;
  m_unreachableDstSeqNo.erase (i);
  return true;
}

void
RerrHeader::Clear ()
{
  m_unreachableDstSeqNo.clear ();
  m_flag = 0;
  m_reserved = 0;
}

bool
RerrHeader::operator== (RerrHeader const & o ) const
{
  if (m_flag != o.m_flag || m_reserved != o.m_reserved || GetDestCount () != o.GetDestCount ())
    {
      return false;
    }

  std::map<Ipv4Address, uint32_t>::const_iterator j = m_unreachableDstSeqNo.begin ();
  std::map<Ipv4Address, uint32_t>::const_iterator k = o.m_unreachableDstSeqNo.begin ();
  for (uint8_t i = 0; i < GetDestCount (); ++i)
    {
      if ((j->first != k->first) || (j->second != k->second))
        {
          return false;
        }

      j++;
      k++;
    }
  return true;
}

//-----------------------------------------------------------------------------
// MsgHeader
//-----------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------
// AdRepHeader
//-----------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------
// TimeTableHeader
//-----------------------------------------------------------------------------
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


//-----------------------------------------------------------------------------
// MsgHeader
//-----------------------------------------------------------------------------
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
    .AddConstructor<RreqHeader> ()
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


std::ostream &
operator<< (std::ostream & os, RerrHeader const & h )
{
  h.Print (os);
  return os;
}
}
}
