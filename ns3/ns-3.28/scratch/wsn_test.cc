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


#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("wsn_test");

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

int main (int argc, char *argv[])
{
    /* Init commandline arguments. */
    uint32_t packetSize = 1024; /* bytes */
    double interval = 1.0;      /* seconds */
    u_int32_t maxPackets = 2;
    double simStopTime = 100.0; /* seconds */
    u_int32_t nNodes = 5;
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

    /* Get the points. */
    Points* p = createPoints(nNodes + 1);
    read_points(p, file_path);

    /* Set verbose */
    if(verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    /* Create the base station. */
    NodeContainer baseStation;
    baseStation.Create(1);

    /* Create the sensor nodes. */
    NodeContainer sensorNodes;
    sensorNodes.Create(nNodes);

    /* Setup wifi physical layer. */
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    /* Setup the wifi MAC layer. */
    Ssid ssid = Ssid ("ns-3-ssid");
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
    WifiMacHelper mac;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue (ssid));

    NetDeviceContainer sensorDevices;
    sensorDevices = wifi.Install(wifiPhy, mac, sensorNodes);

    /* Set the base station mac. */
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue (ssid));
    NetDeviceContainer bsDevices;
    bsDevices = wifi.Install(wifiPhy, mac, baseStation);

    /* Put every sensor node into a line */
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> initialAlloc = CreateObject<ListPositionAllocator>();
    for(uint32_t i = 0; i < sensorNodes.GetN(); ++i) {
        initialAlloc->Add(Vector(p->points[i + 1][0],
                                 p->points[i + 1][1],
                                 p->points[i + 1][2]));

        printf("Node %i, location: (%f, %f, %f)\n", i, p->points[i + 1][0],
               p->points[i + 1][1], p->points[i + 1][2]);
    }

    mobility.SetPositionAllocator(initialAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensorNodes);

    /* Set the basestation location. */
    Ptr<ListPositionAllocator> initialAlloc2 = CreateObject<ListPositionAllocator>();
    initialAlloc2->Add(Vector(p->points[0][0],
                              p->points[0][1],
                              p->points[0][2]));
    mobility.SetPositionAllocator(initialAlloc2);
    mobility.Install(baseStation);

    /* Enable OLSR */
    OlsrHelper olsr;
    Ipv4StaticRoutingHelper staticRouting;

    Ipv4ListRoutingHelper list;
    list.Add (staticRouting, 0);
    list.Add (olsr, 10);

    /* Set the internet protocol? */
    InternetStackHelper stack;
    stack.SetRoutingHelper(list);
    stack.Install(sensorNodes);
    stack.Install(baseStation);

    /* Set the ip addresses of the nodes. */
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer bsInterfaces = address.Assign(bsDevices);
    Ipv4InterfaceContainer snInterfaces = address.Assign(sensorDevices);

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
        udpClient.SetAttribute("MaxPackets", UintegerValue(maxPackets + 1));
        udpClient.SetAttribute("Interval", TimeValue (Seconds (interval)));
        udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

        /* Add the application to the containers. */
        sensorApps.Add(udpClient.Install(sensorNodes.Get(i)));
    }

    /* Start en set the stop time of the applications. */
    basestationApps.Start(Seconds(0.));
    basestationApps.Stop(Seconds(simStopTime));
    sensorApps.Start(Seconds(0.));
    sensorApps.Stop(Seconds(simStopTime));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    /* Set a maximum life time of the simulator. */
    Simulator::Stop(Seconds(simStopTime));

    /* Run the simulation */
    Simulator::Run();
    Simulator::Destroy();

    /* Clean up. */
    deletePoints(p);

    /* Print results. */
    printf("BS received packets: %lu\n", udpServerHelper.GetServer()->GetReceived());
    printf("BS lost packets: %u\n", udpServerHelper.GetServer()->GetLost());

    return 0;
}
