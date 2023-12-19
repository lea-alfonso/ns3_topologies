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
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

// Add the following headers
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"

// Default Network Topology
//
//       10.1.1.0
// n0 -------------- n1   n2   n3   n4
//    point-to-point  |    |    |    |
//                    ================
//                      LAN 10.1.2.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SecondScriptExample");

Time lastPacketReceiptTime = Seconds(0);

static void test(std::string context, Ptr<const Packet> packet, Ptr<NetDevice> sender, Ptr<NetDevice> receiver, Time start, Time end) {
    std::cout << "(" << sender->GetAddress() << " -> " << receiver->GetAddress() << ") start: " << start << " end: " << end << " lastPacketReceiptTime: " << lastPacketReceiptTime << "\n" ;

    Ipv4Address senderAddress;
    Ipv4Address receiverAddress;

    Ptr<Ipv4> ipv4src = sender->GetNode()->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4dest = receiver->GetNode()->GetObject<Ipv4>();

    for (uint32_t i = 0; i < ipv4src->GetNInterfaces(); i++) {
        if (ipv4src->GetAddress(i, 0).GetLocal() == sender->GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal()) {
            senderAddress = ipv4src->GetAddress(i, 0).GetLocal();
            break;
        }
    }

    for (uint32_t i = 0; i < ipv4dest->GetNInterfaces(); i++) {
        if (ipv4dest->GetAddress(i, 0).GetLocal() == receiver->GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal()) {
            receiverAddress = ipv4dest->GetAddress(i, 0).GetLocal();
            break;
        }
    }


    std::cout << "(" << senderAddress << " -> " << receiverAddress << ") start: " << start << " end: " << end << " lastPacketReceiptTime: " << lastPacketReceiptTime << "\n";


    // Calculate throughput (in Mbps)
    double throughput = (packet->GetSize() * 8.0) / (end.GetSeconds() - start.GetSeconds()) / 1e6;

    // Calculate delay (in milliseconds)
    double delay = (end - start).GetMilliSeconds();

    // Calculate jitter (difference in packet receipt time)
    double jitter = (end - lastPacketReceiptTime).GetMilliSeconds();

    // Update lastPacketReceiptTime for the next packet
    lastPacketReceiptTime = end;

    // Print or store the calculated metrics
    std::cout << "Throughput: " << throughput << " Mbps, Delay: " << delay << " ms, Jitter: " << jitter << " ms\n";
}


int main(int argc, char* argv[]) {

    bool verbose = true;
    uint32_t nCsma = 3;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);

    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    nCsma = nCsma == 0 ? 1 : nCsma;

    NodeContainer p2pNodes;
    p2pNodes.Create(2);

    NodeContainer csmaNodes;
    csmaNodes.Add(p2pNodes.Get(1));
    csmaNodes.Create(nCsma);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install(p2pNodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);
    std::ostringstream oss;
    oss << "/ChannelList/"
        << p2pDevices.Get(0)->GetChannel()
        << "/$ns3::PointToPointChannel/TxRxPointToPoint";

    Config::Connect(oss.str(), MakeCallback(&test));

    InternetStackHelper stack;
    stack.Install(p2pNodes.Get(0));
    stack.Install(csmaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign(p2pDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces;
    csmaInterfaces = address.Assign(csmaDevices);

    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps = echoServer.Install(csmaNodes.Get(nCsma));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(csmaInterfaces.GetAddress(nCsma), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(p2pNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    pointToPoint.EnablePcapAll("second");
    csma.EnablePcap("second", csmaDevices.Get(1), true);


    Simulator::Run();


    Simulator::Destroy();
    return 0;
}
