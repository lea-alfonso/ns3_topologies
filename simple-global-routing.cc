/*
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
 */

//
// Network topology
//
//  n0(10.1.1.1)
//     \ 5 Mb/s, 4ms
//      \          1.5Mb/s, 10ms -> 2ms (10ms)
//       n2 -------------------------n3 (10.1.3.1)
//      /
//     / 5 Mb/s, 4ms
//   n1(10.1.2.1)
//
// - all links are point-to-point links with indicated one-way BW/delay
// - CBR/UDP flows from n0 to n3, and from n3 to n1
// - FTP/TCP flow from n0 to n3, starting at time 1.2 to time 1.35 sec.
// - UDP packet size of 210 bytes, with per-packet interval 0.00375 sec.
//   (i.e., DataRate of 448,000 bps)
// - DropTail queues
// - Tracing of queues and packet receptions to file "simple-global-routing.tr"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/big-brother-flow-probe.h"
#include "ns3/ipv4-flow-probe.h"
#include "big-brother-tracker.cc"
#include <tinyxml2.h>
using namespace tinyxml2;


#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleGlobalRoutingExample");


int
main(int argc, char* argv[])
{
    // Users may find it convenient to turn on explicit debugging
    // for selected modules; the below lines suggest how to do this
#if 0
  LogComponentEnable ("SimpleGlobalRoutingExample", LOG_LEVEL_INFO);
#endif

    // Set up some default values for the simulation.  Use the
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(210));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("448kb/s"));

    // DefaultValue::Bind ("DropTailQueue::m_maxPackets", 30);

    // Allow the user to override any of the defaults and the above
    // DefaultValue::Bind ()s at run-time, via command-line arguments
    CommandLine cmd(__FILE__);
    bool enableFlowMonitor = false;
    cmd.AddValue("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.Parse(argc, argv);

    // Here, we will explicitly create four nodes.  In more sophisticated
    // topologies, we could configure a node factory.
    NS_LOG_INFO("Create nodes.");
    NodeContainer c;
    c.Create(4);
    NodeContainer n0n2 = NodeContainer(c.Get(0), c.Get(2));
    NodeContainer n1n2 = NodeContainer(c.Get(1), c.Get(2));
    NodeContainer n3n2 = NodeContainer(c.Get(3), c.Get(2));

    InternetStackHelper internet;
    internet.Install(c);

    // We create the channels first without any IP addressing information
    NS_LOG_INFO("Create channels.");
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("4ms"));
    NetDeviceContainer d0d2 = p2p.Install(n0n2);

    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    p2p.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer d3d2 = p2p.Install(n3n2);
    // We schedule a change of delay in the link
    Simulator::Schedule(Seconds(11), &ChangeLinkDelay, d3d2.Get(0), MilliSeconds(2));


    // Later, we add IP addresses.
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = ipv4.Assign(d0d2);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i3i2 = ipv4.Assign(d3d2);

    // Create router nodes, initialize routing database and set up the routing
    // tables in the nodes.
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create the OnOff application to send UDP datagrams of size
    // 210 bytes at a rate of 448 Kb/s
    NS_LOG_INFO("Create Applications.");
    uint16_t port = 9; // Discard port (RFC 863)
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      Address(InetSocketAddress(i3i2.GetAddress(0), port)));
    onoff.SetConstantRate(DataRate("448kb/s"));
    ApplicationContainer apps = onoff.Install(c.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Create a packet sink to receive these packets
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    apps = sink.Install(c.Get(3));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Create a similar flow from n3 to n1, starting at time 1.1 seconds
    // TODO: Add this without having the system duplicate the measured time
    onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(i1i2.GetAddress(0), port)));
    apps = onoff.Install(c.Get(3));
    apps.Start(Seconds(10.1));
    apps.Stop(Seconds(20.0));

    // Create a packet sink to receive these packets
    apps = sink.Install(c.Get(1));
    apps.Start(Seconds(10.1));
    apps.Stop(Seconds(20.0));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));
    p2p.EnablePcapAll("simple-global-routing");

    // Flow Monitor
    FlowMonitorHelper flowmonHelper;
    if (enableFlowMonitor)
    {
        flowmonHelper.InstallAll();

        flowmonHelper.SerializeToXmlFile("simple-global-routing.flowmon", false, false);
        Ptr<ns3::FlowMonitor> monitor = flowmonHelper.GetMonitor();

        // We clean any previous measures_doc that could be present
        std::string filename = "./contrib/nr/examples/experiments/simple-global-routing_node_to_node_validation.xml";
        Simulator::Schedule(Seconds(10.1), &nodeToNodeTriggerVoidWrapper, monitor,filename);
        Simulator::Schedule(Seconds(20.1), &nodeToNodeTriggerVoidWrapper, monitor,filename);
    }

    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(25));
    Simulator::Run();
    NS_LOG_INFO("Done.");

    std::cout << "Real Nodes in the simulationt:" << std::endl;
    for (auto i = NodeList::Begin(); i != NodeList::End(); ++i)
    {
        Ptr<Node> node = *i;
        std::cout << node->GetId() << std::endl;
    }

    Simulator::Destroy();
    return 0;
}
