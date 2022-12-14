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
//        n0----n1---n2
//

#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Probing");

int main(int argc, char *argv[])
{
  //
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
  //
  //LogComponentEnable ("CsmaNetDevice", LOG_LEVEL_ALL);
  //
  // Allow the user to override any of the defaults and the above Bind() at
  // run-time, via command-line arguments
  //
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Default configurations

  // EDGARS addition, test without first
  //Config::SetDefault("ns3::CsmaChannel::FullDuplex", BooleanValue(true)); //same than DupAckThreshold

  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(917440));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(917440));

  //
  // Explicitly create the nodes required by the topology (shown above).
  //
  NS_LOG_INFO("Create nodes.");
  NodeContainer hosts;
  hosts.Create(3);

  NS_LOG_INFO("Build Topology");
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

  auto link01 = pointToPoint.Install(NodeContainer(hosts.Get(0), hosts.Get(1)));
  auto link12 = pointToPoint.Install(NodeContainer(hosts.Get(1), hosts.Get(2)));

  // Add internet stack to the terminals
  InternetStackHelper internet;
  internet.Install(hosts);

  // We've got the "hardware" in place.  Now we need to add IP addresses.
  //
  NS_LOG_INFO("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  auto if01 = ipv4.Assign(link01);
  ipv4.NewNetwork();
  auto if12 = ipv4.Assign(link12);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  NS_LOG_INFO("Create Applications.");
  uint16_t port = 6001;

  NS_LOG_INFO(if12.GetAddress(1));

  BulkSendHelper sender("ns3::TcpSocketFactory",
                        Address(InetSocketAddress(if12.GetAddress(1), port)));
  sender.SetAttribute("SendSize", UintegerValue(1024));
  sender.SetAttribute("MaxBytes", UintegerValue(100 * 1000000));

  ApplicationContainer app = sender.Install(hosts.Get(0));
  // Start the application
  app.Start(Seconds(1.0));
  app.Stop(Seconds(5));

  // Create an optional packet sink to receive these packets
  PacketSinkHelper sink("ns3::TcpSocketFactory",
                        Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
  app = sink.Install(hosts.Get(1));
  app.Start(Seconds(0.0));

  NS_LOG_INFO("Configure Tracing.");

  pointToPoint.EnablePcapAll("probing-bandwidth");

  //
  // Now, do the actual simulation.
  //
  NS_LOG_INFO("Run Simulation.");
  Simulator::Stop(Seconds(5));
  Simulator::Run();
  Simulator::Destroy();
  NS_LOG_INFO("Done.");
}
