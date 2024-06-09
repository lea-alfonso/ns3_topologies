#include <ns3/core-module.h>
#include <ns3/flow-monitor-module.h>
#include <ns3/point-to-point-module.h>
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/big-brother-flow-probe.h"
#include "ns3/ipv4-flow-probe.h"
#include <tinyxml2.h>
using namespace tinyxml2;

using namespace ns3;

ns3::Time NETWORK_OK_MEASURING_TIME = MilliSeconds(100);
ns3::Time NETWORK_NOT_OK_MEASURING_TIME = MilliSeconds(100);
ns3::Time MEASURING_TIME = MilliSeconds(100);

struct TrackedStats {
    double throughput; // Threshold for throughput
    ns3::Time meanDelay; // Threshold for mean delay
    ns3::Time lastPacketDelay; // Threshold for last packet delay
    ns3::Time meanJitter; // Threshold for mean jitter
    double flowsAverageThroughput;
    Time flowsAverageDelay;
    Time delayValuesMedian;
    // Constructor to initialize the thresholds
    TrackedStats(float throughput, ns3::Time meanDelay, ns3::Time lastPacketDelay, ns3::Time meanJitter, double flowsAverageThroughput, Time flowsAverageDelay, Time delayValuesMedian)
        : throughput(throughput),
          meanDelay(meanDelay),
          lastPacketDelay(lastPacketDelay),
          meanJitter(meanJitter),
          flowsAverageThroughput(flowsAverageThroughput),
          flowsAverageDelay(flowsAverageDelay),
          delayValuesMedian(delayValuesMedian) {}
};

bool compareByTime(const std::pair<uint32_t, ns3::Time>& a, const std::pair<uint32_t, ns3::Time>& b) {
    return a.second.Compare(b.second) < 0;
}

bool IsFlowStatsEmpty(const FlowId flowId, const FlowMonitor::FlowStats& flowStat)
{
    // if (flowStat.delaySum != Seconds(0)) {
    //     std::cout << "Flow id: "<< flowId << "delaySum is not zero\n";
    // }
    // if (flowStat.jitterSum != Seconds(0)) {
    //     std::cout<< "Flow id: "<< flowId << "jitterSum is not zero\n";
    // }
    // if (flowStat.lastDelay != Seconds(0)) {
    //     std::cout<< "Flow id: "<< flowId << "lastDelay is not zero\n";
    // }
    // if (flowStat.txBytes != 0) {
    //     std::cout<< "Flow id: "<< flowId << "txBytes is not zero\n";
    // }
    // if (flowStat.rxBytes != 0) {
    //     std::cout<< "Flow id: "<< flowId << "rxBytes is not zero\n";
    // }
    // if (flowStat.txPackets != 0) {
    //     std::cout<< "Flow id: "<< flowId << "txPackets is not zero\n";
    // }
    // if (flowStat.rxPackets != 0) {
    //     std::cout<< "Flow id: "<< flowId << "rxPackets is not zero\n";
    // }
    // if (flowStat.lostPackets != 0) {
    //     std::cout<< "Flow id: "<< flowId << "lostPackets is not zero\n";
    // }
    // if (flowStat.timesForwarded != 0) {
    //     std::cout<< "Flow id: "<< flowId << "timesForwarded is not zero\n";
    // }
    // if (!flowStat.bytesDropped.empty()) {
    //     std::cout<< "Flow id: "<< flowId << "bytesDropped is not empty\n";
    // }
    // if (!flowStat.packetsDropped.empty()) {
    //     std::cout<< "Flow id: "<< flowId << "packetsDropped is not empty\n";
    // }

    return (flowStat.delaySum == Seconds(0) &&
        flowStat.jitterSum == Seconds(0) &&
        flowStat.lastDelay == Seconds(0) &&
        flowStat.txBytes == 0 &&
        flowStat.rxBytes == 0 &&
        flowStat.txPackets == 0 &&
        flowStat.rxPackets == 0 &&
        flowStat.timesForwarded == 0 &&
        flowStat.bytesDropped.empty() &&
        flowStat.packetsDropped.empty());
}

