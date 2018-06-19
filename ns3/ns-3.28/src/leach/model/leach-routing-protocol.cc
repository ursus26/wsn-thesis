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
#define NS_LOG_APPEND_CONTEXT                                   \
  if (m_ipv4) { std::clog << "[node " << m_ipv4->GetObject<Node> ()->GetId () << "] " << Simulator::Now().GetSeconds() << "s\t"; }

#include "leach-routing-protocol.h"
#include "ns3/log.h"
#include "ns3/boolean.h"
#include "ns3/random-variable-stream.h"
#include "ns3/inet-socket-address.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/udp-l4-protocol.h"
#include "ns3/udp-header.h"
#include "ns3/wifi-net-device.h"
#include "ns3/adhoc-wifi-mac.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include <algorithm>
#include <limits>
#include "math.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LeachRoutingProtocol");

namespace leach {
NS_OBJECT_ENSURE_REGISTERED (RoutingProtocol);

/// UDP Port for AODV control traffic
const uint32_t RoutingProtocol::LEACH_PORT = 501;

/**
* \ingroup aodv
* \brief Tag used by AODV implementation
*/
class DeferredRouteOutputTag : public Tag
{

public:
  /**
   * \brief Constructor
   * \param o the output interface
   */
  DeferredRouteOutputTag (int32_t o = -1) : Tag (),
                                            m_oif (o)
  {
  }

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId ()
  {
    static TypeId tid = TypeId ("ns3::leach::DeferredRouteOutputTag")
      .SetParent<Tag> ()
      .SetGroupName ("Leach")
      .AddConstructor<DeferredRouteOutputTag> ()
    ;
    return tid;
  }

  TypeId  GetInstanceTypeId () const
  {
    return GetTypeId ();
  }

  /**
   * \brief Get the output interface
   * \return the output interface
   */
  int32_t GetInterface () const
  {
    return m_oif;
  }

  /**
   * \brief Set the output interface
   * \param oif the output interface
   */
  void SetInterface (int32_t oif)
  {
    m_oif = oif;
  }

  uint32_t GetSerializedSize () const
  {
    return sizeof(int32_t);
  }

  void  Serialize (TagBuffer i) const
  {
    i.WriteU32 (m_oif);
  }

  void  Deserialize (TagBuffer i)
  {
    m_oif = i.ReadU32 ();
  }

  void  Print (std::ostream &os) const
  {
    os << "DeferredRouteOutputTag: output interface = " << m_oif;
  }

private:
  /// Positive if output device is fixed in RouteOutput
  int32_t m_oif;
};

NS_OBJECT_ENSURE_REGISTERED (DeferredRouteOutputTag);


//-----------------------------------------------------------------------------
RoutingProtocol::RoutingProtocol ()
  : m_ttlStart (1),
    m_ttlIncrement (2),
    m_ttlThreshold (7),
    m_timeoutBuffer (2),
    m_activeRouteTimeout (Seconds (3)),
    m_maxQueueLen (64),
    m_maxQueueTime (Seconds (30)),
    m_queueTime(Seconds (1)),
    m_queueTimer(Timer::CANCEL_ON_DESTROY),
    m_routingTable(Time(5)),
    m_queue (m_maxQueueLen, m_maxQueueTime),
    m_requestId (0),
    m_seqNo (0),
    m_nb (Seconds(5)),
    m_lastBcastTime (Seconds (0)),
    m_CHPercentage(0.05),
    m_isCluserHead(false),
    m_wasCH(false),
    m_nearestCH_addr(Ipv4Address()),
    m_nearestCH_dist(100000), /* Large enough so a new connection is always the CH.  */
    m_sleepNode(Timer::CANCEL_ON_DESTROY),
    m_roundDuration(Seconds(3)),
    m_roundTimer(Timer::CANCEL_ON_DESTROY),
    m_curRound(0),
    m_inSetupPhase(false),
    m_setupDuration(Time(.25)),
    m_setupTimer(Timer::CANCEL_ON_DESTROY),
    m_advertiseDuration(Seconds(.25)),
    m_advertiseTimer(Timer::CANCEL_ON_DESTROY),
    m_broadcastTimer(Timer::CANCEL_ON_DESTROY),
    m_replyDuration(Seconds(1)),
    m_replyTimer(Timer::CANCEL_ON_DESTROY),
    m_sendTimer(Timer::CANCEL_ON_DESTROY),
    m_openTimeFrame(false),
    m_x(0),
    m_y(0),
    m_myAddr(Ipv4Address()),
    m_isBS(false)
{
}

TypeId
RoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::leach::RoutingProtocol")
    .SetParent<Ipv4RoutingProtocol> ()
    .SetGroupName ("Leach")
    .AddConstructor<RoutingProtocol> ()
    .AddAttribute ("TtlStart", "Initial TTL value for RREQ.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&RoutingProtocol::m_ttlStart),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("TtlIncrement", "TTL increment for each attempt using the expanding ring search for RREQ dissemination.",
                   UintegerValue (2),
                   MakeUintegerAccessor (&RoutingProtocol::m_ttlIncrement),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("TtlThreshold", "Maximum TTL value for expanding ring search, TTL = NetDiameter is used beyond this value.",
                   UintegerValue (7),
                   MakeUintegerAccessor (&RoutingProtocol::m_ttlThreshold),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("TimeoutBuffer", "Provide a buffer for the timeout.",
                   UintegerValue (2),
                   MakeUintegerAccessor (&RoutingProtocol::m_timeoutBuffer),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MaxQueueLen", "Maximum number of packets that we allow a routing protocol to buffer.",
                   UintegerValue (64),
                   MakeUintegerAccessor (&RoutingProtocol::SetMaxQueueLen,
                                         &RoutingProtocol::GetMaxQueueLen),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxQueueTime", "Maximum time packets can be queued (in seconds)",
                   TimeValue (Seconds (30)),
                   MakeTimeAccessor (&RoutingProtocol::SetMaxQueueTime,
                                     &RoutingProtocol::GetMaxQueueTime),
                   MakeTimeChecker ())
    .AddAttribute ("UniformRv",
                   "Access to the underlying UniformRandomVariable",
                   StringValue ("ns3::UniformRandomVariable"),
                   MakePointerAccessor (&RoutingProtocol::m_uniformRandomVariable),
                   MakePointerChecker<UniformRandomVariable> ())
  ;
  return tid;
}

void
RoutingProtocol::SetMaxQueueLen (uint32_t len)
{
  m_maxQueueLen = len;
  m_queue.SetMaxQueueLen (len);
}
void
RoutingProtocol::SetMaxQueueTime (Time t)
{
  m_maxQueueTime = t;
  m_queue.SetQueueTimeout (t);
}

RoutingProtocol::~RoutingProtocol ()
{
}

void
RoutingProtocol::DoDispose ()
{
  m_ipv4 = 0;
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::iterator iter =
         m_socketAddresses.begin (); iter != m_socketAddresses.end (); iter++)
    {
      iter->first->Close ();
    }
  m_socketAddresses.clear ();
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::iterator iter =
         m_socketSubnetBroadcastAddresses.begin (); iter != m_socketSubnetBroadcastAddresses.end (); iter++)
    {
      iter->first->Close ();
    }
  m_socketSubnetBroadcastAddresses.clear ();
  Ipv4RoutingProtocol::DoDispose ();
}

