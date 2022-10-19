/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "traffic-generator.cc"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/nix-vector-routing-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <numeric>

#ifdef NS3_MTP
#include "ns3/mtp-module.h"
#endif

#ifdef NS3_MPI
#include "ns3/mpi-module.h"
#endif

#ifdef NS3_MPI
#define RANK(r) (r)
#define LOCAL(r) ((r) == conf::rank)
#else
#define RANK(r) (conf::system ? ((r) + 1) : 0)
#define LOCAL(r) true
#endif

#define LOG(content)           \
  {                            \
    if (conf::rank == 0)       \
      cout << content << endl; \
  }

using namespace std;
using namespace chrono;
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Scratch");

namespace conf {

// fat-tree scale
uint32_t k = 4;
uint32_t cluster = 0;

// bcube scale
uint32_t n = 4;
uint32_t level = 2;

// 2d torus scale
uint32_t row = 4;
uint32_t col = 4;

// spine-leaf scale
uint32_t spine = 4;
uint32_t leaf = 4;
uint32_t host = 4;

// LAN topo
string topo = "scratch/topos/geant.graphml";

// link layer options
uint32_t mtu = 1500;
uint32_t delay = 3000;
string bandwidth = "10Gbps";

// traffic-control layer options
string buffer = "4MB";
bool ecn = true;

// network layer options
bool nix = false;
bool rip = false;
bool ecmp = true;
bool flow = true;

// transport layer options
uint32_t port = 443;
string socket = "ns3::TcpSocketFactory";
string tcp = "ns3::TcpDctcp";

// application layer options
uint32_t size = 1448;
string cdf = "scratch/cdf/web-search.txt";
double load = 0.3;
double incast = 0;
string victim = "0";

// simulation options
string seed = "";
bool flowmon = false;
double time = 1;
double interval = 0.1;

// mtp options
uint32_t thread = 0;
uint32_t period = 0;
string sort = "ByExecutionTime";

// mpi options
uint32_t system = 0;
uint32_t rank = 0;
bool nullmsg = false;

}; // namespace conf

