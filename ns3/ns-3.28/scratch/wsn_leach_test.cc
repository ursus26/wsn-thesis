/**
 * Name:    Bart de Haan
 * Id:      11044616
 * Bachelor Thesis
 */


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/aodv-helper.h"
#include "ns3/dsdv-helper.h"
#include "ns3/leach-helper.h"
#include "ns3/energy-module.h"

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("wsn_leach");

typedef struct Points {
    float** points;
    int nPoints;
} Points;


Points * createPoints(int nPoints)
{
    /* Allocate the point struct. */
    Points* p = new Points;

    /* Allocate the matrix. */
    p->points = new float* [nPoints];
    for(int i = 0; i < nPoints; i++)
        p->points[i] = new float [3];

    /* Set the matrix length. */
    p->nPoints = nPoints;

    return p;
}


void deletePoints(Points* p)
{
    /* Delete the rows. */
    for(int i = 0; i < p->nPoints; i++)
    {
        delete p->points[i];
        p->points[i] = nullptr;
    }

    /* Delete the matrix. */
    delete p->points;
    p->points = nullptr;

    delete p;
    p = nullptr;
}


void read_points(Points* p, std::string file_name)
{
    std::string line = "";
    std::ifstream f;

    /* Open the file. */
    f.open(file_name);

    for(int i = 0; i < p->nPoints; i++)
    {
        /* Read the line. */
        std::getline(f, line);

        std::string::size_type comma1;
        std::string::size_type comma2;

        /* Split on the first comma. */
        comma1 = line.find(",");
        std::string split1 = line.substr(0, comma1);
        std::string split2 = line.substr(comma1 + 1);

        /* Split on the second comma. */
        comma2 = split2.find(",");
        std::string split3 = split2.substr(comma2 + 1);
        split2 = split2.substr(0, comma2);

        p->points[i][0] = std::stof(split1);
        p->points[i][1] = std::stof(split2);
        p->points[i][2] = std::stof(split3);
    }

    /* Close the file. */
    f.close();
}

// void PhyRxDrop(Ptr<Packet> old, Ptr<Packet> new)
// {
//     NS_LOG_INFO("Packet dropped at Rx");
// }

static void RxDrop (Ptr<const Packet> p)
{
    NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
}

static void RxStart (Ptr<const Packet> p)
{
    NS_LOG_UNCOND ("RxStart at " << Simulator::Now ().GetSeconds ());
}

static void RxEnd (Ptr<const Packet> p)
{
    NS_LOG_UNCOND ("RxEnd at " << Simulator::Now ().GetSeconds ());
}

static void TxDrop (Ptr<const Packet> p)
{
    NS_LOG_UNCOND ("TxDrop at " << Simulator::Now ().GetSeconds ());
}

static void TxStart (Ptr<const Packet> p)
{
    NS_LOG_UNCOND ("TxStart at " << Simulator::Now ().GetSeconds ());
}

static void TxEnd (Ptr<const Packet> p)
{
    NS_LOG_UNCOND ("TxEnd at " << Simulator::Now ().GetSeconds ());
}