void
RoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  *stream->GetStream () << "Node: " << m_ipv4->GetObject<Node> ()->GetId ()
                        << "; Time: " << Now ().As (unit)
                        << ", Local time: " << GetObject<Node> ()->GetLocalTime ().As (unit)
                        << ", LEACH Routing table" << std::endl;

  m_routingTable.Print (stream);
  *stream->GetStream () << std::endl;
}

int64_t
RoutingProtocol::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_uniformRandomVariable->SetStream (stream);
  return 1;
}

void
RoutingProtocol::Start ()
{
  NS_LOG_FUNCTION (this);
}

Ptr<Ipv4Route>
RoutingProtocol::RouteOutput (Ptr<Packet> p, const Ipv4Header &header,
                              Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{
    NS_LOG_FUNCTION (this << header << (oif ? oif->GetIfIndex () : 0));


    if(!m_isCluserHead)
    {
        return RouteOutputClusterNode(p, header, oif, sockerr);
    }
    else
    {
        return RouteOutputClusterHead(p, header, oif, sockerr);
    }

    // if(header.GetDestination() != Ipv4Address("10.1.1.1"))
    // {
    //     Ptr<Ipv4Route> route = Create<Ipv4Route> ();
    //     route->SetDestination(header.GetDestination());
    //     route->SetGateway(header.GetDestination());
    //     route->SetSource(m_myAddr);
    //     route->SetOutputDevice(m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(m_myAddr)));
    //
    //     // NS_LOG_INFO("Route to: " << route->GetDestination() << " from: " << route->GetSource()
    //     //             << " through: " << route->GetGateway() << " on interface: " <<
    //     //             route->GetOutputDevice());
    //
    //     return route;
    // }

    /* If this node is not the cluster head, then send data to the CH. */
    // if(!m_isCluserHead && m_nearestCH_addr != Ipv4Address())
    // {
    //     /* Create route to base station through the cluster head. */
    //     Ptr<Ipv4Route> route = Create<Ipv4Route> ();
    //     route->SetDestination(header.GetDestination());
    //     route->SetGateway(m_nearestCH_addr);
    //     route->SetSource(m_myAddr);
    //     route->SetOutputDevice(m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(m_myAddr)));
    //
    //     NS_LOG_INFO("Route to: " << route->GetDestination() << " from: " << route->GetSource()
    //                 << " through: " << route->GetGateway() << " on interface: " <<
    //                 route->GetOutputDevice());
    //
    //     return route;
    // }
    /* If this node is the CH, then send directly to the sink. */
    // else
    // if(m_isCluserHead)
    // {
    //     /* Create direct route to base station. */
    //     Ptr<Ipv4Route> route = Create<Ipv4Route> ();
    //     route->SetDestination(header.GetDestination());
    //     route->SetGateway(header.GetDestination());
    //     route->SetSource(m_myAddr);
    //     route->SetOutputDevice(m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(m_myAddr)));
    //
    //     NS_LOG_INFO("Route to: " << route->GetDestination() << " from: " << route->GetSource()
    //                 << " through: " << route->GetGateway() << " on interface: " <<
    //                 route->GetOutputDevice());
    //
    //     return route;
    // }
    // else if(!m_inSetupPhase && !m_isCluserHead && m_nearestCH_addr == Ipv4Address())
    // {
    //     /* Create route to base station through the cluster head. */
    //     Ptr<Ipv4Route> route = Create<Ipv4Route> ();
    //     route->SetDestination(header.GetDestination());
    //     route->SetGateway(header.GetDestination());
    //     route->SetSource(m_myAddr);
    //     route->SetOutputDevice(m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(m_myAddr)));
    //
    //     NS_LOG_INFO("Route to: " << route->GetDestination() << " from: " << route->GetSource()
    //                << " through: " << route->GetGateway() << " on interface: " <<
    //                route->GetOutputDevice());
    //
    //     return route;
    // }
    //
    // // Valid route not found, in this case we return loopback.
    // // Actual route request will be deferred until packet will be fully formed,
    // // routed to loopback, received from loopback and passed to RouteInput (see below)
    //
    // NS_LOG_INFO("No route available to: " << header.GetDestination() << ", so store the msg, m_nearestCH_addr " << m_nearestCH_addr);
    //
    // uint32_t iif = -2;
    // DeferredRouteOutputTag tag (iif);
    // if (!p->PeekPacketTag (tag))
    // {
    //     p->AddPacketTag (tag);
    // }
    //
    // return LoopbackRoute (header, oif);
}

Ptr<Ipv4Route>
RoutingProtocol::RouteOutputClusterNode (Ptr<Packet> p, const Ipv4Header &header,
                                         Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{
    /* Reply to cluster head. */
    if(m_inSetupPhase && header.GetDestination() == m_nearestCH_addr)
    {
        /* Create route to base station through the cluster head. */
        Ptr<Ipv4Route> route = Create<Ipv4Route> ();
        route->SetDestination(header.GetDestination());
        route->SetGateway(header.GetDestination());
        route->SetSource(m_myAddr);
        route->SetOutputDevice(m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(m_myAddr)));

        NS_LOG_INFO("Route reply to CH: " << route->GetDestination() << " from: " << route->GetSource()
                    << " through: " << route->GetGateway() << " on interface: " <<
                    route->GetOutputDevice());

        return route;
    }


    /* Send message directly if there are no cluster heads found or reply to the
     * cluster head. */
    if(!m_inSetupPhase && m_nearestCH_addr == Ipv4Address())
    {
        /* Create route to base station through the cluster head. */
        Ptr<Ipv4Route> route = Create<Ipv4Route> ();
        route->SetDestination(header.GetDestination());
        route->SetGateway(header.GetDestination());
        route->SetSource(m_myAddr);
        route->SetOutputDevice(m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(m_myAddr)));

        NS_LOG_INFO("1.Route to: " << route->GetDestination() << " from: " << route->GetSource()
                    << " through: " << route->GetGateway() << " on interface: " <<
                    route->GetOutputDevice());

        return route;
    }

    else if(m_openTimeFrame && !m_inSetupPhase)
    {
        /* Create route to base station through the cluster head. */
        Ptr<Ipv4Route> route = Create<Ipv4Route> ();
        route->SetDestination(header.GetDestination());
        route->SetGateway(m_nearestCH_addr);
        route->SetSource(m_myAddr);
        route->SetOutputDevice(m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(m_myAddr)));

        NS_LOG_INFO("2.Route to: " << route->GetDestination() << " from: " << route->GetSource()
                    << " through: " << route->GetGateway() << " on interface: " <<
                    route->GetOutputDevice());

        return route;
    }


    NS_LOG_INFO("No route available to: " << header.GetDestination() << ", so store the msg, m_nearestCH_addr " << m_nearestCH_addr << " m_inSetupPhase: " << m_inSetupPhase);

    uint32_t iif = -2;
    DeferredRouteOutputTag tag (iif);
    if (!p->PeekPacketTag (tag))
    {
        p->AddPacketTag (tag);
    }

    return LoopbackRoute (header, oif);
}

