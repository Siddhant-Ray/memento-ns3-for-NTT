/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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

// Original author: Alexander Dietmüller
// Modified by: Siddhant Ray
// Modified flow creations and interactions to generate traffic data for training the NTT model


#include <iostream>
#include <fstream>
#include <unordered_map>

#include <chrono>
#include <ctime>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/gnuplot.h"
#include "ns3/node.h"
#include "ns3/traffic-control-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/ipv4-header.h"
#include "ns3/random-variable-stream.h"
#include "ns3/rng-seed-manager.h"


#include "ns3/cdf-application.h"
#include "ns3/experiment-tags.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TrafficGenerationExperiment");

const auto TCP = TypeIdValue(TcpSocketFactory::GetTypeId());
const auto UDP = TypeIdValue(UdpSocketFactory::GetTypeId());

// Put the current timestamp and packet size into a log.
void logSize(Ptr<OutputStreamWrapper> stream, Ptr<Packet const> p)
{
    auto current_time = Simulator::Now();
    *stream->GetStream() << current_time.GetSeconds() << ','
                         << p->GetSize() << std::endl;
}

// Tag a packet with a timestamp.
void setTimeTag(Ptr<Packet const> p)
{
    TimestampTag tag;
    tag.SetTime(Simulator::Now());
    p->AddPacketTag(tag);
};

void setIdTag(u_int32_t workload_id, u_int32_t app_id, Ptr<Packet const> p)
{
    IdTag tag;
    tag.SetWorkload(workload_id);
    tag.SetApplication(app_id);
    p->AddPacketTag(tag);
};

void logSenderPacket(Ptr<OutputStreamWrapper> stream, Ptr<Packet const> p)
{
    TimestampTag timestampTag;

    if (p->PeekPacketTag(timestampTag)){

        Ptr<Packet> copy = p->Copy();
        EthernetHeader eHeader;
        copy->RemoveHeader(eHeader);
        Ipv4Header ipHeader;
        copy->RemoveHeader(ipHeader);
        TcpHeader tcpHeader;
        copy->RemoveHeader(tcpHeader);

        auto sender_time = timestampTag.GetTime();
        *stream->GetStream() << "Tx sent at:, "<< sender_time.GetSeconds()<< ", ";
        *stream->GetStream() << "Packet size is, "<< p->GetSize() << ", ";
        *stream->GetStream() << "Packet uid is, "<< p->GetUid() << ", ";
        // Add TCP sequence number to track the packet
        *stream->GetStream() << "TCP sequence num is, "<< tcpHeader.GetSequenceNumber() << ", ";
        *stream->GetStream() << "\n";
    }
    else
    {
        NS_LOG_DEBUG("Packet without timestamp, won't log.");
    };

}

// Log workload tag, timestamp tag, and packet size.
void logPacketInfo(Ptr<OutputStreamWrapper> stream, Ptr<Packet const> p)
{
    TimestampTag timestampTag;
    IdTag idTag;
    FlowIdTag flowid;
    MessageTag mTag;
    p->PeekPacketTag(mTag);
    
    if (p->PeekPacketTag(timestampTag) && p->PeekPacketTag(idTag))
    {
        auto current_time = Simulator::Now();
        auto diff_time = current_time - timestampTag.GetTime();
        *stream->GetStream() << "Tx sent at:, "<< current_time.GetSeconds()<< ", ";
        *stream->GetStream() << "Flow id is, "<< p->PeekPacketTag(flowid) << ", "
                             << "Packet uid is, "<< p->GetUid() << ", "
                             << "Packet size is, "<< p->GetSize() << ", ";
                             
        Ptr<Packet> copy = p->Copy();
        // Headers must be removed in the order they're present.
        EthernetHeader eHeader;
        copy->RemoveHeader(eHeader);
        Ipv4Header ipHeader;
        copy->RemoveHeader(ipHeader);
        *stream->GetStream() << "IP ID is, "<< ipHeader.GetIdentification() << ", "
                                << "DSCP is, "<< ipHeader.GetDscp() << ", "
                                << "ECN is, "<< ipHeader.GetEcn() << ", "
                                << "TTL is, "<< (uint16_t) ipHeader.GetTtl() << ", "
                                << "Payload size is, "<< ipHeader.GetPayloadSize() << ", "
                                << "Protocol is, "<< (uint16_t) ipHeader.GetProtocol() << ", "
                                << "Source IP is, "<< ipHeader.GetSource() << ", "
                                << "Destination IP is, "<< ipHeader.GetDestination() << ", ";           
        TcpHeader tcpHeader;
        UdpHeader udpHeader;

        // Check if it's a TCP or UDP packet
        if ((uint16_t) ipHeader.GetProtocol() == 6)
        {   
            copy->RemoveHeader(tcpHeader);
            *stream->GetStream() << "TCP source port is, "<< tcpHeader.GetSourcePort() << ", "
                                 << "TCP destination port is, "<< tcpHeader.GetDestinationPort() << ", "
                                 << "TCP sequence num is, "<< tcpHeader.GetSequenceNumber() << ", "
                                 << "TCP current window size is, "<< tcpHeader.GetWindowSize() << ", ";
        }
        else if ((uint16_t) ipHeader.GetProtocol() == 17)
        {   
            copy->RemoveHeader(udpHeader);
            *stream->GetStream() << "UDP source port is, "<< udpHeader.GetSourcePort() << ", "
                                 << "UDP destination port is, "<< udpHeader.GetDestinationPort() << ", ";
        }
        else
        {
            *stream->GetStream() << "Unknown transport protocol, ";
        }

        // copy->RemoveHeader(tcpHeader);
        *stream->GetStream() 
                            //  << "TCP source port is, "<< tcpHeader.GetSourcePort() << ", "
                            //  << "TCP destination port is, "<< tcpHeader.GetDestinationPort() << ", "
                            //  << "TCP sequence num is, "<< tcpHeader.GetSequenceNumber() << ", "
                            //  << "TCP current window size is, "<< tcpHeader.GetWindowSize() << ", "
                             << "Delay is, "<< diff_time.GetSeconds() << ", "
                             << "Workload id is, "<< idTag.GetWorkload() << ','
                             << "Application id is, "<< idTag.GetApplication() << ','
                             << "Message id is, "<< mTag.GetSimpleValue() << ',';


        copy->Print(*stream->GetStream());
        *stream->GetStream() << "\n";

    }
    else
    {
        //NS_LOG_DEBUG("Packet without timestamp, won't log.");
    };
};

