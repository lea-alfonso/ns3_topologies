#include "big-brother-flow-probe.h"
#include "flow-monitor.h"
#include "flow-probe.h"
#include "ipv4-flow-probe.h"
#include "ipv4-flow-classifier.h"

#include "ns3/ipv4-l3-protocol.h"
#include "ns3/queue-item.h"

#include <map>
#include "ns3/config.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"

namespace ns3
{

BigBrotherFlowProbe::BigBrotherFlowProbe(Ptr<FlowMonitor> monitor, Ptr<Ipv4FlowClassifier> classifier, Ptr<Node> node)
    : Ipv4FlowProbe(monitor, classifier, node),
      m_nodeId(node->GetId()),
      m_perPacketStats{},
      m_packetStartIndex{}
{
}

BigBrotherFlowProbe::~BigBrotherFlowProbe()
{
}

void BigBrotherFlowProbe::AddPacketStats(FlowId flowId, uint32_t packetId, uint32_t packetSize, Time delayFromFirstProbe)
{
    // Update the overall flow stats
    Ipv4FlowProbe::AddPacketStats(flowId, packetSize, delayFromFirstProbe);

    //If the flow is new, we initialize packetStartIndex
    m_packetStartIndex.emplace(std::make_pair(flowId,0));

    // Update bigBrothers stats
    std::pair<std::map<std::pair<uint32_t,uint32_t>, FlowProbe::FlowStats>::iterator,bool> insert= this->m_perPacketStats.emplace(std::make_pair(flowId, packetId),  FlowProbe::FlowStats());
    if (insert.second) {
        std::map<std::pair<uint32_t,uint32_t>, FlowProbe::FlowStats>::iterator perPacketStatsIt = insert.first;
        perPacketStatsIt->second.bytes += packetSize;
        perPacketStatsIt->second.packets++;
        perPacketStatsIt->second.delayFromFirstProbeSum += delayFromFirstProbe;
    } else {
       std::cerr<< "Error inserting (flow_id,packet_id)->FlowProbe::FlowStat into big-brother-probe, key already existed" << std::endl; 
    }
}

/* static */
TypeId
BigBrotherFlowProbe::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::BigBrotherFlowProbe").SetParent<Ipv4FlowProbe>().SetGroupName("FlowMonitor")
        // No AddConstructor because this class has no default constructor.
        ;
    return tid;
}

} // namespace ns3