Ptr<Ipv4Route>
RoutingProtocol::RouteOutputClusterHead (Ptr<Packet> p, const Ipv4Header &header,
                                         Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{
    /* Create direct route to the destination. */
    Ptr<Ipv4Route> route = Create<Ipv4Route> ();
    route->SetDestination(header.GetDestination());
    route->SetGateway(header.GetDestination());
    route->SetSource(m_myAddr);
    route->SetOutputDevice(m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(m_myAddr)));

    NS_LOG_INFO("Route from CH to: " << route->GetDestination() << " from: " << route->GetSource()
                << " through: " << route->GetGateway() << " on interface: " <<
                route->GetOutputDevice());

    return route;

}


void
RoutingProtocol::DeferredRouteOutput (Ptr<const Packet> p, const Ipv4Header & header,
                                      UnicastForwardCallback ucb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p << header);
  NS_ASSERT (p != 0 && p != Ptr<Packet> ());
  // NS_LOG_INFO("JAJAJ DeferredRouteOutput");

  QueueEntry newEntry (p, header, ucb, ecb);
  bool result = m_queue.Enqueue (newEntry);
  if (result)
    {
      // NS_LOG_INFO ("Add packet " << p->GetUid () << " to queue. Protocol " << (uint16_t)
      //               header.GetProtocol ());

      /* Set the queue timer. */
      // m_roundTimer.Schedule(m_queueTime);

      RoutingTableEntry rt;
      bool result = m_routingTable.LookupRoute (header.GetDestination (), rt);
      if (!result || ((rt.GetFlag () != IN_SEARCH) && result))
        {
          NS_LOG_LOGIC ("Send new RREQ for outbound packet to " << header.GetDestination ());
          // SendRequest (header.GetDestination ());
        }
    }
}

bool
RoutingProtocol::RouteInput (Ptr<const Packet> p, const Ipv4Header &header,
                             Ptr<const NetDevice> idev, UnicastForwardCallback ucb,
                             MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb)
{
    NS_LOG_FUNCTION (this << p->GetUid () << header.GetDestination () << idev->GetAddress ());
    // NS_LOG_INFO("Hello, routeinput!!!!!!!!!!!!!");

    // NS_LOG_INFO("Output dev: " << m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(m_myAddr)) <<
    //             " idev: " << idev << " m_lo: " << m_lo);


    if (m_socketAddresses.empty ())
    {
        NS_LOG_LOGIC ("No leach interfaces");
        return false;
    }

    NS_ASSERT (m_ipv4 != 0);
    NS_ASSERT (p != 0);

    /* Check if input device supports IP */
    NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);

    Ipv4Address dst = header.GetDestination ();
    Ipv4Address origin = header.GetSource ();

    NS_LOG_INFO("RouteInput origin: " << origin << " dst: " << dst);

    /* Deferred route request */
    if (idev == m_lo)
    {
        DeferredRouteOutputTag tag;
        if (p->PeekPacketTag (tag))
        {
            DeferredRouteOutput (p, header, ucb, ecb);
            return true;
        }
    }

    // Forwarding
    return Forwarding (p, header, ucb, ecb);
}

bool
RoutingProtocol::Forwarding (Ptr<const Packet> p, const Ipv4Header & header,
                             UnicastForwardCallback ucb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this);
  // Ipv4Address dst = header.GetDestination ();
  // Ipv4Address origin = header.GetSource ();
  // m_routingTable.Purge ();
  // RoutingTableEntry toDst;


  /* If this node is the CH, then send directly to the sink. */
  if(m_isCluserHead)
  {
      /* Create the route. */
      Ptr<Ipv4Route> route = Create<Ipv4Route> ();;
      route->SetDestination(header.GetDestination());
      route->SetGateway(header.GetDestination());
      route->SetSource(m_myAddr);
      route->SetOutputDevice(m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(m_myAddr)));

      NS_LOG_INFO("Forward to: " << route->GetDestination() << " from: " << header.GetSource()
                  << " through: " << route->GetGateway() << " on interface: " <<
                  route->GetOutputDevice());

      ucb(route, p, header);

      return true;
  }

  NS_LOG_DEBUG ("Drop packet " << p->GetUid () << " because node is not clulster head.");
  return false;
}