void
Initialize (int argc, char *argv[])
{
  CommandLine cmd;

  // parse scale
  cmd.AddValue ("k", "Number of pods in a fat-tree", conf::k);
  cmd.AddValue ("cluster", "Number of clusters in a variant fat-tree", conf::cluster);
  cmd.AddValue ("n", "Number of servers in one BCube", conf::n);
  cmd.AddValue ("row", "Number of rows in 2D torus", conf::row);
  cmd.AddValue ("col", "Number of columns in 2D torus", conf::col);
  cmd.AddValue ("spine", "Number of spines in spine-leaf", conf::spine);
  cmd.AddValue ("leaf", "Number of leaves in spine-leaf", conf::leaf);
  cmd.AddValue ("host", "Number of hosts in one leaf", conf::host);
  cmd.AddValue ("topo", "The topology file for LAN simulation", conf::topo);

  // parse network options
  cmd.AddValue ("mtu", "P2P link MTU", conf::mtu);
  cmd.AddValue ("delay", "Link delay in nanoseconds", conf::delay);
  cmd.AddValue ("bandwidth", "Link bandwidth", conf::bandwidth);
  cmd.AddValue ("buffer", "Switch buffer size", conf::buffer);
  cmd.AddValue ("ecn", "Use explicit congestion control", conf::ecn);
  cmd.AddValue ("nix", "Enable nix-vector routing", conf::nix);
  cmd.AddValue ("rip", "Enable RIP routing", conf::rip);
  cmd.AddValue ("ecmp", "Use equal-cost multi-path routing", conf::ecmp);
  cmd.AddValue ("flow", "Use per-flow ECMP routing", conf::flow);
  cmd.AddValue ("port", "Port number of server applications", conf::port);
  cmd.AddValue ("socket", "Socket protocol", conf::socket);
  cmd.AddValue ("tcp", "TCP protocol", conf::tcp);
  cmd.AddValue ("size", "Application packet size", conf::size);
  cmd.AddValue ("cdf", "Traffic CDF file location", conf::cdf);
  cmd.AddValue ("load", "Traffic load relative to bisection bandwidth", conf::load);
  cmd.AddValue ("incast", "Incast traffic ratio", conf::incast);
  cmd.AddValue ("victim", "Incast traffic victim list", conf::victim);

  // parse simulation options
  cmd.AddValue ("seed", "The seed of the random number generator", conf::seed);
  cmd.AddValue ("flowmon", "Use flow-monitor to record statistics", conf::flowmon);
  cmd.AddValue ("time", "Simulation time in seconds", conf::time);
  cmd.AddValue ("interval", "Simulation progreess print interval in seconds", conf::interval);

  // parse mtp/mpi options
  cmd.AddValue ("thread", "Maximum number of threads", conf::thread);
  cmd.AddValue ("period", "Logical process sorting period", conf::period);
  cmd.AddValue ("sort", "Logical process sorting method", conf::sort);
  cmd.AddValue ("system", "Number of logical processes in MTP manual partition", conf::system);
  cmd.AddValue ("nullmsg", "Enable null message algorithm", conf::nullmsg);
  cmd.Parse (argc, argv);

  // link layer settings
  Config::SetDefault ("ns3::PointToPointChannel::Delay", TimeValue (NanoSeconds (conf::delay)));
  Config::SetDefault ("ns3::PointToPointNetDevice::DataRate", StringValue (conf::bandwidth));
  Config::SetDefault ("ns3::PointToPointNetDevice::Mtu", UintegerValue (conf::mtu));

  // traffic control layer settings
  Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (conf::mtu));
  Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (conf::ecn));
  Config::SetDefault ("ns3::RedQueueDisc::UseHardDrop", BooleanValue (false));
  Config::SetDefault ("ns3::RedQueueDisc::LinkDelay", TimeValue (NanoSeconds (conf::delay)));
  Config::SetDefault ("ns3::RedQueueDisc::LinkBandwidth", StringValue (conf::bandwidth));
  Config::SetDefault ("ns3::RedQueueDisc::MaxSize", QueueSizeValue (QueueSize (conf::buffer)));
  Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (50));
  Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (150));

  // network layer settings
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue (conf::ecmp));
  Config::SetDefault ("ns3::Ipv4GlobalRouting::FlowEcmpRouting", BooleanValue (conf::flow));

  // transport layer settings
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue (conf::tcp));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (conf::size));
  Config::SetDefault ("ns3::TcpSocket::ConnTimeout",
                      TimeValue (conf::tcp == "ns3::TcpDctcp" ? MilliSeconds (10) : Seconds (3)));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1073725440));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1073725440));
  Config::SetDefault (
      "ns3::TcpSocketBase::MinRto",
      TimeValue (conf::tcp == "ns3::TcpDctcp" ? MilliSeconds (5) : MilliSeconds (200)));
  Config::SetDefault (
      "ns3::TcpSocketBase::ClockGranularity",
      TimeValue (conf::tcp == "ns3::TcpDctcp" ? MicroSeconds (100) : MilliSeconds (1)));
  Config::SetDefault ("ns3::RttEstimator::InitialEstimation",
                      TimeValue (conf::tcp == "ns3::TcpDctcp" ? MicroSeconds (200) : Seconds (1)));

  // application layer settings
  Config::SetDefault ("ns3::BulkSendApplication::SendSize", UintegerValue (UINT32_MAX));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (conf::bandwidth));
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (conf::size));
  Config::SetDefault ("ns3::OnOffApplication::OnTime",
                      StringValue ("ns3::ConstantRandomVariable[Constant=1000]"));
  Config::SetDefault ("ns3::OnOffApplication::OffTime",
                      StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  // simulation settings
  Time::SetResolution (Time::PS);
  RngSeedManager::SetSeed (Hash32 (conf::seed));

#ifdef NS3_MTP
#ifndef NS3_MPI
  // initialize mtp
  GlobalValue::Bind ("PartitionSchedulingMethod", StringValue (conf::sort));
  GlobalValue::Bind ("PartitionSchedulingPeriod", UintegerValue (conf::period));
  if (conf::system != 0)
    {
      MtpInterface::Enable (conf::thread, conf::system);
    }
  else if (conf::thread != 0)
    {
      MtpInterface::Enable (conf::thread);
    }
  else
    {
      MtpInterface::Enable ();
    }
#endif
#endif

#ifdef NS3_MPI
#ifndef NS3_MTP
  // initialize mpi
  if (conf::nullmsg)
    {
      GlobalValue::Bind ("SimulatorImplementationType",
                         StringValue ("ns3::NullMessageSimulatorImpl"));
    }
  else
    {
      GlobalValue::Bind ("SimulatorImplementationType",
                         StringValue ("ns3::DistributedSimulatorImpl"));
    }
  MpiInterface::Enable (&argc, &argv);
  conf::rank = MpiInterface::GetSystemId ();
  conf::system = MpiInterface::GetSize ();
#endif
#endif

#ifdef NS3_MTP
#ifdef NS3_MPI
  // initialize hybrid
  GlobalValue::Bind ("PartitionSchedulingMethod", StringValue (conf::sort));
  GlobalValue::Bind ("PartitionSchedulingPeriod", UintegerValue (conf::period));
  MtpInterface::Enable (conf::thread);
  MpiInterface::Enable (&argc, &argv);
  conf::rank = MpiInterface::GetSystemId ();
  conf::system = MpiInterface::GetSize ();
#endif
#endif
}