// Log TCP ack time, sequence number, and window size 
// received at the sender
void logAckInfo(Ptr<OutputStreamWrapper> stream, Ptr<Packet const> p)
{
    TimestampTag timestampTag;
    IdTag idTag;
    FlowIdTag flowid;
    MessageTag mTag;
    p->PeekPacketTag(mTag);

    Ptr<Packet> copy = p->Copy();
    EthernetHeader eHeader;
    copy->RemoveHeader(eHeader);
    Ipv4Header ipHeader;
    copy->RemoveHeader(ipHeader);
    TcpHeader tcpHeader;
    copy->RemoveHeader(tcpHeader);

    auto current_time = Simulator::Now();
    *stream->GetStream() << "Ack received at "<< current_time.GetSeconds()<< ", ";
    *stream->GetStream() << "Packet size is, "<< p->GetSize() << ", ";
    *stream->GetStream() << "Packet uid is, "<< p->GetUid() << ", ";
    // Add TCP sequence number to track the packet
    *stream->GetStream() << "TCP sequence num is, "<< tcpHeader.GetSequenceNumber() << ", ";
    // Add TCP ack number to track the packet
    *stream->GetStream() << "TCP ack num is, "<< tcpHeader.GetAckNumber() << ", ";

    p->Print(*stream->GetStream());
    *stream->GetStream() << "\n";
};

void logValue(Ptr<OutputStreamWrapper> stream, std::string context,
              uint32_t oldval, uint32_t newval)
{
    auto current_time = Simulator::Now();
    *stream->GetStream() << context << ',' << current_time.GetSeconds() << ','
                         << newval << std::endl;
}

void logDrop(Ptr<OutputStreamWrapper> stream,
             std::string context, Ptr<Packet const> p)
{
    auto current_time = Simulator::Now();

    Ptr<Packet> copy = p->Copy();
    EthernetHeader eHeader;
    copy->RemoveHeader(eHeader);
    Ipv4Header ipHeader;
    copy->RemoveHeader(ipHeader);
    TcpHeader tcpHeader;
    copy->RemoveHeader(tcpHeader);

    *stream->GetStream() << context << ',' << current_time.GetSeconds() << ','
                         << p->GetSize() << "," << tcpHeader.GetSequenceNumber() << std::endl;
};

// TODO: Add base stream? Or how to get different random streams?
Ptr<RandomVariableStream> TimeStream(double min = 0.0, double max = 1.0)
{
    return CreateObjectWithAttributes<UniformRandomVariable>(
        "Min", DoubleValue(min),
        "Max", DoubleValue(max));
}

Ptr<RandomVariableStream> TimeStreamInt(double min = 1.0, double max = 1.0)
{
    return CreateObjectWithAttributes<UniformRandomVariable>(
        "Min", DoubleValue(min),
        "Max", DoubleValue(max));
}

PointerValue TimeStreamValue(double min = 0.0, double max = 1.0)
{
    return PointerValue(TimeStream(min, max));
}

NetDeviceContainer GetNetDevices(Ptr<Node> node)
{
    NetDeviceContainer devices;
    for (uint32_t i = 0; i < node->GetNDevices(); ++i)
    {
        devices.Add(node->GetDevice(i));
    }
    return devices;
}

