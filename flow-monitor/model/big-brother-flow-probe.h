// big-brother-flow-probe.h
#ifndef BIG_BROTHER_FLOW_PROBE_H
#define BIG_BROTHER_FLOW_PROBE_H

#include "flow-probe.h"
#include "ipv4-flow-probe.h"
#include "ipv4-flow-classifier.h"

#include "ns3/ipv4-l3-protocol.h"
#include "ns3/queue-item.h"

#include <map>

namespace ns3
{

class FlowMonitor;
class Node;

/// \ingroup flow-monitor
/// \brief Class that tracks node-to-node metrics at the IPv4 layer of a Node
class BigBrotherFlowProbe : public Ipv4FlowProbe
{
public:
    uint32_t m_nodeId; // Node ID of the node being monitored
    // Container to map <FlowId, PacketId> -> map <NodeId, Stats>
    typedef std::map<std::pair<FlowId, uint32_t>, FlowProbe::FlowStats> PerPacketStats;
    PerPacketStats m_perPacketStats;

    BigBrotherFlowProbe(Ptr<FlowMonitor> monitor, Ptr<Ipv4FlowClassifier> classifier, Ptr<Node> node);
    ~BigBrotherFlowProbe() override;

    // Add packet stats with additional node information
    void AddPacketStats(FlowId flowId, uint32_t packetId, uint32_t packetSize, Time delayFromFirstProbe);

    /// Register this type.
    /// \return The TypeId.
    static TypeId GetTypeId();



};

} // namespace ns3

#endif /* BIG_BROTHER_FLOW_PROBE_H */