void
SetupRouting ()
{
  InternetStackHelper internet;
  if (conf::nix)
    {
      internet.SetRoutingHelper (Ipv4NixVectorHelper ());
    }
  else if (conf::rip)
    {
      internet.SetRoutingHelper (RipHelper ());
    }
  else
    {
      internet.SetRoutingHelper (Ipv4GlobalRoutingHelper ());
    }
  internet.SetIpv6StackInstall (false);
  internet.InstallAll ();
  LOG ("\n- Setup the topology...");
}

void
InstallTraffic (map<uint32_t, Ptr<Node>> &hosts, map<Ptr<Node>, Ipv4Address> &addrs,
                double bisection)
{
  // output address for debugging
  LOG ("\n- Calculating routes...");
  LOG ("  Host  NodeId  System  Address");
  for (auto &p : hosts)
    {
      LOG ("  " << left << setw (6) << p.first << setw (8) << p.second->GetId () << setw (8)
                << p.second->GetSystemId () << addrs[p.second]);
    }
  if (!conf::nix)
    {
      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    }

  // server applications
  PacketSinkHelper server (conf::socket, InetSocketAddress (Ipv4Address::GetAny (), conf::port));
  for (auto &p : hosts)
    {
      if (LOCAL (p.second->GetSystemId ()))
        {
          server.Install (p.second).Start (Seconds (0));
        }
    }

  // calculate traffic
  LOG ("\n- Generating traffic...");
  double bandwidth = bisection * DataRate (conf::bandwidth).GetBitRate () * 2;
  string victim;
  stringstream sin (conf::victim);
  vector<uint32_t> victims;
  while (getline (sin, victim, '-'))
    {
      victims.push_back (stoi (victim));
    }
  TrafficGenerator traffic (conf::cdf, hosts.size (), bandwidth * conf::load, conf::incast,
                            victims);

  // install traffic (client applications)
  auto flow = traffic.GetFlow ();
  while (get<0> (flow) < conf::time)
    {
      Ptr<Node> clientNode = hosts[get<1> (flow)];
      Ptr<Node> serverNode = hosts[get<2> (flow)];
      if (LOCAL (clientNode->GetSystemId ()))
        {
          if (conf::socket != "ns3::TcpSocketFactory")
            {
              OnOffHelper client (conf::socket, InetSocketAddress (addrs[serverNode], conf::port));
              client.SetAttribute ("MaxBytes", UintegerValue (get<3> (flow)));
              client.Install (clientNode).Start (Seconds (get<0> (flow)));
            }
          else
            {
              BulkSendHelper client (conf::socket,
                                     InetSocketAddress (addrs[serverNode], conf::port));
              client.SetAttribute ("MaxBytes", UintegerValue (get<3> (flow)));
              client.Install (clientNode).Start (Seconds (get<0> (flow)));
            }
        }
      flow = traffic.GetFlow ();
    }

  // traffic installation check
  LOG ("  Expected data rate = " << bandwidth * conf::load / 1e9 << "Gbps");
  LOG ("  Generated data rate = " << traffic.GetActualDataRate () / 1e9 << "Gbps");
  LOG ("  Expected avg flow size = " << traffic.GetAvgFlowSize () / 1e6 << "MB");
  LOG ("  Generated avg flow size = " << traffic.GetActualAvgFlowSize () / 1e6 << "MB");
  LOG ("  Total flow count = " << traffic.GetFlowCount ());
}