void
RoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_ASSERT (ipv4 != 0);
  NS_ASSERT (m_ipv4 == 0);

  m_ipv4 = ipv4;

  // Create lo route. It is asserted that the only one interface up for now is loopback
  NS_ASSERT (m_ipv4->GetNInterfaces () == 1 && m_ipv4->GetAddress (0, 0).GetLocal () == Ipv4Address ("127.0.0.1"));
  m_lo = m_ipv4->GetNetDevice (0);
  NS_ASSERT (m_lo != 0);
  // Remember lo route
  RoutingTableEntry rt (/*device=*/ m_lo, /*dst=*/ Ipv4Address::GetLoopback (), /*know seqno=*/ true, /*seqno=*/ 0,
                                    /*iface=*/ Ipv4InterfaceAddress (Ipv4Address::GetLoopback (), Ipv4Mask ("255.0.0.0")),
                                    /*hops=*/ 1, /*next hop=*/ Ipv4Address::GetLoopback (),
                                    /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
  m_routingTable.AddRoute (rt);

  Simulator::ScheduleNow (&RoutingProtocol::Start, this);
}

void
RoutingProtocol::NotifyInterfaceUp (uint32_t i)
{
  NS_LOG_FUNCTION (this << m_ipv4->GetAddress (i, 0).GetLocal ());
  m_myAddr = m_ipv4->GetAddress (i, 0).GetLocal ();
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  if (l3->GetNAddresses (i) > 1)
    {
      NS_LOG_WARN ("LEACH does not work with more then one address per each interface.");
    }
  Ipv4InterfaceAddress iface = l3->GetAddress (i, 0);
  if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
    {
      return;
    }

  // Create a socket to listen only on this interface
  Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
                                             UdpSocketFactory::GetTypeId ());
  NS_ASSERT (socket != 0);
  socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvLeach, this));
  socket->BindToNetDevice(l3->GetNetDevice (i));
  socket->Bind (InetSocketAddress (iface.GetLocal (), LEACH_PORT));
  socket->SetAllowBroadcast (true);
  socket->SetIpRecvTtl (true);
  m_socketAddresses.insert (std::make_pair (socket, iface));

  // create also a subnet broadcast socket
  socket = Socket::CreateSocket (GetObject<Node> (),
                                 UdpSocketFactory::GetTypeId ());
  NS_ASSERT (socket != 0);
  socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvLeach, this));
  socket->BindToNetDevice (l3->GetNetDevice (i));
  socket->Bind (InetSocketAddress (iface.GetBroadcast (), LEACH_PORT));
  socket->SetAllowBroadcast (true);
  socket->SetIpRecvTtl (true);
  m_socketSubnetBroadcastAddresses.insert (std::make_pair (socket, iface));

  // Add local broadcast record to the routing table
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
  RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*know seqno=*/ true, /*seqno=*/ 0, /*iface=*/ iface,
                                    /*hops=*/ 1, /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
  m_routingTable.AddRoute (rt);

  if (l3->GetInterface (i)->GetArpCache ())
    {
      m_nb.AddArpCache (l3->GetInterface (i)->GetArpCache ());
    }

  // Allow neighbor manager use this interface for layer 2 feedback if possible
  Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice> ();
  if (wifi == 0)
    {
      return;
    }
  Ptr<WifiMac> mac = wifi->GetMac ();
  if (mac == 0)
    {
      return;
    }

  m_wifiDev = dev->GetObject<WifiNetDevice>();
  mac->TraceConnectWithoutContext ("TxErrHeader", m_nb.GetTxErrorCallback ());
}

void
RoutingProtocol::NotifyInterfaceDown (uint32_t i)
{
  NS_LOG_FUNCTION (this << m_ipv4->GetAddress (i, 0).GetLocal ());

  // Disable layer 2 link state monitoring (if possible)
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  Ptr<NetDevice> dev = l3->GetNetDevice (i);
  Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice> ();
  if (wifi != 0)
    {
      Ptr<WifiMac> mac = wifi->GetMac ()->GetObject<AdhocWifiMac> ();
      if (mac != 0)
        {
          mac->TraceDisconnectWithoutContext ("TxErrHeader",
                                              m_nb.GetTxErrorCallback ());
          m_nb.DelArpCache (l3->GetInterface (i)->GetArpCache ());
        }
    }

  // Close socket
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (m_ipv4->GetAddress (i, 0));
  NS_ASSERT (socket);
  socket->Close ();
  m_socketAddresses.erase (socket);

  // Close socket
  socket = FindSubnetBroadcastSocketWithInterfaceAddress (m_ipv4->GetAddress (i, 0));
  NS_ASSERT (socket);
  socket->Close ();
  m_socketSubnetBroadcastAddresses.erase (socket);

  if (m_socketAddresses.empty ())
    {
      NS_LOG_LOGIC ("No leach interfaces");
      // m_htimer.Cancel ();
      m_nb.Clear ();
      m_routingTable.Clear ();
      return;
    }
  m_routingTable.DeleteAllRoutesFromInterface (m_ipv4->GetAddress (i, 0));
}

void
RoutingProtocol::NotifyAddAddress (uint32_t i, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << " interface " << i << " address " << address);
  // std::cout << " interface " << i << " address " << address << std::endl;
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  if (!l3->IsUp (i))
    {
      return;
    }
  if (l3->GetNAddresses (i) == 1)
    {
      Ipv4InterfaceAddress iface = l3->GetAddress (i, 0);
      Ptr<Socket> socket = FindSocketWithInterfaceAddress (iface);
      if (!socket)
        {
            std::cout << "No Socket!!!" << std::endl;
          if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
            {
              return;
            }
          // Create a socket to listen only on this interface
          Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
                                                     UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvLeach,this));
          socket->BindToNetDevice (l3->GetNetDevice (i));
          socket->Bind (InetSocketAddress (iface.GetLocal (), LEACH_PORT));
          socket->SetAllowBroadcast (true);
          m_socketAddresses.insert (std::make_pair (socket, iface));

          // create also a subnet directed broadcast socket
          socket = Socket::CreateSocket (GetObject<Node> (),
                                         UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvLeach, this));
          socket->BindToNetDevice (l3->GetNetDevice (i));
          socket->Bind (InetSocketAddress (iface.GetBroadcast (), LEACH_PORT));
          socket->SetAllowBroadcast (true);
          socket->SetIpRecvTtl (true);
          m_socketSubnetBroadcastAddresses.insert (std::make_pair (socket, iface));

          // Add local broadcast record to the routing table
          Ptr<NetDevice> dev = m_ipv4->GetNetDevice (
              m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
          RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*know seqno=*/ true,
                                            /*seqno=*/ 0, /*iface=*/ iface, /*hops=*/ 1,
                                            /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
          m_routingTable.AddRoute (rt);
        }
    }
  else
    {
      NS_LOG_LOGIC ("LEACH does not work with more then one address per each interface. Ignore added address");
    }
}

