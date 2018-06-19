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
#ifndef LEACH_ROUTINGPROTOCOL_H
#define LEACH_ROUTINGPROTOCOL_H

#include "leach-rtable.h"
#include "leach-rqueue.h"
#include "leach-packet.h"
#include "leach-neighbor.h"
#include "leach-dpd.h"
#include "ns3/node.h"
#include "ns3/random-variable-stream.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/wifi-net-device.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/applications-module.h"
#include "ns3/type-id.h"
#include <list>
#include <map>

namespace ns3 {
namespace leach {
/**
 * \ingroup leach
 *
 * \brief LEACH routing protocol
 */
class RoutingProtocol : public Ipv4RoutingProtocol
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  static const uint32_t LEACH_PORT;

  /// constructor
  RoutingProtocol ();
  virtual ~RoutingProtocol ();
  virtual void DoDispose ();

  // Inherited from Ipv4RoutingProtocol
  Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
  bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                   UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                   LocalDeliverCallback lcb, ErrorCallback ecb);
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const;

  // Handle protocol parameters
  /**
   * Get maximum queue time
   * \returns the maximum queue time
   */
  Time GetMaxQueueTime () const
  {
    return m_maxQueueTime;
  }
  /**
   * Set the maximum queue time
   * \param t the maximum queue time
   */
  void SetMaxQueueTime (Time t);
  /**
   * Get the maximum queue length
   * \returns the maximum queue length
   */
  uint32_t GetMaxQueueLen () const
  {
    return m_maxQueueLen;
  }
  /**
   * Set the maximum queue length
   * \param len the maximum queue length
   */
  void SetMaxQueueLen (uint32_t len);

  /**
   * Assign a fixed random variable stream number to the random variables
   * used by this model.  Return the number of streams (possibly zero) that
   * have been assigned.
   *
   * \param stream first stream index to use
   * \return the number of stream indices assigned by this model
   */
  int64_t AssignStreams (int64_t stream);

protected:
  virtual void DoInitialize (void);
private:
  uint16_t m_ttlStart;                ///< Initial TTL value for RREQ.
  uint16_t m_ttlIncrement;            ///< TTL increment for each attempt using the expanding ring search for RREQ dissemination.
  uint16_t m_ttlThreshold;            ///< Maximum TTL value for expanding ring search, TTL = NetDiameter is used beyond this value.
  uint16_t m_timeoutBuffer;           ///< Provide a buffer for the timeout.

  Time m_activeRouteTimeout;          ///< Period of time during which the route is considered to be valid.

  /**
   *  NodeTraversalTime is a conservative estimate of the average one hop traversal time for packets
   *  and should include queuing delays, interrupt processing times and transfer times.
   */
  Time m_nodeTraversalTime;
  Time m_netTraversalTime;             ///< Estimate of the average net traversal time.
  Time m_pathDiscoveryTime;            ///< Estimate of maximum time needed to find route in network.
  Time m_myRouteTimeout;               ///< Value of lifetime field in RREP generating by this node.


  uint32_t m_maxQueueLen;              ///< The maximum number of packets that we allow a routing protocol to buffer.
  Time m_maxQueueTime;                 ///< The maximum period of time that a routing protocol is allowed to buffer a packet for.
  //\}
  Time m_queueTime;
  Timer m_queueTimer;
  void queueTimerExpired();

  /// IP protocol
  Ptr<Ipv4> m_ipv4;
  /// Raw unicast socket per each IP interface, map socket -> iface address (IP + mask)
  std::map< Ptr<Socket>, Ipv4InterfaceAddress > m_socketAddresses;
  /// Raw subnet directed broadcast socket per each IP interface, map socket -> iface address (IP + mask)
  std::map< Ptr<Socket>, Ipv4InterfaceAddress > m_socketSubnetBroadcastAddresses;
  /// Loopback device used to defer RREQ until packet will be fully formed
  Ptr<NetDevice> m_lo;

