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

// Network topology
//
//                               disturbance
//                                  |
//        sender --- switchA --- switchB --- receiver
//
//  The "disturbance" host is used to introduce changes in the network
//  conditions.
//

#include <iostream>
#include <fstream>
#include <unordered_map>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

#include "ns3/probing-client.h"
#include "ns3/probing-server.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MiniTopology");

const auto TCP = TypeIdValue(TcpSocketFactory::GetTypeId());
const auto UDP = TypeIdValue(UdpSocketFactory::GetTypeId());

void RTTMeasurement(Ptr<OutputStreamWrapper> stream, uint32_t burst, Time rtt)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << ','
                         << burst << ','
                         << rtt.GetSeconds() << std::endl;
}

// Put the current timestamp and packet size into a log.
void logSize(Ptr<OutputStreamWrapper> stream, Ptr<Packet const> p)
{
    auto current_time = Simulator::Now();
    *stream->GetStream() << current_time.GetSeconds() << ','
                         << p->GetSize() << std::endl;
}

// A timestamp tag that can be added to a packet.
class TimestampTag : public Tag
{
public:
    static TypeId GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::TimestampTag")
                                .SetParent<Tag>()
                                .AddConstructor<TimestampTag>()
                                .AddAttribute("Timestamp",
                                              "Timestamp to save in tag.",
                                              EmptyAttributeValue(),
                                              MakeTimeAccessor(&TimestampTag::timestamp),
                                              MakeTimeChecker());
        return tid;
    };
    TypeId GetInstanceTypeId(void) const { return GetTypeId(); };
    uint32_t GetSerializedSize(void) const { return sizeof(timestamp); };
    void Serialize(TagBuffer i) const
    {
        i.Write(reinterpret_cast<const uint8_t *>(&timestamp),
                sizeof(timestamp));
    };
    void Deserialize(TagBuffer i)
    {
        i.Read(reinterpret_cast<uint8_t *>(&timestamp), sizeof(timestamp));
    };
    void Print(std::ostream &os) const
    {
        os << "t=" << timestamp;
    };

    // these are our accessors to our tag structure
    void SetTime(Time time) { timestamp = time; };
    Time GetTime() { return timestamp; };

private:
    Time timestamp;
};

// Tag a packet with a timestamp.
void setTimeTag(Ptr<Packet const> p)
{
    TimestampTag tag;
    tag.SetTime(Simulator::Now());
    p->AddPacketTag(tag);
};

// If the packet is tagged with a timestamp, write current time and tag
// into the stream.
void logTimeTag(Ptr<OutputStreamWrapper> stream, Ptr<Packet const> p)
{
    TimestampTag tag;
    if (p->PeekPacketTag(tag))
    {
        auto current_time = Simulator::Now();
        auto diff_time = current_time - tag.GetTime();
        *stream->GetStream() << current_time.GetSeconds() << ','
                             << diff_time.GetSeconds() << std::endl;
    }
    else
    {
        NS_LOG_DEBUG("Packet without timestamp.");
    };
};

// TODO: Add base stream? Or how to get different random streams?
Ptr<RandomVariableStream> TimeStream(double min = 0.0, double max = 1.0)
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
    //
    // Users may find it convenient to turn on explicit debugging
    // for selected modules; the below lines suggest how to do this
    //
#if 1
    LogComponentEnable("MiniTopology", LOG_LEVEL_INFO);