void
RoutingProtocol::NotifyRemoveAddress (uint32_t i, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (address);
  if (socket)
    {
      m_routingTable.DeleteAllRoutesFromInterface (address);
      socket->Close ();
      m_socketAddresses.erase (socket);

      Ptr<Socket> unicastSocket = FindSubnetBroadcastSocketWithInterfaceAddress (address);
      if (unicastSocket)
        {
          unicastSocket->Close ();
          m_socketAddresses.erase (unicastSocket);
        }

      Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
      if (l3->GetNAddresses (i))
        {
          Ipv4InterfaceAddress iface = l3->GetAddress (i, 0);
          // Create a socket to listen only on this interface
          Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
                                                     UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvLeach, this));
          // Bind to any IP address so that broadcasts can be received
          socket->BindToNetDevice (l3->GetNetDevice (i));
          socket->Bind (InetSocketAddress (iface.GetLocal (), LEACH_PORT));
          socket->SetAllowBroadcast (true);
          socket->SetIpRecvTtl (true);
          m_socketAddresses.insert (std::make_pair (socket, iface));

          // create also a unicast socket
          socket = Socket::CreateSocket (GetObject<Node> (),
                                         UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvLeach, this));
          socket->BindToNetDevice (l3->GetNetDevice (i));
          socket->Bind (InetSocketAddress (iface.GetBroadcast (), LEACH_PORT));
          socket->SetAllowBroadcast (true);
          socket->SetIpRecvTtl (true);
          m_socketSubnetBroadcastAddresses.insert (std::make_pair (socket, iface));

          // Add local broadcast record to the routing table
          Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
          RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*know seqno=*/ true, /*seqno=*/ 0, /*iface=*/ iface,
                                            /*hops=*/ 1, /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
          m_routingTable.AddRoute (rt);
        }
      if (m_socketAddresses.empty ())
        {
          NS_LOG_LOGIC ("No leach interfaces");
          // m_htimer.Cancel ();
          m_nb.Clear ();
          m_routingTable.Clear ();
          return;
        }
    }
  else
    {
      NS_LOG_LOGIC ("Remove address not participating in LEACH operation");
    }
}

bool
RoutingProtocol::IsMyOwnAddress (Ipv4Address src)
{
  NS_LOG_FUNCTION (this << src);
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ipv4InterfaceAddress iface = j->second;
      if (src == iface.GetLocal ())
        {
          return true;
        }
    }
  return false;
}

