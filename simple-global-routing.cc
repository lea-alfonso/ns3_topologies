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
//      \          1.5Mb/s, 10ms
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
#include <tinyxml2.h>
using namespace tinyxml2;


#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleGlobalRoutingExample");


bool compareByTime(const std::pair<uint32_t, ns3::Time>& a, const std::pair<uint32_t, ns3::Time>& b) {
    return a.second.Compare(b.second) < 0;
}

void ChangeLinkDelay(Ptr<NetDevice> device, Time newDelay) {
    std::cout << Simulator::Now() <<" Changing link delay!" << std::endl;
    Ptr<PointToPointChannel> p2pChannel = device->GetChannel()->GetObject<PointToPointChannel>();
    if (p2pChannel) {
        p2pChannel->SetAttribute("Delay", TimeValue(newDelay));
    } else {
        NS_LOG_ERROR("Failed to get PointToPointChannel object from the device.");
    }
}

void nodeToNodeTrigger(Ptr<FlowMonitor> monitor)
// This function implements the first step in looking for the cause of a bad metric
// Given a flow_id, we look for the worst performing FlowProbes
{
    XMLDocument measurments_doc; 
    XMLError result = measurments_doc.LoadFile("/home/leandro/Scripts/ns-3-dev/simple_global_routing_node_to_node_delays.xml");
    if (result != XML_SUCCESS && result != XML_ERROR_EMPTY_DOCUMENT) {
        // Handle error
        std::cerr << "Error loading file: " << measurments_doc.ErrorIDToName(result) << std::endl;
        return;
    } 
    // --------
    std::cout << "Node To node trigger:" << std::endl;
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    FlowMonitor::FlowProbeContainer probes = monitor->GetAllProbes();
    std::map<std::pair<uint32_t,uint32_t>,ns3::Time> nodeToNodeDelay;

    // Get the root element
    XMLElement* root = measurments_doc.FirstChildElement("network-measurements");
    if (!root) {
        // Create the root element if it doesn't exist
        root = measurments_doc.NewElement("network-measurements");
        measurments_doc.InsertEndChild(root);
    }

    XMLElement* delaysElement = root->FirstChildElement("delays");
    if (!delaysElement) {
        // Create the 'delays' element if it doesn't exist
        delaysElement = measurments_doc.NewElement("delays");
        root->InsertEndChild(delaysElement);
    }

    for (FlowMonitor::FlowStatsContainer::const_iterator flowStatsIt = stats.begin();
        flowStatsIt != stats.end();
        ++flowStatsIt)
    {
        FlowId flowId = flowStatsIt->first;  
        std::cout << "Iteration flowId = " << flowId << std::endl;
        /* The variable perPacketStats is meant to be the structure that'll keep the information
        of the path each packet went through, as well as how much delay since first transmission each probe reported.
        Because of this, it's a map of packet Ids to a vector of pairs, where each pair stores a nodeId
        and a time value, representing the delay introduced measured so far by the time the packet reached 
        each node.  */
        std::map<uint32_t,std::vector<std::pair<uint32_t,ns3::Time>>> perPacketStats;

        uint32_t maxPacketId = flowStatsIt->second.rxPackets;
        std::cout << "\tmaxPacketId = " << maxPacketId << std::endl;

        /* 
        For each packet sent(not necessarely recived) in the flow.We'll
            1- Access each probe involved in the flow
            2- Check if the specific packet went through it
            3- If it did, we'll extract the delay from the bigBrotherStats.
            4- Create an adequate entry for perPacketStats
        */
        for (uint32_t packetId = 0; packetId < maxPacketId; ++packetId)
        {
            std::map<uint32_t,FlowProbe::FlowStats> nodeProbeStats;
            // Step 1
            for (ns3::Ptr<ns3::FlowProbe> probe : probes)
            {
                FlowProbe::Stats probeStats = probe->GetStats();
                Ptr<BigBrotherFlowProbe> bigBrotherProbe = DynamicCast<BigBrotherFlowProbe>(probe);
                // We only measure in the probes that are ipv4/bigBrother
                if (bigBrotherProbe)
                {
                    std::map<std::pair<uint32_t,uint32_t>, FlowProbe::FlowStats>::const_iterator it = bigBrotherProbe->m_perPacketStats.find(std::make_pair(flowId, packetId));
                    // Step 2
                    if (it != bigBrotherProbe->m_perPacketStats.end())
                    {
                        // Step 3
                        FlowProbe::FlowStats flowProbeStats = it->second;
                        // Now we'll insert the extracted delay to the corresponding packet id's entry's vector
                        // While being carefull of creating this vector if it doesn't yet exist, and appending 
                        // to it otherwise.
                        std::map<uint32_t,std::vector<std::pair<uint32_t,ns3::Time>>>::iterator perPacketStatsIt = perPacketStats.find(packetId);
                        std::vector<std::pair<uint32_t,ns3::Time>> nodeAcc;
                        if (perPacketStatsIt != perPacketStats.end())
                        {
                            nodeAcc = perPacketStatsIt->second;
                        }
                        // Observation: We're storing the dealy from first probe of each node
                        nodeAcc.push_back(std::make_pair(bigBrotherProbe->m_nodeId,flowProbeStats.delayFromFirstProbeSum));
                        try 
                        {
                            // Step 4
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
            std::sort(nodesDelay.begin(), nodesDelay.end(), compareByTime);
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
                //TODO: Maybe worth rethinking this shit innit?
                //The measure of delay to every node-to-node metric, is the avg for every package
                delayAcc = (nodesDelay[i+1].second - nodesDelay[i].second); 
                // delayAcc += (nodesDelay[i+1].second - nodesDelay[i].second)/maxPacketId;
                nodeToNodeDelay.insert_or_assign(std::make_pair(firstNode,secondNode),delayAcc );
            }
        }
    }
    
    std::cout << "Per node-to-node delays:" << std::endl;
    ns3::Time maxDelay;
    std::pair<uint32_t,uint32_t> maxDelayIndex;
    XMLElement* maxDelayValueElement;
    XMLElement* maxTimestampElement;
    XMLElement* maxNodePairElement;
    
    for(std::map<std::pair<uint32_t,uint32_t>,ns3::Time>::iterator nodeToNodeDelayIt = nodeToNodeDelay.begin();nodeToNodeDelayIt != nodeToNodeDelay.end() ; ++nodeToNodeDelayIt)
    {
        std::cout<< "("<< nodeToNodeDelayIt->first.first << "," << nodeToNodeDelayIt->first.second << ") -> " << nodeToNodeDelayIt->second << std::endl;
        // Check if the 'delay' element for this node pair already exists
        XMLElement* delayElement = delaysElement->FirstChildElement("delay");
        while (delayElement) {
            XMLElement* nodePairElement = delayElement->FirstChildElement("node-pair");
            if (nodePairElement) {
                uint32_t nodeId1 = std::stoul(nodePairElement->FirstChildElement("node-id")->GetText());
                uint32_t nodeId2 = std::stoul(nodePairElement->LastChildElement("node-id")->GetText());
                if (nodeId1 == nodeToNodeDelayIt->first.first && nodeId2 == nodeToNodeDelayIt->first.second) {
                    // The 'delay' element for this node pair already exists, append the new measurement
                    break;
                }
            }
            delayElement = delayElement->NextSiblingElement("delay");
        }

        if (!delayElement) {
            // The 'delay' element for this node pair doesn't exist, create a new one
            delayElement = measurments_doc.NewElement("delay");
            delaysElement->InsertEndChild(delayElement);

            // Create the 'node-pair' element
            XMLElement* nodePairElement = measurments_doc.NewElement("node-pair");
            delayElement->InsertEndChild(nodePairElement);

            // Add the node IDs
            XMLElement* nodeIdElement1 = measurments_doc.NewElement("node-id");
            nodeIdElement1->InsertEndChild(measurments_doc.NewText(std::to_string(nodeToNodeDelayIt->first.first).c_str()));
            nodePairElement->InsertEndChild(nodeIdElement1);

            XMLElement* nodeIdElement2 = measurments_doc.NewElement("node-id");
            nodeIdElement2->InsertEndChild(measurments_doc.NewText(std::to_string(nodeToNodeDelayIt->first.second).c_str()));
            nodePairElement->InsertEndChild(nodeIdElement2);
        }

        // Create the 'measurements' element
        XMLElement* measurementsElement = delayElement->FirstChildElement("measurements");
        if (!measurementsElement) {
            measurementsElement = measurments_doc.NewElement("measurements");
            delayElement->InsertEndChild(measurementsElement);
        }

        // Create the 'measurement' element
        XMLElement* measurementElement = measurments_doc.NewElement("measurement");
        measurementsElement->InsertEndChild(measurementElement);

        // Add the delay value and timestamp
        XMLElement* delayValueElement = measurments_doc.NewElement("delay-value");
        delayValueElement->InsertEndChild(measurments_doc.NewText(std::to_string(nodeToNodeDelayIt->second.GetNanoSeconds()).c_str()));
        measurementElement->InsertEndChild(delayValueElement);

        XMLElement* timestampElement = measurments_doc.NewElement("timestamp");
        timestampElement->InsertEndChild(measurments_doc.NewText(std::to_string(Simulator::Now().GetNanoSeconds()).c_str()));
        measurementElement->InsertEndChild(timestampElement);

        if (nodeToNodeDelayIt->second > maxDelay)
        {
            maxDelay = nodeToNodeDelayIt->second;
            maxDelayIndex = nodeToNodeDelayIt->first;
        }

    }

    XMLElement* worstLinksElement = root->FirstChildElement("worst-links");
    if (!worstLinksElement) {
        worstLinksElement = measurments_doc.NewElement("worst-links");
        root->InsertFirstChild(worstLinksElement);
    }
    
    XMLElement* worstLinkElement = measurments_doc.NewElement("worst-link");

    // Add the delay value 
    XMLElement* delayValueElement = measurments_doc.NewElement("delay-value");
    delayValueElement->InsertEndChild(measurments_doc.NewText(std::to_string(maxDelay.GetNanoSeconds()).c_str()));
    worstLinkElement->InsertEndChild(delayValueElement);
    // Add the timestamp
    XMLElement* timestampElement = measurments_doc.NewElement("timestamp");
    timestampElement->InsertEndChild(measurments_doc.NewText(std::to_string(Simulator::Now().GetNanoSeconds()).c_str()));
    worstLinkElement->InsertEndChild(timestampElement);

    // Create the 'node-pair' element
    XMLElement* nodePairElement = measurments_doc.NewElement("node-pair");

    // Add the node IDs
    XMLElement* nodeIdElement1 = measurments_doc.NewElement("node-id");
    nodeIdElement1->InsertEndChild(measurments_doc.NewText(std::to_string(maxDelayIndex.first).c_str()));
    nodePairElement->InsertEndChild(nodeIdElement1);

    XMLElement* nodeIdElement2 = measurments_doc.NewElement("node-id");
    nodeIdElement2->InsertEndChild(measurments_doc.NewText(std::to_string(maxDelayIndex.second).c_str()));
    nodePairElement->InsertEndChild(nodeIdElement2);
    worstLinkElement->InsertEndChild(nodePairElement);
     
    worstLinksElement->InsertEndChild(worstLinkElement);
    std::cout << Simulator::Now() <<"The bottle neck link is:" << "("<< maxDelayIndex.first << ", " << maxDelayIndex.second << ") " << "with a delay of " << maxDelay << std::endl;

    measurments_doc.SaveFile( "/home/leandro/Scripts/ns-3-dev/simple_global_routing_node_to_node_delays.xml" );
    monitor->ResetAllStats();
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
        XMLDocument measurments_doc; 
        measurments_doc.SaveFile( "/home/leandro/Scripts/ns-3-dev/simple_global_routing_node_to_node_delays.xml" );

        Simulator::Schedule(Seconds(10.1), &nodeToNodeTrigger, monitor);
        Simulator::Schedule(Seconds(20.1), &nodeToNodeTrigger, monitor);
        // nodeToNodeTrigger(monitor,measurments_doc);
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