#endif
    //
    // Allow the user to override any of the defaults and the above Bind() at
    // run-time, via command-line arguments
    //

    double interval = 1.0;
    u_int32_t burstsize = 1;

    CommandLine cmd;
    cmd.AddValue("interval", "Seconds to wait between probes.", interval);
    cmd.AddValue("burstsize", "Number of probing packets.", burstsize);
    cmd.Parse(argc, argv);

    // Simulation variables
    auto simStart = TimeValue(Seconds(0));
    auto stopTime = Seconds(10);
    auto simStop = TimeValue(stopTime);

    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer hosts;
    hosts.Create(3);
    // Keep references to sender, receiver, and disturbance
    auto sender = hosts.Get(0);
    auto receiver = hosts.Get(1);
    auto disturbance = hosts.Get(2);

    NodeContainer switches;
    switches.Create(2);
    auto switchA = switches.Get(0);
    auto switchB = switches.Get(1);

    NS_LOG_INFO("Build Topology");
    CsmaHelper csma;
    csma.SetChannelAttribute("FullDuplex", BooleanValue(true));
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));

    // Create the csma links
    csma.Install(NodeContainer(sender, switchA));
    csma.Install(NodeContainer(receiver, switchB));
    csma.Install(NodeContainer(disturbance, switchB));
    csma.Install(NodeContainer(switchA, switchB));

    // Create the bridge netdevice, turning the nodes into actual switches
    BridgeHelper bridge;
    bridge.Install(switchA, GetNetDevices(switchA));
    bridge.Install(switchB, GetNetDevices(switchB));

    NS_LOG_INFO("Install Tracing");
    AsciiTraceHelper asciiTraceHelper;

    // Packet size.
    std::stringstream sizefilename;
    sizefilename << "sizes.csv";
    auto sizefile = asciiTraceHelper.CreateFileStream(sizefilename.str());
    sender->GetDevice(0)->TraceConnectWithoutContext(
        "MacTx", MakeBoundCallback(&logSize, sizefile));

    // Log (one-way) delay from sender to receiver (excludes other sources).
    std::stringstream trackfilename;
    //TODO: custom file name?
    //trackfilename << "delays[" << interval << "," << burstsize << "].csv";
    trackfilename << "delays.csv";
    auto trackfile = asciiTraceHelper.CreateFileStream(trackfilename.str());
    sender->GetDevice(0)->TraceConnectWithoutContext(
        "MacTx", MakeCallback(&setTimeTag));
    receiver->GetDevice(0)->TraceConnectWithoutContext(
        "MacRx", MakeBoundCallback(&logTimeTag, trackfile));

    // Add internet stack and IP addresses to the hosts
    NS_LOG_INFO("Setup stack and assign IP Addresses.");
    NetDeviceContainer hostDevices;
    hostDevices.Add(GetNetDevices(sender));
    hostDevices.Add(GetNetDevices(receiver));
    hostDevices.Add(GetNetDevices(disturbance));

    InternetStackHelper internet;
    internet.Install(hosts);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    auto addresses = ipv4.Assign(hostDevices);
    // Get Address: Device with index 0/1, address 0 (only one address)
    auto addrSender = addresses.GetAddress(0, 0);
    auto addrReceiver = addresses.GetAddress(1, 0);
    // auto addrDisturbance = addresses.GetAddress(1, 0);

    /*
    // Create the probing client and server apps
    NS_LOG_INFO("Create Probing Applications.");
    uint16_t port = 9; // Discard port (RFC 863)

    auto probing_client = CreateObjectWithAttributes<ProbingClient>(
        "RemoteAddress", AddressValue(addrReceiver),
        "RemotePort", UintegerValue(port),
        "Interval", TimeValue(Seconds(interval)),
        "Burstsize", UintegerValue(burstsize),
        "StartTime", simStart,
        "StopTime", simStop);
    auto probing_server = CreateObjectWithAttributes<ProbingServer>(
        "Port", UintegerValue(port),
        "StartTime", simStart,
        "StopTime", simStop);

    // Install apps
    h0->AddApplication(probing_client);
    h1->AddApplication(probing_server);
    */

    NS_LOG_INFO("Create Traffic Applications.");
    //TODO: Turn into function?
    //TODO: Do I need bidirectional traffic?
    // Create applications that send traffic in both directions
    auto n_pairs = 1;          //20;
    uint16_t base_port = 4200; // Note: We need two ports per pair
    auto trafficStart = TimeStream(1, 2);
    for (auto i_pair = 0; i_pair < n_pairs; ++i_pair)
    {
        // Addresses
        auto base_port_pair = base_port + 2 * i_pair;

        auto appAddrSender = AddressValue(
            InetSocketAddress(addrSender, base_port_pair));
        auto appAddrReceiver = AddressValue(
            InetSocketAddress(addrReceiver, base_port_pair + 1));

        // Sinks
        Ptr<Application> sinkSender = CreateObjectWithAttributes<PacketSink>(
            "Local", appAddrSender, "Protocol", TCP,
            "StartTime", simStart, "StopTime", simStop);
        Ptr<Application> sinkReceiver = CreateObjectWithAttributes<PacketSink>(
            "Local", appAddrReceiver, "Protocol", TCP,
            "StartTime", simStart, "StopTime", simStop);
        sender->AddApplication(sinkSender);
        receiver->AddApplication(sinkReceiver);

        // Sources
        Ptr<Application> sourceSender = CreateObjectWithAttributes<OnOffApplication>(
            "Remote", appAddrReceiver, "Protocol", TCP,
            "OnTime", TimeStreamValue(), "OffTime", TimeStreamValue(),
            "DataRate", DataRateValue(DataRate("2Mbps")),
            "StartTime", TimeValue(Seconds(trafficStart->GetValue())),
            "StopTime", simStop);
        Ptr<Application> sourceReceiver = CreateObjectWithAttributes<OnOffApplication>(
            "Remote", appAddrSender, "Protocol", TCP,
            "OnTime", TimeStreamValue(), "OffTime", TimeStreamValue(),
            "DataRate", DataRateValue(DataRate("2Mbps")),
            "StartTime", TimeValue(Seconds(trafficStart->GetValue())),
            "StopTime", simStop);
        sender->AddApplication(sourceSender);
        receiver->AddApplication(sourceReceiver);
    }

    /*
    NS_LOG_INFO("Configure Congestion App.");
    // Just blast UDP traffic from time to time
    Ptr<Application> congestion_sink = CreateObjectWithAttributes<PacketSink>(
        "Local", AddressValue(InetSocketAddress(h1_address, 2100)),
        "Protocol", UDP, "StartTime", simStart, "StopTime", simStop);
    h1->AddApplication(congestion_sink);

    Ptr<Application> congestion_source = CreateObjectWithAttributes<OnOffApplication>(
        "Remote", AddressValue(InetSocketAddress(h1_address, 2100)),
        "Protocol", UDP,
        // Shorter and rarer bursts
        "OnTime", TimeStreamValue(0.1, 0.5),
        "OffTime", TimeStreamValue(2, 4),
        "DataRate", DataRateValue(DataRate("10Mbps")),
        "StartTime", TimeValue(Seconds(trafficStart->GetValue())),
        "StopTime", simStop);
    h0->AddApplication(congestion_source);
    */

    NS_LOG_INFO("Configure Tracing.");
    /*
    std::stringstream rtt_file;
    rtt_file << "rtt[" << interval << "," << burstsize << "].csv";
    probing_client->TraceConnectWithoutContext(
        "RTT",
        MakeBoundCallback(
            &RTTMeasurement,
            asciiTraceHelper.CreateFileStream(rtt_file.str())));
    */
    //
    // Configure tracing of all enqueue, dequeue, and NetDevice receive events.
    // Trace output will be sent to the file "csma-bridge.tr"
    //
    //AsciiTraceHelper ascii;
    //csma.EnableAsciiAll(ascii.CreateFileStream("csma-bridge.tr"));

    //
    // Also configure some tcpdump traces; each interface will be traced.
    // The output files will be named:
    //     csma-bridge-<nodeId>-<interfaceId>.pcap
    // and can be read by the "tcpdump -r" command (use "-tt" option to
    // display timestamps correctly)
    //
    csma.EnablePcapAll("csma-bridge", false);

    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");
}