Ptr<Ipv4Route>
RoutingProtocol::LoopbackRoute (const Ipv4Header & hdr, Ptr<NetDevice> oif) const
{
  NS_LOG_FUNCTION (this << hdr);
  NS_ASSERT (m_lo != 0);
  Ptr<Ipv4Route> rt = Create<Ipv4Route> ();
  rt->SetDestination (hdr.GetDestination ());
  //
  // Source address selection here is tricky.  The loopback route is
  // returned when AODV does not have a route; this causes the packet
  // to be looped back and handled (cached) in RouteInput() method
  // while a route is found. However, connection-oriented protocols
  // like TCP need to create an endpoint four-tuple (src, src port,
  // dst, dst port) and create a pseudo-header for checksumming.  So,
  // AODV needs to guess correctly what the eventual source address
  // will be.
  //
  // For single interface, single address nodes, this is not a problem.
  // When there are possibly multiple outgoing interfaces, the policy
  // implemented here is to pick the first available AODV interface.
  // If RouteOutput() caller specified an outgoing interface, that
  // further constrains the selection of source address
  //
  std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin ();
  if (oif)
    {
      // Iterate to find an address on the oif device
      for (j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
        {
          Ipv4Address addr = j->second.GetLocal ();
          int32_t interface = m_ipv4->GetInterfaceForAddress (addr);
          if (oif == m_ipv4->GetNetDevice (static_cast<uint32_t> (interface)))
            {
              rt->SetSource (addr);
              break;
            }
        }
    }
  else
    {
      rt->SetSource (j->second.GetLocal ());
    }
  NS_ASSERT_MSG (rt->GetSource () != Ipv4Address (), "Valid LEACH source address not found");
  rt->SetGateway (Ipv4Address ("127.0.0.1"));
  rt->SetOutputDevice (m_lo);
  return rt;
}

void
RoutingProtocol::SendTo (Ptr<Socket> socket, Ptr<Packet> packet, Ipv4Address destination)
{
  socket->SendTo (packet, 0, InetSocketAddress (destination, LEACH_PORT));
}

void
RoutingProtocol::RecvLeach (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Address sourceAddress;
  Ptr<Packet> packet = socket->RecvFrom (sourceAddress);
  InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom (sourceAddress);
  Ipv4Address sender = inetSourceAddr.GetIpv4 ();
  Ipv4Address receiver;

  if (m_socketAddresses.find (socket) != m_socketAddresses.end ())
    {
      receiver = m_socketAddresses[socket].GetLocal ();
    }
  else if (m_socketSubnetBroadcastAddresses.find (socket) != m_socketSubnetBroadcastAddresses.end ())
    {
      receiver = m_socketSubnetBroadcastAddresses[socket].GetLocal ();
    }
  else
    {
      NS_ASSERT_MSG (false, "Received a packet from an unknown socket");
    }
  NS_LOG_INFO("LEACH node " << this << " received a LEACH packet from " << sender << " to " << receiver);

  // UpdateRouteToNeighbor (sender, receiver);
  TypeHeader tHeader (AODVTYPE_RREQ);
  packet->RemoveHeader (tHeader);
  if (!tHeader.IsValid ())
    {
      NS_LOG_DEBUG ("LEACH message " << packet->GetUid () << " with unknown type received: " << tHeader.Get () << ". Drop");
      return; // drop
    }
  switch (tHeader.Get ())
    {
    case AODVTYPE_RREQ:
      {
        break;
      }
    case AODVTYPE_RREP:
      {
        break;
      }
    case AODVTYPE_RERR:
      {
        break;
      }
    case AODVTYPE_RREP_ACK:
      {
        break;
      }
    case LEACHTYPE_AD:
    {
        RecvAdvertism(packet, receiver, sender);
        break;
    }
    case LEACHTYPE_AD_REP:
    {
        RecvAdReply(packet, sender);
        break;
    }
    case LEACHTYPE_TT:
    {
        RecvTimeTable(packet, sender);
        break;
    }
    case LEACHTYPE_MSG:
    {
        break;
    }
    }
}


double RoutingProtocol::DistTo(u_int32_t a_x, u_int32_t a_y)
{
    /* Simple pythagoras to calculate the distance between two points. */
    int dx = a_x - m_x;
    int dy = a_y - m_y;
    double x_square = pow(dx, 2);
    double y_square = pow(dy, 2);
    double dist = sqrt(x_square + y_square);

    return dist;
}

void
RoutingProtocol::RecvAdvertism(Ptr<Packet> p, Ipv4Address receiver, Ipv4Address sender)
{
    NS_LOG_FUNCTION(this << " receiver: " << receiver << " sender: " << sender);
    if(m_isBS)
    {
        NS_LOG_INFO("Base Station received cluster head advertism, drop packet.");
        return;
    }
    else if(m_isCluserHead)
    {
        NS_LOG_INFO("Receiving node is also a cluster head, drop packet.");
        return;
    }

    AdHeader ad;
    p->RemoveHeader(ad);

    /* Calculare the distance to the received CH. */
    double d = DistTo(ad.GetX(), ad.GetY());

    /* If the distance is smaller than the current CH, then replace the old CH. */
    if(d < m_nearestCH_dist)
    {
        m_nearestCH_addr = sender;
        m_nearestCH_dist = d;

        NS_LOG_INFO("New close CH found");
    }
}

void
RoutingProtocol::SendAdvertism ()
{
  NS_LOG_FUNCTION (this);
  /* Broadcast a RREP with TTL = 1 with the RREP message fields set as follows:
   *   Destination IP Address         The node's IP address.
   *   Destination Sequence Number    The node's latest sequence number.
   *   Hop Count                      0
   *   Lifetime                       AllowedHelloLoss * HelloInterval
   */

   NS_LOG_INFO("Sending Ads");
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      AdHeader ad(iface.GetLocal(), m_seqNo, m_x, m_y);
      Ptr<Packet> packet = Create<Packet> ();
      SocketIpTtlTag tag;
      tag.SetTtl (1);
      packet->AddPacketTag (tag);
      packet->AddHeader (ad);
      TypeHeader tHeader (LEACHTYPE_AD);
      packet->AddHeader (tHeader);
      // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
      Ipv4Address destination;
      if (iface.GetMask () == Ipv4Mask::GetOnes ())
        {
          destination = Ipv4Address ("255.255.255.255");
        }
      else
        {
          destination = iface.GetBroadcast ();
        }


      // Time jitter = Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0,
                            // m_advertiseDuration.GetMilliSeconds() - 25)));
      // NS_LOG_INFO("Send Ads jitter: " << jitter);
      // Simulator::Schedule (jitter, &RoutingProtocol::SendTo, this, socket, packet, destination);
      // Simulator::ScheduleNow(&RoutingProtocol::SendTo, this, socket, packet, destination);
      SendTo(socket, packet, destination);
    }
}

void
RoutingProtocol::RecvAdReply(Ptr<Packet> p, Ipv4Address sender)
{
    NS_LOG_FUNCTION(this << " sender: " << sender);

    /* Get the header. */
    AdRepHeader rep;
    p->RemoveHeader(rep);

    m_clusterNodes.push_back(sender);

    NS_LOG_INFO("New cluster node: " << rep.GetOrigin());
}

void
RoutingProtocol::SendAdRep ()
{
  NS_LOG_FUNCTION (this);
  /* Unicast a reply with the AdRep message fields set as follows:
   *   Origin IP Address              The node's IP address.
   *   Destination IP Address         The CH's IP address.
   */

  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      // Ipv4InterfaceAddress iface = j->second;

      AdRepHeader rep(m_myAddr, m_nearestCH_addr);
      Ptr<Packet> packet = Create<Packet>();
      SocketIpTtlTag tag;
      tag.SetTtl(1);
      packet->AddPacketTag (tag);
      packet->AddHeader(rep);
      TypeHeader tHeader(LEACHTYPE_AD_REP);
      packet->AddHeader(tHeader);

      // Time jitter = Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0, 100)));
      Time jitter = Time (MilliSeconds (m_uniformRandomVariable->GetInteger (1,
                          m_replyDuration.GetMilliSeconds() / 2)));
      // NS_LOG_INFO("Send Ads jitter: " << jitter);
      Simulator::Schedule (jitter, &RoutingProtocol::SendTo, this, socket, packet, m_nearestCH_addr);
      // Simulator::ScheduleNow(&RoutingProtocol::SendTo, this, socket, packet, m_nearestCH_addr);
    }
}

void
RoutingProtocol::SendPacketFromQueue (Ipv4Address dst, Ptr<Ipv4Route> route)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO("HELLLOOOOOOOOOOOOOOOOOO!");
  QueueEntry queueEntry;
  while (m_queue.Dequeue (dst, queueEntry))
    {
      DeferredRouteOutputTag tag;
      Ptr<Packet> p = ConstCast<Packet> (queueEntry.GetPacket ());
      if (p->RemovePacketTag (tag)
          && tag.GetInterface () != -1
          && tag.GetInterface () != m_ipv4->GetInterfaceForDevice (route->GetOutputDevice ()))
        {
          NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
          return;
        }
      UnicastForwardCallback ucb = queueEntry.GetUnicastForwardCallback ();
      Ipv4Header header = queueEntry.GetIpv4Header ();
      header.SetSource (route->GetSource ());
      header.SetTtl (header.GetTtl () + 1); // compensate extra TTL decrement by fake loopback routing
      ucb (route, p, header);
    }
}

