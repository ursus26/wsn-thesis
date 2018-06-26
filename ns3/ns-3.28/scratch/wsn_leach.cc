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
#include "ns3/file-helper.h"
#include "ns3/traced-value.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/config.h"
#include "ns3/gtk-config-store.h"
#include "ns3/stats-module.h"

#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("wsn_leach");


class NodeEnergy : public Object
{
public:
  /**
   * Register this type.
   * \return The TypeId.
   */
    static TypeId GetTypeId(void);
    NodeEnergy();
    void UpdateEnergy(double oldValue, double remainingEnergy);
    double GetRemainingEnergy();
private:
    TracedValue<double> m_remainingEnergy;
};


typedef struct Points {
    float** points;
    int nPoints;
} Points;

/* Global objects. */
EnergySourceContainer sources;
static int depletedNodes = 0;
static double batteryValue = 25;   /* Joules */
Ptr<NodeEnergy>* pNodeEnergy;
int nodeEnergySize;

NS_OBJECT_ENSURE_REGISTERED(NodeEnergy);


TypeId
NodeEnergy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NodeEnergy")
    .SetParent<Object> ()
    .SetGroupName ("wsn_leach")
    .AddConstructor<NodeEnergy> ()
    .AddTraceSource ("RemainingEnergy",
                     "Remaining energy in battery",
                     MakeTraceSourceAccessor (&NodeEnergy::m_remainingEnergy),
                     "ns3::TracedValueCallback::Double")
  ;
  return tid;
}

NodeEnergy::NodeEnergy (void)
{
  NS_LOG_FUNCTION (this);
  m_remainingEnergy = batteryValue;
}

void NodeEnergy::UpdateEnergy(double oldValue, double remainingEnergy)
{
    NS_LOG_FUNCTION(this);
    m_remainingEnergy = remainingEnergy;
    // NS_LOG_UNCOND(this << " UpdateEnergy remainingEnergy: " << remainingEnergy);
}

double NodeEnergy::GetRemainingEnergy()
{
    return m_remainingEnergy.Get();
}


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

void NodeEnergyDepletion()
{
    depletedNodes += 1;

    NS_LOG_INFO(Simulator::Now ().GetSeconds () << ", Node energy depleted. depletedNodes = " << depletedNodes);

    // std::cout << Simulator::Now ().GetSeconds () << ", Depleted nodes: " <<
    //              depletedNodes << "/" << nodeEnergySize << std::endl;
}

/* Trace function for remaining energy at node. */
void RemainingEnergy (double oldValue, double remainingEnergy)
{
  // NS_LOG_UNCOND (Simulator::Now ().GetSeconds ()
  //                << "s Current remaining energy = " << remainingEnergy << "J");

    for(int i = 0; i < nodeEnergySize; i++)
    {
        if(pNodeEnergy[i]->GetRemainingEnergy() == oldValue)
        {
            pNodeEnergy[i]->UpdateEnergy(oldValue, remainingEnergy);
            return;
        }
    }

    NS_LOG_UNCOND("Error, Couldn't find the correct NodeEnergy object.");
}

/* Trace function for total energy consumption at node. */
void TotalEnergy (double oldValue, double totalEnergy)
{
  // NS_LOG_UNCOND (Simulator::Now ().GetSeconds ()
                 // << "s Total energy consumed by radio = " << totalEnergy << "J");
}