void ChangeLinkDelay(Ptr<NetDevice> device, Time newDelay) {
    std::cout << Simulator::Now().As(Time::MS) <<" Changing link delay!" << std::endl;
    Ptr<PointToPointChannel> p2pChannel = device->GetChannel()->GetObject<PointToPointChannel>();
    if (p2pChannel) {
        p2pChannel->SetAttribute("Delay", TimeValue(newDelay));
    } else {
        NS_LOG_ERROR("Failed to get PointToPointChannel object from the device.");
    }
}

std::optional<std::pair<int64_t,int64_t>> nodeToNodeTrigger(Ptr<FlowMonitor> monitor, std::string node_to_node_doc_path)
// This function implements the first step in looking for the cause of a bad metric
// Given a flow_id, we look for the worst performing FlowProbes
{
    XMLDocument measurements_doc; 
    XMLError result = measurements_doc.LoadFile(node_to_node_doc_path.c_str());
    if (result != XML_SUCCESS && result != XML_ERROR_EMPTY_DOCUMENT) {
        // Handle error
        std::cerr << "Error loading file: " << measurements_doc.ErrorIDToName(result) << std::endl;
        return std::nullopt;
    } 

    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    FlowMonitor::FlowProbeContainer probes = monitor->GetAllProbes();
    std::map<std::pair<uint32_t,uint32_t>,ns3::Time> nodeToNodeDelay;

    // Get the root element
    XMLElement* root = measurements_doc.FirstChildElement("network-measurements");
    if (!root) {
        // Create the root element if it doesn't exist
        root = measurements_doc.NewElement("network-measurements");
        measurements_doc.InsertEndChild(root);
    }

    XMLElement* delaysElement = root->FirstChildElement("delays");
    if (!delaysElement) {
        // Create the 'delays' element if it doesn't exist
        delaysElement = measurements_doc.NewElement("delays");
        root->InsertEndChild(delaysElement);
    }

    for (FlowMonitor::FlowStatsContainer::const_iterator flowStatsIt = stats.begin();
        flowStatsIt != stats.end();
        ++flowStatsIt)
    {
        FlowId flowId = flowStatsIt->first;  
        /* The variable perPacketStats is meant to be the structure that'll keep the information
        of the path each packet went through, as well as how much delay since first transmission each probe reported.
        Because of this, it's a map of packet Ids to a vector of pairs, where each pair stores a nodeId
        and a time value, representing the delay introduced measured so far by the time the packet reached 
        each node.  */
        std::map<uint32_t,std::vector<std::pair<uint32_t,ns3::Time>>> perPacketStats;

        /* 
        For each packet sent(not necessarely recived) in the flow.We'll
            1- Access each probe involved in the flow
            2- Check if the specific packet went through it
            3- If it did, we'll extract the delay from the bigBrotherStats.
            4- Create an adequate entry for perPacketStats
        */
        // Step 1
        for (ns3::Ptr<ns3::FlowProbe> probe : probes)
        {
            FlowProbe::Stats probeStats = probe->GetStats();
            Ptr<BigBrotherFlowProbe> bigBrotherProbe = DynamicCast<BigBrotherFlowProbe>(probe);
            // We only measure in the probes that are ipv4/bigBrother
            if (bigBrotherProbe)
            {
                std::map<FlowId,uint32_t>::const_iterator it = bigBrotherProbe->m_packetStartIndex.find(flowId);
                if (it != bigBrotherProbe->m_packetStartIndex.end()){
                    uint32_t startIndex = it->second;
                    for (uint32_t packetId = startIndex; packetId < startIndex + flowStatsIt->second.rxPackets; ++packetId)
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
                            // Observation: We're storing the delay from first probe of each node
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
        
        }

        // packetId -> vector of (node,delay)
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
                // This would override if another packet from whichever flow, had passed between those nodes before
                //The measure of delay to every node-to-node metric, is the avg for every package
                delayAcc = (nodesDelay[i+1].second - nodesDelay[i].second); 
                // delayAcc += (nodesDelay[i+1].second - nodesDelay[i].second)/maxPacketId;
                nodeToNodeDelay.insert_or_assign(std::make_pair(firstNode,secondNode),delayAcc );
            }
        }
    }
    
    ns3::Time maxDelay;
    std::pair<uint32_t,uint32_t> maxDelayIndex;
    
    for(std::map<std::pair<uint32_t,uint32_t>,ns3::Time>::iterator nodeToNodeDelayIt = nodeToNodeDelay.begin();nodeToNodeDelayIt != nodeToNodeDelay.end() ; ++nodeToNodeDelayIt)
    {
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
            delayElement = measurements_doc.NewElement("delay");
            delaysElement->InsertEndChild(delayElement);

            // Create the 'node-pair' element
            XMLElement* nodePairElement = measurements_doc.NewElement("node-pair");
            delayElement->InsertEndChild(nodePairElement);

            // Add the node IDs
            XMLElement* nodeIdElement1 = measurements_doc.NewElement("node-id");
            nodeIdElement1->InsertEndChild(measurements_doc.NewText(std::to_string(nodeToNodeDelayIt->first.first).c_str()));
            nodePairElement->InsertEndChild(nodeIdElement1);

            XMLElement* nodeIdElement2 = measurements_doc.NewElement("node-id");
            nodeIdElement2->InsertEndChild(measurements_doc.NewText(std::to_string(nodeToNodeDelayIt->first.second).c_str()));
            nodePairElement->InsertEndChild(nodeIdElement2);
        }

        // Create the 'measurements' element
        XMLElement* measurementsElement = delayElement->FirstChildElement("measurements");
        if (!measurementsElement) {
            measurementsElement = measurements_doc.NewElement("measurements");
            delayElement->InsertEndChild(measurementsElement);
        }

        // Create the 'measurement' element
        XMLElement* measurementElement = measurements_doc.NewElement("measurement");
        measurementsElement->InsertEndChild(measurementElement);

        // Add the delay value and timestamp
        XMLElement* delayValueElement = measurements_doc.NewElement("delay-value");
        delayValueElement->InsertEndChild(measurements_doc.NewText(std::to_string(nodeToNodeDelayIt->second.GetNanoSeconds()).c_str()));
        measurementElement->InsertEndChild(delayValueElement);

        XMLElement* timestampElement = measurements_doc.NewElement("timestamp");
        timestampElement->InsertEndChild(measurements_doc.NewText(std::to_string(Simulator::Now().GetNanoSeconds()).c_str()));
        measurementElement->InsertEndChild(timestampElement);

        if (nodeToNodeDelayIt->second > maxDelay)
        {
            maxDelay = nodeToNodeDelayIt->second;
            maxDelayIndex = nodeToNodeDelayIt->first;
        }

    }

    XMLElement* worstLinksElement = root->FirstChildElement("worst-links");
    if (!worstLinksElement) {
        worstLinksElement = measurements_doc.NewElement("worst-links");
        root->InsertFirstChild(worstLinksElement);
    }
    
    XMLElement* worstLinkElement = measurements_doc.NewElement("worst-link");

    // Add the delay value 
    XMLElement* delayValueElement = measurements_doc.NewElement("delay-value");
    delayValueElement->InsertEndChild(measurements_doc.NewText(std::to_string(maxDelay.GetNanoSeconds()).c_str()));
    worstLinkElement->InsertEndChild(delayValueElement);
    // Add the timestamp
    XMLElement* timestampElement = measurements_doc.NewElement("timestamp");
    timestampElement->InsertEndChild(measurements_doc.NewText(std::to_string(Simulator::Now().GetNanoSeconds()).c_str()));
    worstLinkElement->InsertEndChild(timestampElement);

    // Create the 'node-pair' element
    XMLElement* nodePairElement = measurements_doc.NewElement("node-pair");

    // Add the node IDs
    XMLElement* nodeIdElement1 = measurements_doc.NewElement("node-id");
    nodeIdElement1->InsertEndChild(measurements_doc.NewText(std::to_string(maxDelayIndex.first).c_str()));
    nodePairElement->InsertEndChild(nodeIdElement1);

    XMLElement* nodeIdElement2 = measurements_doc.NewElement("node-id");
    nodeIdElement2->InsertEndChild(measurements_doc.NewText(std::to_string(maxDelayIndex.second).c_str()));
    nodePairElement->InsertEndChild(nodeIdElement2);
    worstLinkElement->InsertEndChild(nodePairElement);
     
    worstLinksElement->InsertEndChild(worstLinkElement);

    measurements_doc.SaveFile(node_to_node_doc_path.c_str());

    return std::make_pair(maxDelayIndex.first,maxDelayIndex.second);
}
void nodeToNodeTriggerVoidWrapper(Ptr<FlowMonitor> monitor, std::string node_to_node_doc_path) {
    nodeToNodeTrigger(monitor,node_to_node_doc_path);
}