void RoutingProtocol::SendTimeTable()
{
    Time roundLeft = Time(MilliSeconds(m_roundDuration.GetMilliSeconds() -
        m_advertiseDuration.GetMilliSeconds() - m_replyDuration.GetMilliSeconds()));

    Time timeIncr = Time(MilliSeconds(roundLeft.GetMilliSeconds() / (m_clusterNodes.size() + 1)));

    NS_LOG_INFO("Time to next slot: " << timeIncr.GetSeconds() << " Cluster size: " << m_clusterNodes.size());

    int j = 1;
    for (std::list<Ipv4Address>::const_iterator i = m_clusterNodes.begin();
         i != m_clusterNodes.end(); i++)
    {
        /* Calculate the time slot. */
        Time slot = Time(MilliSeconds(j * timeIncr.GetMilliSeconds() + Simulator::Now().GetMilliSeconds()));
        NS_LOG_INFO(*i << " gets time slot: " <<  slot.GetSeconds());
        j++;

        for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
          {
            Ptr<Socket> socket = j->first;

            // NS_LOG_INFO("What? " << i->Get() << " " << Ipv4Address(i->Get()) << " " << *i);

            TimeTableHeader tt(m_myAddr, *i, slot, timeIncr);
            Ptr<Packet> packet = Create<Packet> ();
            SocketIpTtlTag tag;
            tag.SetTtl (1);
            packet->AddPacketTag (tag);
            packet->AddHeader(tt);
            TypeHeader tHeader(LEACHTYPE_TT);
            packet->AddHeader(tHeader);
            // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
            // Ipv4Address destination;

            Simulator::ScheduleNow(&RoutingProtocol::SendTo, this, socket, packet, *i);
        }

    }

    /* We leave the setup phase and enter the steady state phase. */
    m_inSetupPhase = false;
}

void RoutingProtocol::RecvTimeTable(Ptr<Packet> p, Ipv4Address origin)
{
    /* Get the header. */
    TimeTableHeader tt;
    p->RemoveHeader(tt);

    Time delay = (tt.GetTimeSlot() - Simulator::Now());

    if(!delay.IsPositive())
    {
        NS_LOG_INFO("Delay is not possitive.");
        return;
    }

    NS_LOG_INFO("New time slot: " << tt.GetTimeSlot().GetSeconds() << " delay: " << delay.GetSeconds());

    /* Set the timer and put the node to sleep. */
    m_sendTimer.Cancel();
    m_sendTimer.Schedule(delay);
    m_sendTime = tt.GetTimeDuration();
    sleep();

    /* We leave the setup phase and enter the steady state phase. */
    m_inSetupPhase = false;
}

void
RoutingProtocol::SendFromQueue()
{
    NS_LOG_FUNCTION (this);

    /* Wake the node up. */
    resume();

    Ipv4Address dst = Ipv4Address("10.1.1.1");

    QueueEntry queueEntry;
    while (m_queue.Dequeue (dst, queueEntry))
    {
        DeferredRouteOutputTag tag;
        Ptr<Packet> p = ConstCast<Packet> (queueEntry.GetPacket ());

        Ptr<Ipv4Route> route = Create<Ipv4Route> ();
        route->SetDestination(dst);

        if(m_isCluserHead || (!m_inSetupPhase && m_nearestCH_addr == Ipv4Address()))
            route->SetGateway(dst);
        else
            route->SetGateway(m_nearestCH_addr);

        route->SetSource(m_myAddr);
        route->SetOutputDevice(m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(m_myAddr)));

        NS_LOG_INFO("Queued packet, route to: " << route->GetDestination() <<
        " from: " << route->GetSource() << " through: " << route->GetGateway() <<
        " on interface: " << route->GetOutputDevice());

        UnicastForwardCallback ucb = queueEntry.GetUnicastForwardCallback ();
        Ipv4Header header = queueEntry.GetIpv4Header ();
        header.SetSource (route->GetSource ());
        header.SetTtl (header.GetTtl () + 1); // compensate extra TTL decrement by fake loopback routing
        ucb (route, p, header);
    }
}

Ptr<Socket>
RoutingProtocol::FindSocketWithInterfaceAddress (Ipv4InterfaceAddress addr ) const
{
  NS_LOG_FUNCTION (this << addr);
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      if (iface == addr)
        {
          return socket;
        }
    }
  Ptr<Socket> socket;
  return socket;
}

Ptr<Socket>
RoutingProtocol::FindSubnetBroadcastSocketWithInterfaceAddress (Ipv4InterfaceAddress addr ) const
{
  NS_LOG_FUNCTION (this << addr);
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketSubnetBroadcastAddresses.begin (); j != m_socketSubnetBroadcastAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      if (iface == addr)
        {
          return socket;
        }
    }
  Ptr<Socket> socket;
  return socket;
}