  /// Routing table
  RoutingTable m_routingTable;
  /// A "drop-front" queue used by the routing layer to buffer packets to which it does not have a route.
  RequestQueue m_queue;
  /// Broadcast ID
  uint32_t m_requestId;
  /// Request sequence number
  uint32_t m_seqNo;
  /// Handle neighbors
  Neighbors m_nb;

private:
  /// Start protocol operation
  void Start ();
  /**
   * Queue packet and send route request
   *
   * \param p the packet to route
   * \param header the IP header
   * \param ucb the UnicastForwardCallback function
   * \param ecb the ErrorCallback function
   */
  void DeferredRouteOutput (Ptr<const Packet> p, const Ipv4Header & header, UnicastForwardCallback ucb, ErrorCallback ecb);
  /**
   * If route exists and is valid, forward packet.
   *
   * \param p the packet to route
   * \param header the IP header
   * \param ucb the UnicastForwardCallback function
   * \param ecb the ErrorCallback function
   * \returns true if forwarded
   */
  bool Forwarding (Ptr<const Packet> p, const Ipv4Header & header, UnicastForwardCallback ucb, ErrorCallback ecb);
  /**
   * Test whether the provided address is assigned to an interface on this node
   * \param src the source IP address
   * \returns true if the IP address is the node's IP address
   */
  bool IsMyOwnAddress (Ipv4Address src);
  /**
   * Find unicast socket with local interface address iface
   *
   * \param iface the interface
   * \returns the socket associated with the interface
   */
  Ptr<Socket> FindSocketWithInterfaceAddress (Ipv4InterfaceAddress iface) const;
  /**
   * Find subnet directed broadcast socket with local interface address iface
   *
   * \param iface the interface
   * \returns the socket associated with the interface
   */
  Ptr<Socket> FindSubnetBroadcastSocketWithInterfaceAddress (Ipv4InterfaceAddress iface) const;
  /**
   * Create loopback route for given header
   *
   * \param header the IP header
   * \param oif the output interface net device
   * \returns the route
   */
  Ptr<Ipv4Route> LoopbackRoute (const Ipv4Header & header, Ptr<NetDevice> oif) const;

  ///\name Receive control packets
  //\{
  /// Receive and process control packet
  void RecvLeach (Ptr<Socket> socket);
  /// Receive and process control packet
  void RecvAdvertism(Ptr<Packet> p, Ipv4Address receiver, Ipv4Address src);

  void RecvAdReply(Ptr<Packet> p, Ipv4Address origin);

  void RecvTimeTable(Ptr<Packet> p, Ipv4Address origin);

  ///\name Send
  //\{
  /// Forward packet from route request queue
  void SendPacketFromQueue (Ipv4Address dst, Ptr<Ipv4Route> route);

  /// Send hello
  // void SendHello ();
  /// Send cluster head advertism
  void SendAdvertism();

  /* Send reply to cluster head. */
  void SendAdRep();

  void SendTimeTable();

  void SendFromQueue();

  /**
   * Send packet to destination scoket
   * \param socket - destination node socket
   * \param packet - packet to send
   * \param destination - destination node IP address
   */
  void SendTo (Ptr<Socket> socket, Ptr<Packet> packet, Ipv4Address destination);

  /// Provides uniform random variables.
  Ptr<UniformRandomVariable> m_uniformRandomVariable;
  /// Keep track of the last bcast time
  Time m_lastBcastTime;


    /**
     * LEACH variables
     */

    /* Cluster head information */
    double      m_CHPercentage;
    bool        m_isCluserHead;
    bool        m_wasCH;
    Ipv4Address m_nearestCH_addr;
    double      m_nearestCH_dist;
    std::list<Ipv4Address> m_clusterNodes;

    /* Cluster node information */
    Timer m_sleepNode;

    /* Round variables */
    Time        m_roundDuration;
    Timer       m_roundTimer;
    u_int32_t   m_curRound;

    /* Setup variables */
    bool    m_inSetupPhase;
    Time    m_setupDuration;    /* Might be useless */
    Timer   m_setupTimer;       /* Might also be useless */
    Time    m_advertiseDuration;
    Timer   m_advertiseTimer;
    Timer   m_broadcastTimer;
    Time    m_replyDuration;
    Timer   m_replyTimer;
    Timer   m_sendTimer;
    bool    m_openTimeFrame;
    Time    m_sendTime;

    /* Node information */
    u_int32_t   m_x;
    u_int32_t   m_y;
    Ipv4Address m_myAddr;
    bool        m_isBS;
    Ptr<WifiNetDevice> m_wifiDev;

    /**
    * \brief Calculates the threshold.
    * \return the threshold float between 0 and 1.
    */
    float electionProbability();
    void runElection();
    void newRound();
    void setupTimerExpires();
    double DistTo(u_int32_t a_x, u_int32_t a_y);

    void advertisePhaseExpired();
    void replyPhaseExpired();
    void sendTimerExpired();

    /* Let the node sleep or wake up. */
    void sleep();
    void resume();

    /* Route output */
    Ptr<Ipv4Route> RouteOutputClusterHead(Ptr<Packet> p, const Ipv4Header &header,
                                          Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
    Ptr<Ipv4Route> RouteOutputClusterNode(Ptr<Packet> p, const Ipv4Header &header,
                                          Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);

};

} //namespace leach
} //namespace ns3

#endif /* LEACH_ROUTINGPROTOCOL_H */
