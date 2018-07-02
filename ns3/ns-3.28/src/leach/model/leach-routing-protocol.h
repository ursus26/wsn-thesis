/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Name: Bart de Haan
 *
 * This file is started out as a copy of aodv-routing-protocol.h but it has
 * been modified to implement the LEACH routing protocol.
 * Link to original: https://www.nsnam.org/doxygen/aodv-routing-protocol_8h.html
 *
 * The biggest changes to this file is the different variables and functions
 * needed for the LEACH protocol.
 */

#ifndef LEACH_ROUTINGPROTOCOL_H
#define LEACH_ROUTINGPROTOCOL_H

#include "leach-rtable.h"
#include "leach-rqueue.h"
#include "leach-packet.h"
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


class RoutingProtocol : public Ipv4RoutingProtocol
{
public:
    static TypeId GetTypeId(void);
    static const uint32_t LEACH_PORT;

    RoutingProtocol ();
    virtual ~RoutingProtocol();
    virtual void DoDispose();

    /* Inherited functions from Ipv4RoutingProtocol */
    Ptr<Ipv4Route> RouteOutput(Ptr<Packet> p, const Ipv4Header &header,
                               Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
    bool RouteInput(Ptr<const Packet> p, const Ipv4Header &header,
                    Ptr<const NetDevice> idev, UnicastForwardCallback ucb,
                    MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb);
    virtual void NotifyInterfaceUp(uint32_t interface);
    virtual void NotifyInterfaceDown(uint32_t interface);
    virtual void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address);
    virtual void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address);
    virtual void SetIpv4(Ptr<Ipv4> ipv4);
    virtual void PrintRoutingTable(Ptr<OutputStreamWrapper> stream,
                                   Time::Unit unit = Time::S) const;

    void SetMaxQueueTime (Time t);
    Time GetMaxQueueTime () const
    {
        return m_maxQueueTime;
    }

    void SetMaxQueueLen (uint32_t len);
    uint32_t GetMaxQueueLen () const
    {
        return m_maxQueueLen;
    }

    /**
     * Random variable stream.
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
   * NodeTraversalTime is a conservative estimate of the average one hop
   * traversal time for packets and should include queuing delays, interrupt
   * processing times and transfer times.
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

  /* IP protocol */
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

private:
    /* Start protocol operation */
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



    /**
     * Receive functions that process incoming packets.
     */

    /* Receive and process a LEACH packet. */
    void RecvLeach (Ptr<Socket> socket);

    /* Receive and process a cluster head advertisment packet. This will be
     * received by cluster nodes. */
    void RecvAdvertism(Ptr<Packet> p, Ipv4Address receiver, Ipv4Address src);

    /* Receive and process a packet that request to join a cluster. This is
     * packet will be received by a cluster head. */
    void RecvAdReply(Ptr<Packet> p, Ipv4Address origin);

    /* Receive and process a packet that contains a time slot for a cluster
     * node. */
    void RecvTimeTable(Ptr<Packet> p, Ipv4Address origin);



    ///\name Send
    //\{
    /// Forward packet from route request queue
    void SendPacketFromQueue (Ipv4Address dst, Ptr<Ipv4Route> route);


    /* Broadcast cluster advertisment to non cluster head nodes in the
    * advertisment phase. */
    void SendAdvertism();

    /* Send reply message to a cluster head in order to join its cluster. */
    void SendAdRep();

    /* Send a time slot to cluster nodes. */
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

    /* Calculates the threshold and returns this threshold which is a float
     * between 0 and 1.
     */
    float electionProbability();
    void runElection();
    void newRound();
    void setupTimerExpires();
    double DistTo(u_int32_t a_x, u_int32_t a_y);

    /* Functions that are called when a timer expires. */
    void advertisePhaseExpired();
    void replyPhaseExpired();
    void sendTimerExpired();

    /* Let the node sleep or wake up. */
    void sleep();
    void resume();

    /* Route output. This creates a route for the cluster head and cluster nodes. */
    Ptr<Ipv4Route> RouteOutputClusterHead(Ptr<Packet> p, const Ipv4Header &header,
                                          Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
    Ptr<Ipv4Route> RouteOutputClusterNode(Ptr<Packet> p, const Ipv4Header &header,
                                          Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);

};

} /* namespace leach */
} /* namespace ns3 */

#endif /* LEACH_ROUTINGPROTOCOL_H */