void
RoutingProtocol::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);

  m_queueTimer.SetFunction(&RoutingProtocol::queueTimerExpired, this);
  m_advertiseTimer.SetFunction(&RoutingProtocol::advertisePhaseExpired, this);
  m_broadcastTimer.SetFunction(&RoutingProtocol::SendAdvertism, this);
  m_replyTimer.SetFunction(&RoutingProtocol::replyPhaseExpired, this);
  m_sleepNode.SetFunction(&RoutingProtocol::SendPacketFromQueue, this);
  m_sendTimer.SetFunction(&RoutingProtocol::sendTimerExpired, this);

  /* Schedule a new round. */
  m_roundTimer.SetFunction(&RoutingProtocol::newRound, this);
  // uint32_t startTime = m_uniformRandomVariable->GetInteger (0, 100);
  // m_roundTimer.Schedule(MilliSeconds(startTime));
  m_roundTimer.Schedule();

  /* Check if this node is the base station. */
  if(m_ipv4->GetObject<Node>()->GetId() == 0)
  {
      m_isBS = true;
  }

  /* Node testing. */
  // NS_LOG_INFO("GetNApplications: " << m_ipv4->GetObject<Node>()->GetNApplications());
  // NS_LOG_INFO("App name: " << m_ipv4->GetObject<Node>()->GetApplication(0)->GetTypeId().GetName());
  // NS_LOG_INFO("App device name: " << m_ipv4->GetNetDevice(0)->GetNode()->GetApplication(0)->GetTypeId().GetName());
  //
  // TypeId id = m_ipv4->GetNetDevice(0)->GetNode()->GetApplication(0)->GetTypeId();
  //
  // for(uint i = 0; i < id.GetAttributeN(); i++)
  // {
  //     NS_LOG_INFO("==> Attribute: " << id.GetAttributeFullName(i));
  // }

  // NS_LOG_INFO("POSITION: " << m_ipv4->GetObject<Node>()->GetObject<MobilityModel>()->GetPosition().x);

  /* Get the position of the node. */
  m_x = m_ipv4->GetObject<Node>()->GetObject<MobilityModel>()->GetPosition().x;
  m_y = m_ipv4->GetObject<Node>()->GetObject<MobilityModel>()->GetPosition().y;

  Ipv4RoutingProtocol::DoInitialize();
}


void RoutingProtocol::queueTimerExpired()
{
    NS_LOG_INFO(Simulator::Now ().GetSeconds () << " QUEUE TIMER EXPIRED");
}

void RoutingProtocol::advertisePhaseExpired()
{
    NS_LOG_FUNCTION(Simulator::Now ().GetSeconds ());
    NS_LOG_INFO("ADVERTISE TIMER EXPIRED");

    /* If we found a cluster head, then reply to it. */
    if(!m_isCluserHead && m_nearestCH_addr != Ipv4Address())
    {
        SendAdRep();
    }
    /* Leave setup phase if there are not CH. */
    else if(!m_isCluserHead && m_nearestCH_addr == Ipv4Address())
    {
        m_inSetupPhase = false;
        SendFromQueue();
    }

    /* Schedule the next phase of LEACH. */
    m_replyTimer.Cancel();
    m_replyTimer.Schedule(m_replyDuration);
}

void RoutingProtocol::replyPhaseExpired()
{
    NS_LOG_FUNCTION(Simulator::Now().GetSeconds());
    NS_LOG_INFO("REPLY TIMER EXPIRED");

    /* Send the time table. */
    if(m_isCluserHead)
    {
        SendTimeTable();
        SendFromQueue();
    }
}

void RoutingProtocol::sendTimerExpired()
{
    NS_LOG_FUNCTION(Simulator::Now().GetSeconds());
    NS_LOG_INFO("SEND TIMER EXPIRED");

    /* The time window for sending to the CH is open. */
    if(!m_openTimeFrame && !m_inSetupPhase)
    {
        NS_LOG_INFO("OPEN TIME FRAME");
        resume();

        m_openTimeFrame = true;
        SendFromQueue();

        /* Schedule the event that our window is closing. */
        m_sendTimer.Cancel();
        m_sendTimer.Schedule(m_sendTime);
    }
    /* The time window is closing. */
    else
    {
        NS_LOG_INFO("CLOSE TIME FRAME");
        m_openTimeFrame = false;
        sleep();
    }


}

/**
 * Calculate a threshhold for every node.
 * The return value should be between 0 and 1.
 */
float RoutingProtocol::electionProbability()
{
    NS_LOG_FUNCTION(this);

    /* Return a probability of 0.0 if the node was a CH in (1 / m_CHPercentage)
     * number of rounds. */
    if(m_wasCH)
        return 0.0;

    /* Return the probability that this node becomes a CH. */
    return m_CHPercentage / (float) (1 - m_CHPercentage * fmod(m_curRound, (1 / m_CHPercentage)));
}

/**
 * The node will elect itself as a CH with a certain probability.
 */
void RoutingProtocol::runElection()
{
    NS_LOG_FUNCTION(this);

    double randNum = m_uniformRandomVariable->GetValue(0.0, 1.0);
    if(randNum < electionProbability())
    {
        NS_LOG_INFO("Node is elected");
        m_isCluserHead = true;
    }

    // if(m_ipv4->GetObject<Node> ()->GetId () == 1)
    // {
    //     NS_LOG_INFO("Node is elected");
    //     m_isCluserHead = true;
    // }
}

/**
 * Start a new round in the leach protocol by first choosing new cluster heads.
 */
void RoutingProtocol::newRound()
{
    NS_LOG_FUNCTION (this);
    m_curRound += 1;
    NS_LOG_INFO("New round: " << m_curRound);
    m_clusterNodes.clear();
    m_inSetupPhase = true;
    m_openTimeFrame = false;

    /* Stop early if base station because it doesn't need to check if it will
     * become a BS. */
    if(m_isBS)
        return;

    /* Schedule the next round. */
    m_roundTimer.Cancel();
    m_roundTimer.Schedule(m_roundDuration);

    /* If this node was a CH in the previous round, then reset it. */
    if(m_isCluserHead)
    {
        m_isCluserHead = false;
        m_wasCH = true;
    }

    /* Everyone has been a CH, so reset everybody. */
    if(fmod(m_curRound, 1 / m_CHPercentage) < m_curRound)
        m_wasCH = false;

    m_nearestCH_addr = Ipv4Address();
    m_nearestCH_dist = 1000000;

    /* Check if the node is elected as CH. */
    runElection();

    /* Advertise to surrounding nodes if CH. */
    if(m_isCluserHead)
    {
        // Time jitter = Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0,
                            // m_advertiseDuration.GetMilliSeconds() - 25)));
        // NS_LOG_INFO("Send Ads jitter: " << jitter);
        // m_broadcastTimer.Schedule(jitter);
        SendAdvertism();

    }

    m_advertiseTimer.Cancel();
    m_advertiseTimer.Schedule(m_advertiseDuration);
}

void RoutingProtocol::sleep()
{
    m_wifiDev->GetPhy()->SetSleepMode();
}

void RoutingProtocol::resume()
{
    m_wifiDev->GetPhy()->ResumeFromSleep();
}

} //namespace leach
} //namespace ns3