int main (int argc, char *argv[])
{
    /**************************************************************************
     * Command line arguments
     *************************************************************************/

    /* Init commandline arguments. */
    uint32_t packetSize = 1024; /* bytes */
    double interval = 5.0;      /* seconds */
    u_int32_t maxPackets = 3;
    double simStopTime = 50.0;  /* seconds */
    u_int32_t nNodes = 1;
    bool verbose = true;
    std::string file_path = "";

    /* Set comandline arguments and parse them. */
    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes in the network", nNodes);
    cmd.AddValue("verbose", "Outputs more info", verbose);
    cmd.AddValue("file", "File containing the node coords", file_path);
    cmd.AddValue("stopTime", "Stop time of the simulator", simStopTime);
    cmd.AddValue ("packetSize", "Size of the packet that is being sent. Unit: byte", packetSize);
    cmd.AddValue ("maxPackets", "Maximum number of packets send per node", maxPackets);
    cmd.AddValue ("interval", "Interval between sending packets. Unit: seconds", interval);
    cmd.Parse (argc, argv);

    nNodes = 1;

    /* Get the points. */
    Points* p = createPoints(nNodes + 1);
    read_points(p, file_path);


    /**************************************************************************
     * Logging
     *************************************************************************/

    if(verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("LeachRoutingProtocol", LOG_LEVEL_INFO);
        // LogComponentEnable("YansWifiChannel", LOG_LEVEL_INFO);
        LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_FUNCTION);
        LogComponentEnable("OlsrRoutingProtocol", LOG_LEVEL_FUNCTION);
        LogComponentEnable("DsdvRoutingProtocol", LOG_LEVEL_FUNCTION);
    }


    /**************************************************************************
     * Node creation
     *************************************************************************/

    /* Create the base station. */
    NodeContainer baseStation;
    baseStation.Create(1);

    /* Create the sensor nodes. */
    NodeContainer sensorNodes;
    sensorNodes.Create(nNodes);


    /**************************************************************************
     * Physical layer
     *************************************************************************/

    /* Setup physical layer. */
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    // wifiPhy.TraceConnectWithoutContext("PhyRxDrop", MakeCallback(&PhyRxDrop));


    /**************************************************************************
     * MAC protocol & device creation
     *************************************************************************/

    /* Setup MAC layer */
    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue (ssid));

    /* Set the wifi standard. */
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

    /* Create the sensor devices. */
    NetDeviceContainer sensorDevices;
    sensorDevices = wifi.Install(wifiPhy, mac, sensorNodes);

    /* Set the base station mac. */
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue (ssid));

    /* Create the base station device. */
    NetDeviceContainer bsDevices;
    bsDevices = wifi.Install(wifiPhy, mac, baseStation);


    /**************************************************************************
     * Mobility
     *************************************************************************/

    /* Put every sensor node on a plane */
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> initialAlloc = CreateObject<ListPositionAllocator>();
    for(uint32_t i = 0; i < sensorNodes.GetN(); ++i) {
        initialAlloc->Add(Vector(10, 0, 0));
    }
    mobility.SetPositionAllocator(initialAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensorNodes);

    /* Set the basestation location. */
    Ptr<ListPositionAllocator> initialAlloc2 = CreateObject<ListPositionAllocator>();
    initialAlloc2->Add(Vector(0, 0, 0));
    mobility.SetPositionAllocator(initialAlloc2);
    mobility.Install(baseStation);


    /**************************************************************************
     * Energy model
     *************************************************************************/

    /* energy source */
    BasicEnergySourceHelper basicSourceHelper;
    basicSourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue(10));
    EnergySourceContainer sources = basicSourceHelper.Install(sensorNodes);

    /* device energy model */
    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.0174));
    DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (sensorDevices, sources);


    /**************************************************************************
     * Network protocol and IP assignment
     *************************************************************************/

    /* TODO Cleanup */
    OlsrHelper olsr;
    AodvHelper aodv;
    Ipv4StaticRoutingHelper staticRouting;

    /* Enable LEACH */
    LeachHelper leach;

    Ipv4ListRoutingHelper list;
    /* TODO cleanup */
    // list.Add(staticRouting, 0);
    // list.Add(olsr, 10);
    // list.Add(aodv, 5);
    list.Add(leach, 20);

    /* Set the internet protocol */
    InternetStackHelper stack;
    stack.SetRoutingHelper(list);
    stack.Install(sensorNodes);
    stack.Install(baseStation);

    /* Set the ip addresses of the nodes. */
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer bsInterfaces = address.Assign(bsDevices);
    Ipv4InterfaceContainer snInterfaces = address.Assign(sensorDevices);


    /**************************************************************************
     * Application
     *************************************************************************/

    /* Create a container for the base station & sensor application. */
    ApplicationContainer basestationApps;
    ApplicationContainer sensorApps;

    /* Setup the base station application. */
    uint32_t port = 9;
    UdpServerHelper udpServerHelper(port);
    basestationApps.Add(udpServerHelper.Install(baseStation.Get(0)));

    /* Install the sensor application. */
    for(u_int32_t i = 0; i < nNodes; i++)
    {
        /* Setup the client application. */
        UdpClientHelper udpClient(bsInterfaces.GetAddress(0), port);
        // udpClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        udpClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        udpClient.SetAttribute("Interval", TimeValue (Seconds (interval)));
        udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

        /* Add the application to the containers. */
        sensorApps.Add(udpClient.Install(sensorNodes.Get(i)));
    }

    /* Start en set the stop time of the applications. */
    basestationApps.Start(Seconds(1));
    basestationApps.Stop(Seconds(simStopTime));
    sensorApps.Start(Seconds(1));
    sensorApps.Stop(Seconds(simStopTime));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();


    /**************************************************************************
     * Tracing
     *************************************************************************/

    wifiPhy.EnablePcapAll("leachPhy");

    for(u_int32_t i = 0; i < nNodes; i++)
    {
        sensorDevices.Get(i)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));
        sensorDevices.Get(i)->TraceConnectWithoutContext ("PhyRxStart", MakeCallback (&RxStart));
        sensorDevices.Get(i)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxEnd));

        sensorDevices.Get(i)->TraceConnectWithoutContext ("PhyTxDrop", MakeCallback (&TxDrop));
        sensorDevices.Get(i)->TraceConnectWithoutContext ("PhyTxStart", MakeCallback (&TxStart));
        sensorDevices.Get(i)->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&TxEnd));
    }

    bsDevices.Get(0)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));
    bsDevices.Get(0)->TraceConnectWithoutContext ("PhyRxStart", MakeCallback (&RxStart));
    bsDevices.Get(0)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxEnd));
    bsDevices.Get(0)->TraceConnectWithoutContext ("PhyTxDrop", MakeCallback (&TxDrop));
    bsDevices.Get(0)->TraceConnectWithoutContext ("PhyTxStart", MakeCallback (&TxStart));
    bsDevices.Get(0)->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&TxEnd));


    /**************************************************************************
     * Simulation
     *************************************************************************/

    /* Set a maximum life time of the simulator. */
    Simulator::Stop(Seconds(simStopTime));

    /* Run the simulation */
    Simulator::Run();

    /* Get the amount of energy consumed and left in the battery. */
    for (DeviceEnergyModelContainer::Iterator iter = deviceModels.Begin();
         iter != deviceModels.End(); iter ++)
    {
        double energyConsumed = (*iter)->GetTotalEnergyConsumption ();
        NS_LOG_UNCOND ("End of simulation (" << Simulator::Now ().GetSeconds ()
                       << "s) Total energy consumed by radio = " << energyConsumed << "J");
    }

    int i = 1;
    for (EnergySourceContainer::Iterator iter = sources.Begin();
         iter != sources.End(); iter++)
    {
        double energyRemaining = (*iter)->GetRemainingEnergy();
        NS_LOG_UNCOND ("Node " << i << " Total energy remaining = " << energyRemaining << "J");
        i++;
    }

    Simulator::Destroy();

    /* Clean up. */
    deletePoints(p);


    /* Print results. */
    printf("BS received packets: %lu\n", udpServerHelper.GetServer()->GetReceived());
    printf("BS lost packets: %u\n", udpServerHelper.GetServer()->GetLost());

    return 0;
}