void
PrintProgress ()
{
  LOG ("  Progressed to " << Simulator::Now ().GetSeconds () << "s");
  Simulator::Schedule (Seconds (conf::interval), PrintProgress);
}

void
StartSimulation ()
{
  // install flow-monitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  if (conf::flowmon)
    {
      flowMonitor = flowHelper.InstallAll ();
    }

  // print progress
  if (conf::interval)
    {
      Simulator::Schedule (Seconds (conf::interval), PrintProgress);
    }

  // start the simulation
  Simulator::Stop (Seconds (conf::time));
  LOG ("\n- Start simulation...");
  auto start = system_clock::now ();
  Simulator::Run ();
  auto end = system_clock::now ();
  auto time = duration_cast<duration<double>> (end - start).count ();

  // output simulation statistics
  uint64_t eventCount = Simulator::GetEventCount ();
  if (conf::flowmon)
    {
      uint64_t dropped = 0, totalTx = 0, totalRx = 0, totalTxBytes = 0, flowCount = 0,
               finishedFlowCount = 0;
      double totalThroughput = 0;
      Time totalFct (0), totalFinishedFct (0), totalDelay (0);
      flowMonitor->CheckForLostPackets ();
      for (auto &p : flowMonitor->GetFlowStats ())
        {
          dropped = p.second.packetsDropped.size ();
          if ((p.second.timeLastRxPacket - p.second.timeFirstTxPacket).GetTimeStep () > 0 &&
              p.second.txPackets && p.second.rxPackets)
            {
              totalTx += p.second.txPackets;
              totalRx += p.second.rxPackets;
              totalTxBytes += p.second.txBytes;
              totalFct += p.second.timeLastRxPacket - p.second.timeFirstTxPacket;
              if (p.second.txPackets - p.second.rxPackets == p.second.packetsDropped.size ())
                {
                  totalFinishedFct += p.second.timeLastRxPacket - p.second.timeFirstTxPacket;
                  finishedFlowCount++;
                }
              totalDelay += p.second.delaySum;
              totalThroughput +=
                  (double) p.second.txBytes /
                  (p.second.timeLastRxPacket - p.second.timeFirstTxPacket).GetSeconds ();
              flowCount++;
            }
        }
      double avgFct = (double) totalFct.GetMicroSeconds () / flowCount;
      double avgFinishedFct = (double) totalFinishedFct.GetMicroSeconds () / finishedFlowCount;
      double avgDelay = (double) totalDelay.GetMicroSeconds () / totalRx;
      double avgThroughput = totalThroughput / flowCount / 1e9 * 8;
      LOG ("  Detected #flow = " << flowCount);
      LOG ("  Finished #flow = " << finishedFlowCount);
      LOG ("  Average FCT (all) = " << avgFct << "us");
      LOG ("  Average FCT (finished) = " << avgFinishedFct << "us");
      LOG ("  Average end to end delay = " << avgDelay << "us");
      LOG ("  Average flow throughput = " << avgThroughput << "Gbps");
      LOG ("  Network throughput = " << totalTxBytes / 1e9 * 8 / conf::time << "Gbps");
      LOG ("  Total Tx packets = " << totalTx);
      LOG ("  Total Rx packets = " << totalRx);
      LOG ("  Dropped packets = " << dropped);
    }
  Simulator::Destroy ();

#ifdef NS3_MPI
  uint64_t eventCounts[conf::system];
  MPI_Gather (&eventCount, 1, MPI_UNSIGNED_LONG_LONG, eventCounts, 1, MPI_UNSIGNED_LONG_LONG, 0,
              MpiInterface::GetCommunicator ());
  LOG ("\n- Done!");
  for (uint32_t i = 0; i < conf::system; i++)
    {
      LOG ("  Event count of LP " << i << " = " << eventCounts[i]);
    }
  LOG ("  Event count = " << accumulate (eventCounts, eventCounts + conf::system, 0ULL));
  LOG ("  Simulation time = " << time << "s\n");
  MpiInterface::Disable ();
#else
  LOG ("\n- Done!");
  LOG ("  Event count = " << eventCount);
  LOG ("  Simulation time = " << time << "s\n");
#endif
}