int main(int argc, char *argv[])
{   
    // Measure wall clock time 
    auto start = std::chrono::system_clock::now();
	std::time_t start_time = std::chrono::system_clock::to_time_t(start);
	
	std::cout << "Started computation at " << std::ctime(&start_time);
    //
    // Users may find it convenient to turn on explicit debugging
    // for selected modules; the below lines suggest how to do this
    //
    #if 1
    LogComponentEnable("TrafficGenerationExperiment", LOG_LEVEL_INFO);
    #endif
    //
    // Allow the user to override any of the defaults and the above Bind() at
    // run-time, via command-line arguments
    //

    int n_apps = 10;
    DataRate linkrate("5Mbps");
    DataRate baserate("100kbps");
    int start_window = 1;
    Time delay("15ms"); // Make this comparable to the queue delay, which goes up to 0.5 seconds.
    QueueSize queuesize("100p");
    QueueSize queuesize_small("100p");
    auto seed = 1;

    // Different for different receivers (can choose to turn off for some, not for all)
    DataRate congestion1("0Mbps");
    DataRate congestion2("0Mbps");
    DataRate congestion3("0Mbps");

    std::string basedir = "./distributions/";
    std::string w1 = basedir + "SingleTestDistribution.txt";
    std::string w2 = basedir + "SingleTestDistribution.txt";
    std::string w3 = basedir + "SingleTestDistribution.txt";
    std::string prefix = "shift";
    double c_w1 = 1;
    double c_w2 = 1;
    double c_w3 = 1;

    auto choose_topo = 1;
    std::string cc_alg = "";

    CommandLine cmd;
    cmd.AddValue("topo", "Choose the topology", choose_topo);
    cmd.AddValue("apps", "Number of traffic apps per workload.", n_apps);
    cmd.AddValue("apprate", "Base traffic rate for each app.", baserate);
    cmd.AddValue("startwindow", "Maximum diff in start time.", start_window);
    cmd.AddValue("linkrate", "Link capacity rate.", linkrate);
    cmd.AddValue("linkdelay", "Link delay.", delay);
    cmd.AddValue("queuesize", "Bottleneck queue size.", queuesize);
    cmd.AddValue("w1", "Factor for W1 traffic (FB webserver).", c_w1);
    cmd.AddValue("w2", "Factor for W2 traffic (DCTCP messages).", c_w2);
    cmd.AddValue("w3", "Factor for W3 traffic (FB hadoop).", c_w3);
    cmd.AddValue("congestion1", "Congestion traffic rate.", congestion1);
    cmd.AddValue("congestion2", "Congestion traffic rate.", congestion2);
    cmd.AddValue("congestion3", "Congestion traffic rate.", congestion3);
    cmd.AddValue("prefix", "Prefix for log files.", prefix);
    cmd.AddValue("seed", "Set simulation seed", seed);
    cmd.AddValue("cc", "TCP-CC algorithm.", cc_alg);
    cmd.Parse(argc, argv);

    // Compute resulting workload datarates.
    auto rate_w1 = DataRate(static_cast<uint64_t>(c_w1 * baserate.GetBitRate()));
    auto rate_w2 = DataRate(static_cast<uint64_t>(c_w2 * baserate.GetBitRate()));
    auto rate_w3 = DataRate(static_cast<uint64_t>(c_w3 * baserate.GetBitRate()));

    // Print Overview of seetings
    NS_LOG_DEBUG("Overview:"
                 << std::endl
                 << "Topology is:  "
                 << choose_topo << std::endl
                 << "Congestion for receiver 1: "
                 << congestion1 << std::endl
                 << "Congestion for receiver 2: "
                 << congestion2 << std::endl
                 << "Congestion for receiver 3: "
                 << congestion3 << std::endl
                 << "Apps: "
                 << n_apps << std::endl
                 << "Workloads (data rates per app):"
                 << std::endl
                 << "W1: " << w1 << " (" << rate_w1 << ")"
                 << std::endl
                 << "W2: " << w2 << " (" << rate_w2 << ")"
                 << std::endl
                 << "W3: " << w3 << " (" << rate_w3 << ")");

    // Simulation variables
    auto simStart = TimeValue(Seconds(0));
    auto stopTime = Seconds(30);
    auto simStop = TimeValue(stopTime);
    RngSeedManager::SetSeed(seed);
    std::cout<< seed<< std::endl;

    // Fix MTU and Segment size, otherwise the small TCP default (536) is used.
    Config::SetDefault("ns3::CsmaNetDevice::Mtu", UintegerValue(1500));

    // Tcp Socket (general socket conf)
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4000000));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(4000000));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1380));

    
    //
    // Explicitly create the nodes required by the topology (shown above).
    //

    // Network topology 1
    //
    //                                  disturbance1
    //                                       |
    // 3x n_apps(senders) --- switchA --- switchB --- receiver1
    //                          |
    //                          |
    //                          |       disturbance2
    //                          |            |
    //                       switchC --- switchD --- receiver2
    //                          |       
    //                          |                 
    //                          |                   disturbance3
    //                          |                       |
    //                       switchE --- switchF --- switchG--recevier3
    //
    //

    // Network topology 2
    //
    //                               disturbance1
    //                                  |
    //        sender(s) --- switchA --- switchB --- receiver1
    //                      |
    //                      |
    //                      |        disturbance2
    //                      |           |
    //                   switchC --- switchD --- receiver2
    //                                  |       
    //                                  |                 
    //                                  |                   disturbance3
    //                                  |                       |
    //                               switchE --- switchF --- switchG--recevier3
    //
    
    //  The "disturbance" host is used to introduce changes in the network
    //  conditions. Disturbance nodes are explicit and can be turned off.
    //  If no disturbance specified, only congestion is from actual traffic on the shared links.

    // Hosts track only receivers and disturbances
    auto num_hosts = 6;

    NS_LOG_INFO("Create nodes (hosts and disturbances).");
    NodeContainer hosts;
    hosts.Create(num_hosts);
    // Keep references to sender, receiver, and disturbance
    // auto sender = hosts.Get(0);

    auto receiver1 = hosts.Get(0);
    auto disturbance1 = hosts.Get(1);
    auto receiver2 = hosts.Get(2);
    auto disturbance2 = hosts.Get(3);
    auto receiver3 = hosts.Get(4);
    auto disturbance3 = hosts.Get(5);
    
    NodeContainer switches;
    switches.Create(7);
    auto switchA = switches.Get(0);
    auto switchB = switches.Get(1);
    auto switchC = switches.Get(2);
    auto switchD = switches.Get(3);
    auto switchE = switches.Get(4);
    auto switchF = switches.Get(5);
    auto switchG = switches.Get(6);

    NodeContainer senders;
    senders.Create(3 * n_apps);

    NodeContainer ssenders;
    ssenders.Create(3 * n_apps);

    NodeContainer tsenders;
    tsenders.Create(3 * n_apps);

    NS_LOG_INFO("Build Topology");
    CsmaHelper csma;
    csma.SetChannelAttribute("FullDuplex", BooleanValue(true));
    csma.SetChannelAttribute("DataRate", DataRateValue(linkrate));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(15)));

    // Create the csma links
    // csma.Install(NodeContainer(sender, switchA));

    csma.Install(NodeContainer(receiver1, switchB));
    csma.Install(NodeContainer(disturbance1, switchB));
    csma.Install(NodeContainer(switchA, switchB));

    csma.Install(NodeContainer(switchA, switchC));
    csma.Install(NodeContainer(switchC, switchD));
    csma.Install(NodeContainer(receiver2, switchD));
    csma.Install(NodeContainer(disturbance2, switchD));

    if (choose_topo == 1)
    csma.Install(NodeContainer(switchC, switchE));
    else if (choose_topo == 2)
    csma.Install(NodeContainer(switchD, switchE));
    
    csma.Install(NodeContainer(switchE, switchF));
    csma.Install(NodeContainer(switchF, switchG));
    csma.Install(NodeContainer(receiver3, switchG));
    csma.Install(NodeContainer(disturbance3, switchG));

    for (auto it = senders.Begin(); it != senders.End(); it++)
    {
        Ptr<Node> sender = *it;
        csma.Install(NodeContainer(sender, switchA));
    }

    for (auto it = ssenders.Begin(); it != ssenders.End(); it++)
    {
        Ptr<Node> sender = *it;
        csma.Install(NodeContainer(sender, switchC));
    }

    for (auto it = tsenders.Begin(); it != tsenders.End(); it++)
    {
        Ptr<Node> sender = *it;
        csma.Install(NodeContainer(sender, switchE));
    }

    // Update the queue size
    Config::Set(
        "/NodeList/6/DeviceList/0/$ns3::CsmaNetDevice/TxQueue/MaxSize",
        QueueSizeValue(queuesize));
    Config::Set(
        "/NodeList/8/DeviceList/0/$ns3::CsmaNetDevice/TxQueue/MaxSize",
        QueueSizeValue(queuesize));
    Config::Set(
        "/NodeList/10/DeviceList/0/$ns3::CsmaNetDevice/TxQueue/MaxSize",
        QueueSizeValue(queuesize));
    Config::Set(
        "/NodeList/12/DeviceList/0/$ns3::CsmaNetDevice/TxQueue/MaxSize",
        QueueSizeValue(queuesize));

    // Create the bridge netdevice, turning the nodes into actual switches
    BridgeHelper bridge;
    bridge.Install(switchA, GetNetDevices(switchA));
    bridge.Install(switchB, GetNetDevices(switchB));

    bridge.Install(switchC, GetNetDevices(switchC));
    bridge.Install(switchD, GetNetDevices(switchD));


    bridge.Install(switchE, GetNetDevices(switchE));
    bridge.Install(switchF, GetNetDevices(switchF));
    bridge.Install(switchG, GetNetDevices(switchG));

    
    // Add internet stack and IP addresses to the hosts
    NS_LOG_INFO("Setup stack and assign IP Addresses.");
    NetDeviceContainer hostDevices;
    // hostDevices.Add(GetNetDevices(sender));
    hostDevices.Add(GetNetDevices(receiver1));
    hostDevices.Add(GetNetDevices(disturbance1));
    hostDevices.Add(GetNetDevices(receiver2));
    hostDevices.Add(GetNetDevices(disturbance2));
    hostDevices.Add(GetNetDevices(receiver3));
    hostDevices.Add(GetNetDevices(disturbance3));

    for (auto it = senders.Begin(); it != senders.End(); it++)
    {
        Ptr<Node> sender = *it;
        hostDevices.Add(GetNetDevices(sender));
    }

    for (auto it = ssenders.Begin(); it != ssenders.End(); it++)
    {
        Ptr<Node> sender = *it;
        hostDevices.Add(GetNetDevices(sender));
    }

    for (auto it = tsenders.Begin(); it != tsenders.End(); it++)
    {
        Ptr<Node> sender = *it;
        hostDevices.Add(GetNetDevices(sender));
    }

    InternetStackHelper internet;
    internet.Install(hosts);
    internet.Install(senders);
    internet.Install(ssenders);
    internet.Install(tsenders);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    auto addresses = ipv4.Assign(hostDevices);

    // Get Address: Device with index 0/1, address 0 (only one address)
    // Print all addresses as a sanity check 
    NS_LOG_INFO("Src, Dst IP addresses");

    // for (int k = 0; k < num_hosts; ++k){
    //     std::cout<< addresses.GetAddress(k, 0)<< std::endl;
    // }

    // print all IP addresses
    for (uint32_t i = 0; i < hostDevices.GetN(); ++i)
    {
        auto addr = addresses.GetAddress(i, 0);
        NS_LOG_INFO("IP: " << addr);
    }

    // List receivers which we use
    auto addrReceiver1 = addresses.GetAddress(0, 0);
    auto addrReceiver2 = addresses.GetAddress(2, 0);
    auto addrReceiver3 = addresses.GetAddress(4, 0);

    NS_LOG_INFO("Create Traffic Applications.");

    // Send multi application data to receiver 1
    uint16_t base_port1 = 4200; // Note: We need two ports per pair
    auto trafficStart1 =  TimeStream(1, 1 + start_window);

    for (auto i_app = 0; i_app < n_apps; ++i_app)
    {
        // We also need to set the appropriate tag at every application!

        // Addresses
        auto port = base_port1 + i_app;
        auto recvAddr1 = AddressValue(InetSocketAddress(addrReceiver1, port));

        // Sink
        Ptr<Application> sink = CreateObjectWithAttributes<PacketSink>(
            "Local", recvAddr1, "Protocol", TCP,
            "StartTime", simStart, "StopTime", simStop);
        receiver1->AddApplication(sink);

        // Sources for each workload
        // App indexing scheme: 0--n_apps-1: w1, n_apps -- 2n_apps-1: w2, etc.
       
        if (rate_w1 > 0)
        {
            auto _id = i_app;
            auto app_start_time = trafficStart1->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source1 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr1, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w1), "CdfFile", StringValue(w1),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source1->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 1, _id));
            senders.Get(_id)->AddApplication(source1);
        }
        if (rate_w2 > 0)
        {
            auto _id = i_app + n_apps;
            auto app_start_time = trafficStart1->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source2 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr1, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w2), "CdfFile", StringValue(w2),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source2->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 2, _id));
            senders.Get(_id)->AddApplication(source2);
        }
        if (rate_w3 > 0)
        {
            auto _id = i_app + (2 * n_apps);
            auto app_start_time = trafficStart1->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source3 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr1, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w3), "CdfFile", StringValue(w3),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source3->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 3, _id));
            senders.Get(_id)->AddApplication(source3);
        }
    }

    // Explicit congestion for receiver 1
    if (congestion1 > 0)
    {
        NS_LOG_INFO("Configure congestion app for receiver 1.");
        // Just blast UDP traffic from time to time
        Ptr<Application> congestion_sink = CreateObjectWithAttributes<PacketSink>(
            "Local", AddressValue(InetSocketAddress(addrReceiver1, 2100)),
            "Protocol", UDP, "StartTime", simStart, "StopTime", simStop);
        receiver1->AddApplication(congestion_sink);

        auto c_start_time = std::floor(trafficStart1->GetValue());
        std::cout << "Congestion starting at..." << c_start_time << std::endl;

        Ptr<Application> congestion_source = CreateObjectWithAttributes<OnOffApplication>(
            "Remote", AddressValue(InetSocketAddress(addrReceiver1, 2100)),
            "Protocol", UDP,
            "OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"),
            "OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"),
            "DataRate", DataRateValue(congestion1),
            "StartTime", TimeValue(Seconds(c_start_time)),
            "StopTime", simStop);
        disturbance1->AddApplication(congestion_source);
    }
    else
    {
        NS_LOG_INFO("No explicit congestion for receiver 1.");
    }


    // Send multi application data to receiver 2
    uint16_t base_port2 = 5200; // Note: We need two ports per pair
    auto trafficStart2 = TimeStream(1, 1 + start_window);

    for (auto i_app = 0; i_app < n_apps; ++i_app)
    {
        // We also need to set the appropriate tag at every application!

        // Addresses
        auto port = base_port2 + i_app;
        auto recvAddr2 = AddressValue(InetSocketAddress(addrReceiver2, port));

        // Sink
        Ptr<Application> sink = CreateObjectWithAttributes<PacketSink>(
            "Local", recvAddr2, "Protocol", TCP,
            "StartTime", simStart, "StopTime", simStop);
        receiver2->AddApplication(sink);

        // Sources for each workload
        // App indexing scheme: 0--n_apps-1: w1, n_apps -- 2n_apps-1: w2, etc.
        if (rate_w1 > 0)
        {
            auto _id = i_app;
            auto app_start_time = trafficStart2->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source1 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr2, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w1), "CdfFile", StringValue(w1),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source1->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 1, _id));
            senders.Get(_id)->AddApplication(source1);
        }
        if (rate_w2 > 0)
        {
            auto _id = i_app + n_apps;
            auto app_start_time = trafficStart2->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source2 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr2, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w2), "CdfFile", StringValue(w2),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source2->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 2,_id));
            senders.Get(_id)->AddApplication(source2);
        }
        if (rate_w3 > 0)
        {
            auto _id = i_app + (2 * n_apps);
            auto app_start_time = trafficStart2->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source3 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr2, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w3), "CdfFile", StringValue(w3),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source3->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 3, _id));
            senders.Get(_id)->AddApplication(source3);
        }
    }

    // Explicit congestion for receiver 2
    if (congestion2 > 0)
    {
        NS_LOG_INFO("Configure congestion app for receiver 2.");
        // Just blast UDP traffic from time to time
        Ptr<Application> congestion_sink = CreateObjectWithAttributes<PacketSink>(
            "Local", AddressValue(InetSocketAddress(addrReceiver2, 2100)),
            "Protocol", UDP, "StartTime", simStart, "StopTime", simStop);
        receiver2->AddApplication(congestion_sink);

        auto c_start_time = std::floor(trafficStart2->GetValue());
        std::cout << "Congestion starting at..." << c_start_time << std::endl;

        Ptr<Application> congestion_source = CreateObjectWithAttributes<OnOffApplication>(
            "Remote", AddressValue(InetSocketAddress(addrReceiver2, 2100)),
            "Protocol", UDP,
            "OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"),
            "OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"),
            "DataRate", DataRateValue(congestion2),
            "StartTime", TimeValue(Seconds(c_start_time)),
            "StopTime", simStop);
        disturbance2->AddApplication(congestion_source);
    }
    else
    {
        NS_LOG_INFO("No explicit congestion for receiver 2.");
    }

    // Send multi application data to receiver 3
    uint16_t base_port3 = 6200; // Note: We need two ports per pair
    auto trafficStart3 = TimeStream(1, 1 + start_window);

    for (auto i_app = 0; i_app < n_apps; ++i_app)
    {
        // We also need to set the appropriate tag at every application!

        // Addresses
        auto port = base_port3 + i_app;
        auto recvAddr3 = AddressValue(InetSocketAddress(addrReceiver3, port));

        // Sink
        Ptr<Application> sink = CreateObjectWithAttributes<PacketSink>(
            "Local", recvAddr3, "Protocol", TCP,
            "StartTime", simStart, "StopTime", simStop);
        receiver3->AddApplication(sink);

        // Sources for each workload
        // App indexing scheme: 0--n_apps-1: w1, n_apps -- 2n_apps-1: w2, etc.
        if (rate_w1 > 0)
        {
            auto _id = i_app;
            auto app_start_time = trafficStart3->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source1 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr3, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w1), "CdfFile", StringValue(w1),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source1->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 1, _id));
            senders.Get(_id)->AddApplication(source1);
        }
        if (rate_w2 > 0)
        {
            auto _id = i_app + n_apps;
            auto app_start_time = trafficStart3->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source2 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr3, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w2), "CdfFile", StringValue(w2),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source2->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 2, _id));
            senders.Get(_id)->AddApplication(source2);
        }
        if (rate_w3 > 0)
        {
            auto _id = i_app + (2 * n_apps);
            auto app_start_time = trafficStart3->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source3 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr3, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w3), "CdfFile", StringValue(w3),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source3->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 3, _id));
            senders.Get(_id)->AddApplication(source3);
        }
    }

    // Explicit congestion for receiver 3
    if (congestion3 > 0)
    {
        NS_LOG_INFO("Configure congestion app for receiver 3.");
        // Just blast UDP traffic from time to time
        Ptr<Application> congestion_sink = CreateObjectWithAttributes<PacketSink>(
            "Local", AddressValue(InetSocketAddress(addrReceiver3, 2100)),
            "Protocol", UDP, "StartTime", simStart, "StopTime", simStop);
        receiver3->AddApplication(congestion_sink);

        auto c_start_time = std::floor(trafficStart1->GetValue());
        std::cout << "Congestion starting at..." << c_start_time << std::endl;

        Ptr<Application> congestion_source = CreateObjectWithAttributes<OnOffApplication>(
            "Remote", AddressValue(InetSocketAddress(addrReceiver3, 2100)),
            "Protocol", UDP,
            "OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"),
            "OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"),
            "DataRate", DataRateValue(congestion3),
            "StartTime", TimeValue(Seconds(c_start_time)),
            "StopTime", simStop);
        disturbance3->AddApplication(congestion_source);
    }
    else
    {
        NS_LOG_INFO("No explicit congestion for receiver 3.");
    }

    // Send multi application data to receiver 1
    uint16_t base_port4 = 7200; // Note: We need two ports per pair
    // auto trafficStart1 =  TimeStream(1, 1 + start_window);

    for (auto i_app = 0; i_app < n_apps; ++i_app)
    {
        // We also need to set the appropriate tag at every application!

        // Addresses
        auto port = base_port4 + i_app;
        auto recvAddr1 = AddressValue(InetSocketAddress(addrReceiver1, port));

        // Sink
        Ptr<Application> sink = CreateObjectWithAttributes<PacketSink>(
            "Local", recvAddr1, "Protocol", TCP,
            "StartTime", simStart, "StopTime", simStop);
        receiver1->AddApplication(sink);

        // Sources for each workload
        // App indexing scheme: 0--n_apps-1: w1, n_apps -- 2n_apps-1: w2, etc.
       
        if (rate_w1 > 0)
        {
            auto _id = i_app;
            auto app_start_time = trafficStart1->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source1 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr1, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w1), "CdfFile", StringValue(w1),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source1->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 1, _id));
            ssenders.Get(_id)->AddApplication(source1);
        }
        if (rate_w2 > 0)
        {
            auto _id = i_app + n_apps;
            auto app_start_time = trafficStart1->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source2 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr1, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w2), "CdfFile", StringValue(w2),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source2->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 2, _id));
            ssenders.Get(_id)->AddApplication(source2);
        }
        if (rate_w3 > 0)
        {
            auto _id = i_app + (2 * n_apps);
            auto app_start_time = trafficStart1->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source3 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr1, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w3), "CdfFile", StringValue(w3),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source3->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 3, _id));
            ssenders.Get(_id)->AddApplication(source3);
        }
    }

    // Explicit congestion for receiver 1
    if (congestion1 > 0)
    {
        NS_LOG_INFO("Configure congestion app for receiver 1.");
        // Just blast UDP traffic from time to time
        Ptr<Application> congestion_sink = CreateObjectWithAttributes<PacketSink>(
            "Local", AddressValue(InetSocketAddress(addrReceiver1, 2100)),
            "Protocol", UDP, "StartTime", simStart, "StopTime", simStop);
        receiver1->AddApplication(congestion_sink);

        auto c_start_time = std::floor(trafficStart1->GetValue());
        std::cout << "Congestion starting at..." << c_start_time << std::endl;

        Ptr<Application> congestion_source = CreateObjectWithAttributes<OnOffApplication>(
            "Remote", AddressValue(InetSocketAddress(addrReceiver1, 2100)),
            "Protocol", UDP,
            "OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"),
            "OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"),
            "DataRate", DataRateValue(congestion1),
            "StartTime", TimeValue(Seconds(c_start_time)),
            "StopTime", simStop);
        disturbance1->AddApplication(congestion_source);
    }
    else
    {
        NS_LOG_INFO("No explicit congestion for receiver 1.");
    }


    // Send multi application data to receiver 2
    uint16_t base_port5 = 8200; // Note: We need two ports per pair
    // auto trafficStart2 = TimeStream(1, 1 + start_window);

    for (auto i_app = 0; i_app < n_apps; ++i_app)
    {
        // We also need to set the appropriate tag at every application!

        // Addresses
        auto port = base_port5 + i_app;
        auto recvAddr2 = AddressValue(InetSocketAddress(addrReceiver2, port));

        // Sink
        Ptr<Application> sink = CreateObjectWithAttributes<PacketSink>(
            "Local", recvAddr2, "Protocol", TCP,
            "StartTime", simStart, "StopTime", simStop);
        receiver2->AddApplication(sink);

        // Sources for each workload
        // App indexing scheme: 0--n_apps-1: w1, n_apps -- 2n_apps-1: w2, etc.
        if (rate_w1 > 0)
        {
            auto _id = i_app;
            auto app_start_time = trafficStart2->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source1 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr2, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w1), "CdfFile", StringValue(w1),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source1->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 1, _id));
            ssenders.Get(_id)->AddApplication(source1);
        }
        if (rate_w2 > 0)
        {
            auto _id = i_app + n_apps;
            auto app_start_time = trafficStart2->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source2 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr2, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w2), "CdfFile", StringValue(w2),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source2->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 2,_id));
            ssenders.Get(_id)->AddApplication(source2);
        }
        if (rate_w3 > 0)
        {
            auto _id = i_app + (2 * n_apps);
            auto app_start_time = trafficStart2->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source3 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr2, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w3), "CdfFile", StringValue(w3),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source3->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 3, _id));
            ssenders.Get(_id)->AddApplication(source3);
        }
    }

    // Explicit congestion for receiver 2
    if (congestion2 > 0)
    {
        NS_LOG_INFO("Configure congestion app for receiver 2.");
        // Just blast UDP traffic from time to time
        Ptr<Application> congestion_sink = CreateObjectWithAttributes<PacketSink>(
            "Local", AddressValue(InetSocketAddress(addrReceiver2, 2100)),
            "Protocol", UDP, "StartTime", simStart, "StopTime", simStop);
        receiver2->AddApplication(congestion_sink);

        auto c_start_time = std::floor(trafficStart2->GetValue());
        std::cout << "Congestion starting at..." << c_start_time << std::endl;

        Ptr<Application> congestion_source = CreateObjectWithAttributes<OnOffApplication>(
            "Remote", AddressValue(InetSocketAddress(addrReceiver2, 2100)),
            "Protocol", UDP,
            "OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"),
            "OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"),
            "DataRate", DataRateValue(congestion2),
            "StartTime", TimeValue(Seconds(c_start_time)),
            "StopTime", simStop);
        disturbance2->AddApplication(congestion_source);
    }
    else
    {
        NS_LOG_INFO("No explicit congestion for receiver 2.");
    }

    // Send multi application data to receiver 3
    uint16_t base_port6 = 9200; // Note: We need two ports per pair
    // auto trafficStart3 = TimeStream(1, 1 + start_window);

    for (auto i_app = 0; i_app < n_apps; ++i_app)
    {
        // We also need to set the appropriate tag at every application!

        // Addresses
        auto port = base_port6 + i_app;
        auto recvAddr3 = AddressValue(InetSocketAddress(addrReceiver3, port));

        // Sink
        Ptr<Application> sink = CreateObjectWithAttributes<PacketSink>(
            "Local", recvAddr3, "Protocol", TCP,
            "StartTime", simStart, "StopTime", simStop);
        receiver3->AddApplication(sink);

        // Sources for each workload
        // App indexing scheme: 0--n_apps-1: w1, n_apps -- 2n_apps-1: w2, etc.
        if (rate_w1 > 0)
        {
            auto _id = i_app;
            auto app_start_time = trafficStart3->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source1 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr3, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w1), "CdfFile", StringValue(w1),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source1->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 1, _id));
            ssenders.Get(_id)->AddApplication(source1);
        }
        if (rate_w2 > 0)
        {
            auto _id = i_app + n_apps;
            auto app_start_time = trafficStart3->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source2 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr3, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w2), "CdfFile", StringValue(w2),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source2->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 2, _id));
            ssenders.Get(_id)->AddApplication(source2);
        }
        if (rate_w3 > 0)
        {
            auto _id = i_app + (2 * n_apps);
            auto app_start_time = trafficStart3->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source3 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr3, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w3), "CdfFile", StringValue(w3),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source3->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 3, _id));
            ssenders.Get(_id)->AddApplication(source3);
        }
    }

    // Explicit congestion for receiver 3
    if (congestion3 > 0)
    {
        NS_LOG_INFO("Configure congestion app for receiver 3.");
        // Just blast UDP traffic from time to time
        Ptr<Application> congestion_sink = CreateObjectWithAttributes<PacketSink>(
            "Local", AddressValue(InetSocketAddress(addrReceiver3, 2100)),
            "Protocol", UDP, "StartTime", simStart, "StopTime", simStop);
        receiver3->AddApplication(congestion_sink);

        auto c_start_time = std::floor(trafficStart1->GetValue());
        std::cout << "Congestion starting at..." << c_start_time << std::endl;

        Ptr<Application> congestion_source = CreateObjectWithAttributes<OnOffApplication>(
            "Remote", AddressValue(InetSocketAddress(addrReceiver3, 2100)),
            "Protocol", UDP,
            "OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"),
            "OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"),
            "DataRate", DataRateValue(congestion3),
            "StartTime", TimeValue(Seconds(c_start_time)),
            "StopTime", simStop);
        disturbance3->AddApplication(congestion_source);
    }
    else
    {
        NS_LOG_INFO("No explicit congestion for receiver 3.");
    }

    // Send multi application data to receiver 1
    uint16_t base_port7 = 10200; // Note: We need two ports per pair
    // auto trafficStart1 =  TimeStream(1, 1 + start_window);

    for (auto i_app = 0; i_app < n_apps; ++i_app)
    {
        // We also need to set the appropriate tag at every application!

        // Addresses
        auto port = base_port7 + i_app;
        auto recvAddr1 = AddressValue(InetSocketAddress(addrReceiver1, port));

        // Sink
        Ptr<Application> sink = CreateObjectWithAttributes<PacketSink>(
            "Local", recvAddr1, "Protocol", TCP,
            "StartTime", simStart, "StopTime", simStop);
        receiver1->AddApplication(sink);

        // Sources for each workload
        // App indexing scheme: 0--n_apps-1: w1, n_apps -- 2n_apps-1: w2, etc.
       
        if (rate_w1 > 0)
        {
            auto _id = i_app;
            auto app_start_time = trafficStart1->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source1 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr1, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w1), "CdfFile", StringValue(w1),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source1->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 1, _id));
            tsenders.Get(_id)->AddApplication(source1);
        }
        if (rate_w2 > 0)
        {
            auto _id = i_app + n_apps;
            auto app_start_time = trafficStart1->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source2 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr1, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w2), "CdfFile", StringValue(w2),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source2->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 2, _id));
            tsenders.Get(_id)->AddApplication(source2);
        }
        if (rate_w3 > 0)
        {
            auto _id = i_app + (2 * n_apps);
            auto app_start_time = trafficStart1->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source3 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr1, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w3), "CdfFile", StringValue(w3),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source3->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 3, _id));
            tsenders.Get(_id)->AddApplication(source3);
        }
    }

    // Explicit congestion for receiver 1
    if (congestion1 > 0)
    {
        NS_LOG_INFO("Configure congestion app for receiver 1.");
        // Just blast UDP traffic from time to time
        Ptr<Application> congestion_sink = CreateObjectWithAttributes<PacketSink>(
            "Local", AddressValue(InetSocketAddress(addrReceiver1, 2100)),
            "Protocol", UDP, "StartTime", simStart, "StopTime", simStop);
        receiver1->AddApplication(congestion_sink);

        auto c_start_time = std::floor(trafficStart1->GetValue());
        std::cout << "Congestion starting at..." << c_start_time << std::endl;

        Ptr<Application> congestion_source = CreateObjectWithAttributes<OnOffApplication>(
            "Remote", AddressValue(InetSocketAddress(addrReceiver1, 2100)),
            "Protocol", UDP,
            "OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"),
            "OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"),
            "DataRate", DataRateValue(congestion1),
            "StartTime", TimeValue(Seconds(c_start_time)),
            "StopTime", simStop);
        disturbance1->AddApplication(congestion_source);
    }
    else
    {
        NS_LOG_INFO("No explicit congestion for receiver 1.");
    }


    // Send multi application data to receiver 2
    uint16_t base_port8 = 11200; // Note: We need two ports per pair
    // auto trafficStart2 = TimeStream(1, 1 + start_window);

    for (auto i_app = 0; i_app < n_apps; ++i_app)
    {
        // We also need to set the appropriate tag at every application!

        // Addresses
        auto port = base_port8 + i_app;
        auto recvAddr2 = AddressValue(InetSocketAddress(addrReceiver2, port));

        // Sink
        Ptr<Application> sink = CreateObjectWithAttributes<PacketSink>(
            "Local", recvAddr2, "Protocol", TCP,
            "StartTime", simStart, "StopTime", simStop);
        receiver2->AddApplication(sink);

        // Sources for each workload
        // App indexing scheme: 0--n_apps-1: w1, n_apps -- 2n_apps-1: w2, etc.
        if (rate_w1 > 0)
        {
            auto _id = i_app;
            auto app_start_time = trafficStart2->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source1 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr2, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w1), "CdfFile", StringValue(w1),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source1->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 1, _id));
            tsenders.Get(_id)->AddApplication(source1);
        }
        if (rate_w2 > 0)
        {
            auto _id = i_app + n_apps;
            auto app_start_time = trafficStart2->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source2 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr2, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w2), "CdfFile", StringValue(w2),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source2->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 2,_id));
            tsenders.Get(_id)->AddApplication(source2);
        }
        if (rate_w3 > 0)
        {
            auto _id = i_app + (2 * n_apps);
            auto app_start_time = trafficStart2->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source3 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr2, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w3), "CdfFile", StringValue(w3),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source3->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 3, _id));
            tsenders.Get(_id)->AddApplication(source3);
        }
    }

    // Explicit congestion for receiver 2
    if (congestion2 > 0)
    {
        NS_LOG_INFO("Configure congestion app for receiver 2.");
        // Just blast UDP traffic from time to time
        Ptr<Application> congestion_sink = CreateObjectWithAttributes<PacketSink>(
            "Local", AddressValue(InetSocketAddress(addrReceiver2, 2100)),
            "Protocol", UDP, "StartTime", simStart, "StopTime", simStop);
        receiver2->AddApplication(congestion_sink);

        auto c_start_time = std::floor(trafficStart2->GetValue());
        std::cout << "Congestion starting at..." << c_start_time << std::endl;

        Ptr<Application> congestion_source = CreateObjectWithAttributes<OnOffApplication>(
            "Remote", AddressValue(InetSocketAddress(addrReceiver2, 2100)),
            "Protocol", UDP,
            "OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"),
            "OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"),
            "DataRate", DataRateValue(congestion2),
            "StartTime", TimeValue(Seconds(c_start_time)),
            "StopTime", simStop);
        disturbance2->AddApplication(congestion_source);
    }
    else
    {
        NS_LOG_INFO("No explicit congestion for receiver 2.");
    }

    // Send multi application data to receiver 3
    uint16_t base_port9 = 12200; // Note: We need two ports per pair
    // auto trafficStart3 = TimeStream(1, 1 + start_window);

    for (auto i_app = 0; i_app < n_apps; ++i_app)
    {
        // We also need to set the appropriate tag at every application!

        // Addresses
        auto port = base_port9 + i_app;
        auto recvAddr3 = AddressValue(InetSocketAddress(addrReceiver3, port));

        // Sink
        Ptr<Application> sink = CreateObjectWithAttributes<PacketSink>(
            "Local", recvAddr3, "Protocol", TCP,
            "StartTime", simStart, "StopTime", simStop);
        receiver3->AddApplication(sink);

        // Sources for each workload
        // App indexing scheme: 0--n_apps-1: w1, n_apps -- 2n_apps-1: w2, etc.
        if (rate_w1 > 0)
        {
            auto _id = i_app;
            auto app_start_time = trafficStart3->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source1 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr3, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w1), "CdfFile", StringValue(w1),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source1->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 1, _id));
            tsenders.Get(_id)->AddApplication(source1);
        }
        if (rate_w2 > 0)
        {
            auto _id = i_app + n_apps;
            auto app_start_time = trafficStart3->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source2 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr3, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w2), "CdfFile", StringValue(w2),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source2->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 2, _id));
            tsenders.Get(_id)->AddApplication(source2);
        }
        if (rate_w3 > 0)
        {
            auto _id = i_app + (2 * n_apps);
            auto app_start_time = trafficStart3->GetValue();
            // Round the start time to the lower integer
            app_start_time = std::floor(app_start_time);
            std::cout<< "App " << _id << " starting at..." <<app_start_time << std::endl;
            Ptr<CdfApplication> source3 = CreateObjectWithAttributes<CdfApplication>(
                "Remote", recvAddr3, "Protocol", TCP,
                "DataRate", DataRateValue(rate_w3), "CdfFile", StringValue(w3),
                "StartTime", TimeValue(Seconds(app_start_time)),
                "StopTime", simStop);
            source3->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&setIdTag, 3, _id));
            tsenders.Get(_id)->AddApplication(source3);
        }
    }

    // Explicit congestion for receiver 3
    if (congestion3 > 0)
    {
        NS_LOG_INFO("Configure congestion app for receiver 3.");
        // Just blast UDP traffic from time to time
        Ptr<Application> congestion_sink = CreateObjectWithAttributes<PacketSink>(
            "Local", AddressValue(InetSocketAddress(addrReceiver3, 2100)),
            "Protocol", UDP, "StartTime", simStart, "StopTime", simStop);
        receiver3->AddApplication(congestion_sink);

        auto c_start_time = std::floor(trafficStart1->GetValue());
        std::cout << "Congestion starting at..." << c_start_time << std::endl;

        Ptr<Application> congestion_source = CreateObjectWithAttributes<OnOffApplication>(
            "Remote", AddressValue(InetSocketAddress(addrReceiver3, 2100)),
            "Protocol", UDP,
            "OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"),
            "OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"),
            "DataRate", DataRateValue(congestion3),
            "StartTime", TimeValue(Seconds(c_start_time)),
            "StopTime", simStop);
        disturbance3->AddApplication(congestion_source);
    }
    else
    {
        NS_LOG_INFO("No explicit congestion for receiver 3.");
    }

    NS_LOG_INFO("Install Tracing");
    AsciiTraceHelper asciiTraceHelper;

    // Log (one-way) delay from sender to receiver (excludes other sources).
    std::stringstream trackfilename;
    // trackfilename << prefix << "_delays.csv";
    trackfilename << prefix << ".csv"; // only one file for now.
    auto trackfile = asciiTraceHelper.CreateFileStream(trackfilename.str());

    for (auto it = senders.Begin(); it != senders.End(); it++)
    {
        Ptr<Node> sender = *it;
        sender->GetDevice(0)->TraceConnectWithoutContext(
            "MacTx", MakeCallback(&setTimeTag));
    }

    for (auto it = ssenders.Begin(); it != ssenders.End(); it++)
    {
        Ptr<Node> sender = *it;
        sender->GetDevice(0)->TraceConnectWithoutContext(
            "MacTx", MakeCallback(&setTimeTag));

        // std::stringstream sender_trackfilename;
        // sender_trackfilename << prefix << "_sender_" << sender->GetId() << ".csv";
        // auto sender_trackfile = asciiTraceHelper.CreateFileStream(sender_trackfilename.str());

        // sender->GetDevice(0)->TraceConnectWithoutContext(
        //     "MacTx", MakeBoundCallback(&logSenderPacket, sender_trackfile));
    }

    for (auto it = tsenders.Begin(); it != tsenders.End(); it++)
    {
        Ptr<Node> sender = *it;
        sender->GetDevice(0)->TraceConnectWithoutContext(
            "MacTx", MakeCallback(&setTimeTag));
    }

    /*sender->GetDevice(0)->TraceConnectWithoutContext(
        "MacTx", MakeCallback(&setTimeTag));*/

    // Separate log for each receiver
    std::stringstream receiver1filename;
    receiver1filename << "results_test_large/" << "_receiver1.csv";
    auto receiver1file = asciiTraceHelper.CreateFileStream(receiver1filename.str());
    receiver1->GetDevice(0)->TraceConnectWithoutContext(
        "MacRx", MakeBoundCallback(&logPacketInfo, receiver1file));
    
    std::stringstream receiver2filename;
    receiver2filename << "results_test_large/" << "_receiver2.csv";
    auto receiver2file = asciiTraceHelper.CreateFileStream(receiver2filename.str());
    receiver2->GetDevice(0)->TraceConnectWithoutContext(
        "MacRx", MakeBoundCallback(&logPacketInfo, receiver2file));

    std::stringstream receiver3filename;
    receiver3filename << "results_test_large/" << "_receiver3.csv";
    auto receiver3file = asciiTraceHelper.CreateFileStream(receiver3filename.str());
    receiver3->GetDevice(0)->TraceConnectWithoutContext(
        "MacRx", MakeBoundCallback(&logPacketInfo, receiver3file));

    // Log packetInfo at the switches A, C and E
    std::stringstream switchAfilename1;
    switchAfilename1 << "results_test_large/" << "switch_A_port1.csv";
    auto switchAfile1 = asciiTraceHelper.CreateFileStream(switchAfilename1.str());
    switchA->GetDevice(0)->TraceConnectWithoutContext(
        "MacTx", MakeBoundCallback(&logPacketInfo, switchAfile1));

    std::stringstream switchAfilename2;
    switchAfilename2 << "results_test_large/" << "switch_A_port2.csv";
    auto switchAfile2 = asciiTraceHelper.CreateFileStream(switchAfilename2.str());
    switchA->GetDevice(1)->TraceConnectWithoutContext(
        "MacTx", MakeBoundCallback(&logPacketInfo, switchAfile2));

    std::stringstream switchAfilename3;
    switchAfilename3 << "results_test_large/" << "switch_A_port3.csv";
    auto switchAfile3 = asciiTraceHelper.CreateFileStream(switchAfilename3.str());
    switchA->GetDevice(2)->TraceConnectWithoutContext(
        "MacTx", MakeBoundCallback(&logPacketInfo, switchAfile3));

    std::stringstream switchCfilename1;
    switchCfilename1 << "results_test_large/" << "switch_C_port1.csv";
    auto switchCfile1 = asciiTraceHelper.CreateFileStream(switchCfilename1.str());
    switchC->GetDevice(0)->TraceConnectWithoutContext(
        "MacTx", MakeBoundCallback(&logPacketInfo, switchCfile1));

    std::stringstream switchCfilename2;
    switchCfilename2 << "results_test_large/" << "switch_C_port2.csv";
    auto switchCfile2 = asciiTraceHelper.CreateFileStream(switchCfilename2.str());
    switchC->GetDevice(1)->TraceConnectWithoutContext(
        "MacTx", MakeBoundCallback(&logPacketInfo, switchCfile2));

    std::stringstream switchCfilename3;
    switchCfilename3 << "results_test_large/" << "switch_C_port3.csv";
    auto switchCfile3 = asciiTraceHelper.CreateFileStream(switchCfilename3.str());
    switchC->GetDevice(2)->TraceConnectWithoutContext(
        "MacTx", MakeBoundCallback(&logPacketInfo, switchCfile3));

    std::stringstream switchEfilename1;
    switchEfilename1 << "results_test_large/" << "switch_E_port1.csv";
    auto switchEfile1 = asciiTraceHelper.CreateFileStream(switchEfilename1.str());
    switchE->GetDevice(0)->TraceConnectWithoutContext(
        "MacTx", MakeBoundCallback(&logPacketInfo, switchEfile1));

    std::stringstream switchEfilename2;
    switchEfilename2 << "results_test_large/" << "switch_E_port2.csv";
    auto switchEfile2 = asciiTraceHelper.CreateFileStream(switchEfilename2.str());
    switchE->GetDevice(1)->TraceConnectWithoutContext(
        "MacTx", MakeBoundCallback(&logPacketInfo, switchEfile2));

    std::stringstream switchEfilename3;
    switchEfilename3 << "results_test_large/" << "switch_E_port3.csv";
    auto switchEfile3 = asciiTraceHelper.CreateFileStream(switchEfilename3.str());
    switchE->GetDevice(2)->TraceConnectWithoutContext(
        "MacTx", MakeBoundCallback(&logPacketInfo, switchEfile3));
    
    //csma.EnablePcapAll("csma-bridge", false);

    // Track queues
    auto queuefile = asciiTraceHelper.CreateFileStream("results_test_large/queue.csv");
    Config::Connect(
        "/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/TxQueue/PacketsInQueue",
        MakeBoundCallback(&logValue, queuefile));

    auto dropfile = asciiTraceHelper.CreateFileStream("results_test_large/drops.csv");
    Config::Connect(
        "/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/MacTxDrop",
        MakeBoundCallback(&logDrop, dropfile));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    std::time_t end_time = std::chrono::system_clock::to_time_t(end);

    std::cout << "Finished computation at " << std::ctime(&end_time)
              << "Elapsed time: " << elapsed_seconds.count() << "s\n";
}