int main (int argc, char *argv[])
{
    /**************************************************************************
     * Command line arguments
     *************************************************************************/

    /* Init commandline arguments, NOTE: some values are global because
     * energy needs to be traced. */
    uint32_t packetSize = 1024; /* bytes */
    double interval = 1;        /* seconds */
    u_int32_t maxPackets = 0;
    double simStopTime = 100.0; /* seconds */
    u_int32_t nNodes = 5;
    bool verbose = true;
    bool pcap = false;
    std::string file_path = "";

    /* Set comandline arguments and parse them. */
    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes in the network", nNodes);
    cmd.AddValue("verbose", "Outputs more info", verbose);
    cmd.AddValue("pcap", "Capture packets", pcap);
    cmd.AddValue("file", "File containing the node coords", file_path);
    cmd.AddValue("stopTime", "Stop time of the simulator", simStopTime);
    cmd.AddValue("packetSize", "Size of the packet that is being sent. Unit: byte", packetSize);
    cmd.AddValue("maxPackets", "Maximum number of packets send per node", maxPackets);
    cmd.AddValue("interval", "Interval between sending packets. Unit: seconds", interval);
    cmd.AddValue("energy", "Energy available to a node", batteryValue);
    cmd.Parse (argc, argv);

    /* Get the points. */
    Points* p = createPoints(nNodes + 1);
    read_points(p, file_path);

    /* Create objects for tracing the amount of energy in nodes. */
    nodeEnergySize = nNodes;
    pNodeEnergy = new Ptr<NodeEnergy>[nodeEnergySize];
    for(int i = 0; i < nodeEnergySize; i++)
        pNodeEnergy[i] = CreateObject<NodeEnergy>();


    /**************************************************************************
     * Logging
     *************************************************************************/

    if(verbose)
    {
        // LogComponentEnable("LeachRoutingProtocol", LOG_LEVEL_INFO);
        LogComponentEnable("wsn_leach", LOG_LEVEL_INFO);
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

    /**************************************************************************
     * MAC protocol & device creation
     *************************************************************************/

    /* Setup MAC layer */
    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns-3-ssid");
    mac.SetType("ns3::AdhocWifiMac",
                "Ssid", SsidValue (ssid));

    /* Set the wifi standard. */
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

    /* Create the sensor devices. */
    NetDeviceContainer sensorDevices;
    sensorDevices = wifi.Install(wifiPhy, mac, sensorNodes);

    /* Set the base station mac. */
    mac.SetType("ns3::AdhocWifiMac",
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
        initialAlloc->Add(Vector(p->points[i + 1][0],
                                 p->points[i + 1][1],
                                 p->points[i + 1][2]));
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


    /**************************************************************************
     * Energy model
     *************************************************************************/

    /* energy source */
    BasicEnergySourceHelper basicSourceHelper;
    basicSourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue(batteryValue));

    for(u_int32_t i = 0; i < nNodes; i++)
        sources.Add(basicSourceHelper.Install(sensorNodes.Get(i)));

    /* device energy model */
    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.SetDepletionCallback(MakeCallback(&NodeEnergyDepletion));
    DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (sensorDevices, sources);


    /**************************************************************************
     * Network protocol and IP assignment
     *************************************************************************/

    /* Enable LEACH */
    LeachHelper leach;

    Ipv4ListRoutingHelper list;
    list.Add(leach, 100);

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

        if(maxPackets > 0)
        {
            udpClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
            NS_LOG_INFO("Set maxPackets: " << maxPackets);
        }

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

    /* Enable packet capture. */
    if(pcap)
    {
        std::cout << "Capture packets" << std::endl;
        wifiPhy.EnablePcapAll("leachPhy");
    }

    /* Energy tracing. */
    int sourceID = 0;
    for (EnergySourceContainer::Iterator iter = sources.Begin();
         iter != sources.End(); iter++)
    {
        /* Trace the remaining energy and store it in our object. */
        Ptr<BasicEnergySource> basicSourcePtr = DynamicCast<BasicEnergySource>(*iter);
        basicSourcePtr->TraceConnectWithoutContext("RemainingEnergy",
                                                   MakeCallback(&RemainingEnergy));

        /* Create a config path for the trace source. */
        std::string path = "/Names/NodeEnergy";
        std::string id = std::to_string(sourceID);
        path += id;

        /* Add the trace source to the config tree. */
        Names::Add (path, pNodeEnergy[sourceID]);
        // Names::Add(path, std::to_string(sourceID), pNodeEnergy[sourceID]);

        sourceID++;
    }

    /**************************************************************************
     * Data collection
     *************************************************************************/

     /* The filehelpers need to be initialized outside the for loop. I don't
      * know why. */
     FileHelper *fileHelpers = new FileHelper[sourceID + 1];

     sourceID = 0;
     for (EnergySourceContainer::Iterator iter = sources.Begin();
          iter != sources.End(); iter++)
     {
         /* Create a path to the trace source. */
         std::string id = std::to_string(sourceID);
         std::string path = "/Names/NodeEnergy";
         path += id;
         std::string tracePath = path + "/RemainingEnergy";

         /* Set the output file. */
         fileHelpers[sourceID].ConfigureFile("wsn_leach_energy_remaining" + id,
                                   FileAggregator::COMMA_SEPARATED);

         fileHelpers[sourceID].Set2dFormat("Time (Seconds) = %.4f\tEnergy Remaining (Joule) = %.4f");

         /* Multiple files. */
         fileHelpers[sourceID].WriteProbe("ns3::DoubleProbe",
                               tracePath,
                               "Output");

         sourceID++;
     }

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
    std::cout << "Total Depleted nodes: " << depletedNodes << "/" << nNodes << std::endl;

    return 0;
}
