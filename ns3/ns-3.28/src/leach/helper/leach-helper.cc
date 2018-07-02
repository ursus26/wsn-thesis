/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Name: Bart de Haan
 *
 * This file is a modified version of aodv-helper.cc from the original
 * NS3 version 3.28.
 * Link: https://www.nsnam.org/doxygen/aodv-helper_8cc_source.html
 *
 * The changes that are made to this file is the replecement of AODV for LEACH
 * in order to make it work with the LEACH module.
 */
#include "leach-helper.h"
#include "ns3/leach-routing-protocol.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-list-routing.h"

namespace ns3
{

LeachHelper::LeachHelper() :
  Ipv4RoutingHelper ()
{
  m_agentFactory.SetTypeId ("ns3::leach::RoutingProtocol");
}

LeachHelper*
LeachHelper::Copy (void) const
{
  return new LeachHelper (*this);
}

Ptr<Ipv4RoutingProtocol>
LeachHelper::Create (Ptr<Node> node) const
{
  Ptr<leach::RoutingProtocol> agent = m_agentFactory.Create<leach::RoutingProtocol> ();
  node->AggregateObject (agent);
  return agent;
}

void
LeachHelper::Set (std::string name, const AttributeValue &value)
{
  m_agentFactory.Set (name, value);
}

int64_t
LeachHelper::AssignStreams (NodeContainer c, int64_t stream)
{
  int64_t currentStream = stream;
  Ptr<Node> node;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      node = (*i);
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      NS_ASSERT_MSG (ipv4, "Ipv4 not installed on node");
      Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol ();
      NS_ASSERT_MSG (proto, "Ipv4 routing not installed on node");
      Ptr<leach::RoutingProtocol> leach = DynamicCast<leach::RoutingProtocol> (proto);
      if (leach)
        {
          currentStream += leach->AssignStreams (currentStream);
          continue;
        }
      // leach may also be in a list
      Ptr<Ipv4ListRouting> list = DynamicCast<Ipv4ListRouting> (proto);
      if (list)
        {
          int16_t priority;
          Ptr<Ipv4RoutingProtocol> listProto;
          Ptr<leach::RoutingProtocol> listleach;
          for (uint32_t i = 0; i < list->GetNRoutingProtocols (); i++)
            {
              listProto = list->GetRoutingProtocol (i, priority);
              listleach = DynamicCast<leach::RoutingProtocol> (listProto);
              if (listleach)
                {
                  currentStream += listleach->AssignStreams (currentStream);
                  break;
                }
            }
        }
    }
  return (currentStream - stream);
}

}
