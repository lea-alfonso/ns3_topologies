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
//     \ 5 Mb/s, 2ms
//      \          1.5Mb/s, 10ms
//       n2 -------------------------n3 (10.1.3.1)
//      /
//     / 5 Mb/s, 2ms
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


#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleGlobalRoutingExample");

bool compareByTime(const std::pair<uint32_t, ns3::Time>& a, const std::pair<uint32_t, ns3::Time>& b) {
    return a.second.Compare(b.second) < 0;
}

void nodeToNodeTrigger(Ptr<FlowMonitor> monitor)
// This function implements the first step in looking for the cause of a bad metric
// Given a flow_id, we look for the worst performing FlowProbes
{
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    FlowMonitor::FlowProbeContainer probes = monitor->GetAllProbes();
    std::map<std::pair<uint32_t,uint32_t>,ns3::Time> nodeToNodeDelay;

    for (FlowMonitor::FlowStatsContainer::const_iterator flowStatsIt = stats.begin();
        flowStatsIt != stats.end();
        ++flowStatsIt)
    {
        FlowId flowId = flowStatsIt->first;  
        std::map<uint32_t,std::vector<std::pair<uint32_t,ns3::Time>>> perPacketStats;

    // FlowMonitor::FlowStatsContainer::iterator flowStatsIt = stats.find(flowId);
    // if (flowStatsIt != stats.end())
    // {
        // We iterate over all packets that have been received
        uint32_t maxPacketId = flowStatsIt->second.rxPackets;
        for (uint32_t packetId = 0; packetId < maxPacketId; ++packetId)
        {
            std::map<uint32_t,FlowProbe::FlowStats> nodeProbeStats;
            for (ns3::Ptr<ns3::FlowProbe> probe : probes)
            {
                FlowProbe::Stats probeStats = probe->GetStats();
                Ptr<BigBrotherFlowProbe> bigBrotherProbe = DynamicCast<BigBrotherFlowProbe>(probe);
                // We only work measure in the probes that are ipv4/bigBrother
                if (bigBrotherProbe)
                {
                    // std::cout << "bigBrotherProbe id:" << bigBrotherProbe->m_nodeId << std::endl;

                    std::map<std::pair<uint32_t,uint32_t>, FlowProbe::FlowStats>::const_iterator it = bigBrotherProbe->m_perPacketStats.find(std::make_pair(flowId, packetId));
                    if (it != bigBrotherProbe->m_perPacketStats.end())
                    {
                        FlowProbe::FlowStats flowProbeStats = it->second;
                        // std::cout << "bigBrotherProbe delay:" << flowProbeStats.delayFromFirstProbeSum << std::endl;

                        std::map<uint32_t,std::vector<std::pair<uint32_t,ns3::Time>>>::iterator perPacketStatsIt = perPacketStats.find(packetId);
                        std::vector<std::pair<uint32_t,ns3::Time>> nodeAcc;
                        if (perPacketStatsIt != perPacketStats.end())
                        {
                            // std::cout << "packetId already in perpacketstats, packetId=" << packetId << std::endl;
                            nodeAcc = perPacketStatsIt->second;
                            // for(std::pair<uint32_t,ns3::Time> it : nodeAcc)
                            // {
                            //     std::cout << "Node id:" << it.first << "Delay" << it.second << std::endl;
                            // }
                        }
                        nodeAcc.push_back(std::make_pair(bigBrotherProbe->m_nodeId,flowProbeStats.delayFromFirstProbeSum));
                        // std::cout << "Node ACC after updating:" << std::endl;
                        // for(std::pair<uint32_t,ns3::Time> it : nodeAcc)
                        // {
                        //     std::cout << "Node id:" << it.first << "Delay" << it.second << std::endl;
                        // }
                        try 
                        {
                            perPacketStats.insert_or_assign(packetId, nodeAcc);
                        } catch (const std::exception& e)
                        {
                            std::cerr << "Error inserting into perPacketStats: " << e.what() << std::endl;
                        }
                    }

                } 
            }
        
        }
        for(std::map<uint32_t,std::vector<std::pair<uint32_t,ns3::Time>>>::const_iterator it = perPacketStats.begin(); it != perPacketStats.end(); ++it)
        {   
            std::vector<std::pair<uint32_t,ns3::Time>> nodesDelay = it->second;
            // for(std::pair<uint32_t,ns3::Time> pairIt : nodesDelay)
            // {
            //     std::cout << "\tNode id: " << pairIt.first << " Delay: " << pairIt.second << "\n" << std::endl;
            // }
            std::sort(nodesDelay.begin(), nodesDelay.end(), compareByTime);
            // std::cout << "Delays sorted" << std::endl;
            // for(std::pair<uint32_t,ns3::Time> pairIt : nodesDelay)
            // {
            //     std::cout << "\tNode id: " << pairIt.first << " Delay: " << pairIt.second << "\n" << std::endl;
            // }
            for(size_t i = 0; i < nodesDelay.size() - 1;++i)
            {
                // We insert the pairs with the lesser nodeId first, and the delay of the second
                uint32_t firstNode;
                uint32_t secondNode;
                if (nodesDelay[i].first < nodesDelay[i+1].first)
                {
                    firstNode = nodesDelay[i].first ;
                    secondNode = nodesDelay[i+1].first ;
                } else 
                {
                    firstNode = nodesDelay[i+1].first ;
                    secondNode = nodesDelay[i].first ;
                }
                std::map<std::pair<uint32_t,uint32_t>,ns3::Time>::const_iterator findIt = nodeToNodeDelay.find(std::make_pair(firstNode,secondNode));
                ns3::Time delayAcc;
                if (findIt != nodeToNodeDelay.end())
                {
                    delayAcc = findIt->second;
                }
                //The measure of delay to every node-to-node metric, is the avg for every package
                delayAcc += (nodesDelay[i+1].second - nodesDelay[i].second)/maxPacketId;
                nodeToNodeDelay.insert_or_assign(std::make_pair(firstNode,secondNode),delayAcc );
            }
        }
    }
    for(std::map<std::pair<uint32_t,uint32_t>,ns3::Time>::iterator nodeToNodeDelayIt = nodeToNodeDelay.begin();nodeToNodeDelayIt != nodeToNodeDelay.end() ; ++nodeToNodeDelayIt)
    {
        std::cout<< "("<< nodeToNodeDelayIt->first.first << "," << nodeToNodeDelayIt->first.second << ") -> " << nodeToNodeDelayIt->second << std::endl;
    }
}

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
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer d0d2 = p2p.Install(n0n2);

    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    p2p.SetDeviceAttribute("DataRate", StringValue("1500kbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer d3d2 = p2p.Install(n3n2);

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
    onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(i1i2.GetAddress(0), port)));
    apps = onoff.Install(c.Get(3));
    apps.Start(Seconds(1.1));
    apps.Stop(Seconds(10.0));

    // Create a packet sink to receive these packets
    apps = sink.Install(c.Get(1));
    apps.Start(Seconds(1.1));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));
    p2p.EnablePcapAll("simple-global-routing");

    // Flow Monitor
    FlowMonitorHelper flowmonHelper;
    if (enableFlowMonitor)
    {
        flowmonHelper.InstallAll();
    }

    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(11));
    Simulator::Run();
    NS_LOG_INFO("Done.");

    std::cout << "Real Nodes in the simulationt:" << std::endl;
    for (auto i = NodeList::Begin(); i != NodeList::End(); ++i)
    {
        Ptr<Node> node = *i;
        std::cout << node->GetId() << std::endl;
    }

    if (enableFlowMonitor)
    {
        flowmonHelper.SerializeToXmlFile("simple-global-routing.flowmon", false, false);
        Ptr<ns3::FlowMonitor> monitor = flowmonHelper.GetMonitor();
        nodeToNodeTrigger(monitor);
    }

    Simulator::Destroy();
    return 0;
}
