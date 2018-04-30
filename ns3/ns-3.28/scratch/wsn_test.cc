/**
 * Name:    Bart de Haan
 * Id:      11044616
 * Bachelor Thesis
 */


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
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

int
main (int argc, char *argv[])
{
    /* Init commandline arguments. */
    u_int32_t nNodes = 5;
    bool verbose = true;
    std::string file_path = "";

    /* Set comandline arguments and parse them. */
    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes in the network", nNodes);
    cmd.AddValue("verbose", "Outputs more info", verbose);
    cmd.AddValue("file", "File containing the node coords", file_path);
    cmd.Parse (argc, argv);

    /* Get the points. */
    Points* p = createPoints(nNodes);
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
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer sensorDevices;
    sensorDevices = wifi.Install(wifiPhy, mac, sensorNodes);

    /* Put every sensor node into a line */
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> initialAlloc = CreateObject<ListPositionAllocator>();
    for(uint32_t i = 0; i < sensorNodes.GetN(); ++i) {
        initialAlloc->Add(Vector(p->points[i][0],
                                 p->points[i][1],
                                 p->points[i][2]));

        printf("Node %i, location: (%f, %f, %f)\n", i, p->points[i][0],
               p->points[i][1], p->points[i][2]);
    }

    mobility.SetPositionAllocator(initialAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(sensorNodes);

    /* Set the internet protocol? */
    InternetStackHelper stack;
    stack.Install(sensorNodes);

    /* Set the ip addresses of the nodes. */
    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    address.Assign(sensorDevices);

    /* Set a maximum life time of the simulator. */
    Simulator::Stop(Seconds(30.0));

    /* Run the simulation */
    Simulator::Run();
    Simulator::Destroy();

    /* Clean up. */
    deletePoints(p);

    return 0;
}