void reportFlowStats(Ptr<FlowMonitor> monitor,Ptr<Ipv4FlowClassifier> classifier,std::string filename, bool newFiles){

    // File managment for our logs
    XMLDocument measurements_doc; 
    std::ios_base::openmode open_file_flags;
    std::string flow_performance_measurements_path = filename +"_flow_performance_measurements.dat";
    std::string node_to_node_doc_path = filename + "_node_to_node_delays.xml";

    if (newFiles) {
        // We clean any previous measures_doc that could be present
        measurements_doc.SaveFile( node_to_node_doc_path.c_str() );
        open_file_flags = std::ofstream::out | std::ofstream::trunc;

    } else {
        // We open a file to save the flow performance metrics
        open_file_flags = std::ofstream::out | std::ofstream::app ;
    }

    std::ofstream outFile;
    outFile.open(flow_performance_measurements_path.c_str(),open_file_flags);
    if (!outFile.is_open())
    {
        std::cerr << "Can't open file " << flow_performance_measurements_path << std::endl;
    }
    outFile.setf(std::ios_base::fixed);

    monitor->CheckForLostPackets(MilliSeconds(300));

    TrackedStats measurements(0, Seconds(0), Seconds(0), Seconds(0), 0, Seconds(0),Seconds(0));
    // TrackedStats(float throughput, ns3::Time meanDelay, ns3::Time lastPacketDelay, ns3::Time meanJitter, double flowsAverageThroughput, Time flowsAverageDelay, Time delayValuesMedian)
    TrackedStats thresholds( 0.06, Seconds(1.0044), Seconds(0), Seconds(0), 0.055, Seconds(0.00419),Seconds(0));
    double averageFlowThroughput = 0.0;
    double averageFlowDelay = 0.0;
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    std::vector<double> delayValues(stats.size());
    uint64_t cont = 0;

    // Redefining equal to topology1_3 because we're prototyping
    Time simTime = MilliSeconds(1400);
    Time appStartTime = MilliSeconds(400);
    
    outFile << "Report flow stats " << Simulator::Now().As(Time::MS) << ", Current measuring time " << MEASURING_TIME << std::endl;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin();
         i != stats.end();
         ++i)
    {
        if (!IsFlowStatsEmpty(i->first,i->second)) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::stringstream protoStream;
        protoStream << (uint16_t)t.protocol;
        if (t.protocol == 6)
        {
            protoStream.str("TCP");
        }
        if (t.protocol == 17)
        {
            protoStream.str("UDP");
        }
        outFile << "\tFlow " << i->first << " (" << t.sourceAddress << ":" << t.sourcePort << " -> "
                << t.destinationAddress << ":" << t.destinationPort << ") proto " << protoStream.str() << "\n";
        outFile << "\t\tTx Packets: " << i->second.txPackets << "\n";
        outFile << "\t\tTx Bytes:   " << i->second.txBytes << "\n";
        outFile << "\t\tTxOffered:  " << i->second.txBytes * 8.0 / (simTime - appStartTime).GetSeconds() / 1000.0 / 1000.0 << " Mbps\n";
        outFile << "\t\tRx Packets: " << i->second.rxPackets << "\n";
        outFile << "\t\tRx Bytes:   " << i->second.rxBytes << "\n";

        // We initialize througput, meanDelay and meanJitter with -
        measurements.throughput = 0;
        measurements.meanDelay = Seconds(0);
        measurements.meanJitter = Seconds(0);
        
        measurements.lastPacketDelay = i->second.lastDelay;
        if (i->second.rxPackets > 0)
        {
                // Measure the duration of the flow from receiver's perspective
                double rxDuration = (simTime - appStartTime).GetSeconds();

                averageFlowThroughput += i->second.rxBytes * 8.0 / rxDuration / 1000 / 1000;
                averageFlowDelay += i->second.delaySum.GetSeconds() / i->second.rxPackets;
                delayValues[cont] = i->second.delaySum.GetSeconds() / i->second.rxPackets;
                cont++;
                // In Mbps
                measurements.throughput = i->second.rxBytes * 8.0 / rxDuration / 1000 / 1000;
                measurements.meanDelay = Seconds( i->second.delaySum.GetSeconds() / i->second.rxPackets);
                measurements.meanJitter = Seconds(i->second.jitterSum.GetSeconds() / i->second.rxPackets);
            } 
            
            outFile << "\t\tThroughput: " << measurements.throughput << " Mbps\n";
            outFile << "\t\tMean delay: "<< measurements.meanDelay.As(Time::MS) << " \n";
            outFile << "\t\tLast packet delay: " << measurements.lastPacketDelay.As(Time::MS) << " \n";
            outFile << "\t\tMean jitter: " << measurements.meanJitter.As(Time::MS) << "\n";

            if(
                measurements.throughput < thresholds.throughput || 
                measurements.meanDelay > thresholds.meanDelay
            ){
                outFile << "\n\t Flow "<< i->first <<" surpassing threshold " << "\n";
            } else{
                outFile << "\n\t Flow "<< i->first <<" under threshold " << "\n";
            }
        }
    }
    std::sort(delayValues.begin(), delayValues.end());
    double delayValuesMedian = delayValues[stats.size() / 2];

    // These metrics are global to all the flows
    measurements.flowsAverageThroughput = averageFlowThroughput / stats.size();
    measurements.flowsAverageDelay = Seconds(averageFlowDelay / stats.size());
    measurements.delayValuesMedian = Seconds(delayValuesMedian);

    outFile << "\n\n\tMean flow throughput: " << measurements.flowsAverageThroughput << " Mbps\n";
    outFile << "\tMean flow delay: " << measurements.flowsAverageDelay.As(Time::MS) << "\n";
    outFile << "\tMedian flow delay: " << measurements.delayValuesMedian.As(Time::MS) << "\n";
    
    if(
        measurements.flowsAverageThroughput < thresholds.flowsAverageThroughput || 
        measurements.flowsAverageDelay > thresholds.flowsAverageDelay
    ){
        MEASURING_TIME = NETWORK_NOT_OK_MEASURING_TIME; 
        outFile << "\n\tavg. threshold surpassed " << "\n";
        std::optional<std::pair<int64_t,int64_t>> worstPerformingLink = nodeToNodeTrigger(monitor,node_to_node_doc_path);
        if (worstPerformingLink.has_value()) {
            outFile << "\tWorst performing link: ("<< worstPerformingLink.value().first << "," << worstPerformingLink.value().second << ")" << std::endl;
            
        }
    } 
    else{

        MEASURING_TIME = NETWORK_OK_MEASURING_TIME;
        outFile << "\n\tavg. under threshold " << "\n";
        std::optional<std::pair<int64_t,int64_t>> worstPerformingLink = nodeToNodeTrigger(monitor,node_to_node_doc_path);
    }
    outFile.close();

    // We reset all stats to ensure that we're not reusing them for the next iteration
    monitor->ResetAllStats();
    std::cout << "finished with reportFlowStats" << std::endl;
    // Simulator::Schedule(simTime , &reportFlowStats, monitor, classifier, filename, false);
}