/*
 * Copyright (c) 2023 State Key Laboratory for Novel Software Technology
 *
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
 * Author: Songyuan Bai <i@f5soft.site>
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mpi-module.h"
#include "ns3/network-module.h"
#include "ns3/nix-vector-routing-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <vector>

using namespace std;
using namespace chrono;
using namespace ns3;

#define LOCAL(r) ((r) == conf::rank)
#define LOG(content)                                                                               \
    {                                                                                              \
        if (conf::rank == 0)                                                                       \
            cout << content << endl;                                                               \
    }

// random variable distribution
class Distribution
{
  public:
    // load a distribution from a CDF file
    Distribution(const string filename)
    {
        ifstream fin;
        fin.open(filename);
        while (!fin.eof())
        {
            double x;
            double cdf;
            fin >> x >> cdf;
            m_cdf.emplace_back(x, cdf);
        }
        fin.close();

        m_rand = CreateObject<UniformRandomVariable>();
    }

    // expectation value of the distribution
    double Expectation() const
    {
        double ex = 0;
        for (uint32_t i = 1; i < m_cdf.size(); i++)
        {
            ex +=
                (m_cdf[i].first + m_cdf[i - 1].first) / 2 * (m_cdf[i].second - m_cdf[i - 1].second);
        }
        return ex;
    }

    // get a random value from the distribution
    double Sample()
    {
        double rand = m_rand->GetValue(0, 1);
        for (uint32_t i = 1; i < m_cdf.size(); i++)
        {
            if (rand <= m_cdf[i].second)
            {
                double slope =
                    (m_cdf[i].first - m_cdf[i - 1].first) / (m_cdf[i].second - m_cdf[i - 1].second);
                return m_cdf[i - 1].first + slope * (rand - m_cdf[i - 1].second);
            }
        }
        return m_cdf[m_cdf.size() - 1].second;
    }

  private:
    // the actual CDF function
    vector<pair<double, double>> m_cdf;
    // random variable stream
    Ptr<UniformRandomVariable> m_rand;
};

// traffic generator
class TrafficGenerator
{
  public:
    TrafficGenerator(const string cdfFile,
                     const uint32_t hostTotal,
                     const double dataRate,
                     const double incastRatio,
                     const vector<uint32_t> victims)
        : m_currentTime(0),
          m_incastRatio(incastRatio),
          m_hostTotal(hostTotal),
          m_victims(victims),
          m_flowCount(0),
          m_flowSizeTotal(0),
          m_distribution(cdfFile)
    {
        m_averageInterval = m_distribution.Expectation() * 8 / dataRate;
        m_uniformRand = CreateObject<UniformRandomVariable>();
        m_expRand = CreateObject<ExponentialRandomVariable>();
    }

    // get one flow with incremental time and random src, dst and size
    tuple<double, uint32_t, uint32_t, uint32_t> GetFlow()
    {
        uint32_t src;
        uint32_t dst;
        if (m_uniformRand->GetValue(0, 1) < m_incastRatio)
        {
            dst = m_victims[m_uniformRand->GetInteger(0, m_victims.size() - 1)];
        }
        else
        {
            dst = m_uniformRand->GetInteger(0, m_hostTotal - 1);
        }
        do
        {
            src = m_uniformRand->GetInteger(0, m_hostTotal - 1);
        } while (src == dst);

        uint32_t flowSize = max((uint32_t)round(m_distribution.Sample()), 1U);
        m_currentTime += m_expRand->GetValue(m_averageInterval, 0);
        m_flowSizeTotal += flowSize;
        m_flowCount++;

        return make_tuple(m_currentTime, src, dst, flowSize);
    }

    double GetActualDataRate() const
    {
        return m_flowSizeTotal / m_currentTime * 8;
    }

    double GetAvgFlowSize() const
    {
        return m_distribution.Expectation();
    }

    double GetActualAvgFlowSize() const
    {
        return m_flowSizeTotal / (double)m_flowCount;
    }

    uint32_t GetFlowCount() const
    {
        return m_flowCount;
    }

  private:
    double m_currentTime;
    double m_averageInterval;
    double m_incastRatio;
    uint32_t m_hostTotal;
    vector<uint32_t> m_victims;

    uint32_t m_flowCount;
    uint64_t m_flowSizeTotal;
    Distribution m_distribution;
    Ptr<UniformRandomVariable> m_uniformRand;
    Ptr<ExponentialRandomVariable> m_expRand;
};

namespace conf
{

// fat-tree scale
uint32_t k = 4;
uint32_t cluster = 0;

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
string cdf = "src/mtp/examples/web-search.txt";
double load = 0.3;
double incast = 0;
string victim = "0";

// simulation options
string seed = "";
bool flowmon = false;
double time = 1;
double interval = 0.1;

// mpi options
uint32_t system = 0;
uint32_t rank = 0;
bool nullmsg = false;
}; // namespace conf

void
Initialize(int argc, char* argv[])
{
    CommandLine cmd;

    // parse scale
    cmd.AddValue("k", "Number of pods in a fat-tree", conf::k);
    cmd.AddValue("cluster", "Number of clusters in a variant fat-tree", conf::cluster);

    // parse network options
    cmd.AddValue("mtu", "P2P link MTU", conf::mtu);
    cmd.AddValue("delay", "Link delay in nanoseconds", conf::delay);
    cmd.AddValue("bandwidth", "Link bandwidth", conf::bandwidth);
    cmd.AddValue("buffer", "Switch buffer size", conf::buffer);
    cmd.AddValue("ecn", "Use explicit congestion control", conf::ecn);
    cmd.AddValue("nix", "Enable nix-vector routing", conf::nix);
    cmd.AddValue("rip", "Enable RIP routing", conf::rip);
    cmd.AddValue("ecmp", "Use equal-cost multi-path routing", conf::ecmp);
    cmd.AddValue("flow", "Use per-flow ECMP routing", conf::flow);
    cmd.AddValue("port", "Port number of server applications", conf::port);
    cmd.AddValue("socket", "Socket protocol", conf::socket);
    cmd.AddValue("tcp", "TCP protocol", conf::tcp);
    cmd.AddValue("size", "Application packet size", conf::size);
    cmd.AddValue("cdf", "Traffic CDF file location", conf::cdf);
    cmd.AddValue("load", "Traffic load relative to bisection bandwidth", conf::load);
    cmd.AddValue("incast", "Incast traffic ratio", conf::incast);
    cmd.AddValue("victim", "Incast traffic victim list", conf::victim);

    // parse simulation options
    cmd.AddValue("seed", "The seed of the random number generator", conf::seed);
    cmd.AddValue("flowmon", "Use flow-monitor to record statistics", conf::flowmon);
    cmd.AddValue("time", "Simulation time in seconds", conf::time);
    cmd.AddValue("interval", "Simulation progreess print interval in seconds", conf::interval);

    // parse mtp/mpi options
    cmd.AddValue("system", "Number of logical processes in MTP manual partition", conf::system);
    cmd.AddValue("nullmsg", "Enable null message algorithm", conf::nullmsg);
    cmd.Parse(argc, argv);

    // link layer settings
    Config::SetDefault("ns3::PointToPointChannel::Delay", TimeValue(NanoSeconds(conf::delay)));
    Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue(conf::bandwidth));
    Config::SetDefault("ns3::PointToPointNetDevice::Mtu", UintegerValue(conf::mtu));

    // traffic control layer settings
    Config::SetDefault("ns3::RedQueueDisc::MeanPktSize", UintegerValue(conf::mtu));
    Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(conf::ecn));
    Config::SetDefault("ns3::RedQueueDisc::UseHardDrop", BooleanValue(false));
    Config::SetDefault("ns3::RedQueueDisc::LinkDelay", TimeValue(NanoSeconds(conf::delay)));
    Config::SetDefault("ns3::RedQueueDisc::LinkBandwidth", StringValue(conf::bandwidth));
    Config::SetDefault("ns3::RedQueueDisc::MaxSize", QueueSizeValue(QueueSize(conf::buffer)));
    Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(50));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(150));

    // network layer settings
    Config::SetDefault("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue(conf::ecmp));
    Config::SetDefault("ns3::Ipv4GlobalRouting::FlowEcmpRouting", BooleanValue(conf::flow));

    // transport layer settings
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(conf::tcp));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(conf::size));
    Config::SetDefault("ns3::TcpSocket::ConnTimeout",
                       TimeValue(conf::tcp == "ns3::TcpDctcp" ? MilliSeconds(10) : Seconds(3)));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1073725440));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1073725440));
    Config::SetDefault(
        "ns3::TcpSocketBase::MinRto",
        TimeValue(conf::tcp == "ns3::TcpDctcp" ? MilliSeconds(5) : MilliSeconds(200)));
    Config::SetDefault(
        "ns3::TcpSocketBase::ClockGranularity",
        TimeValue(conf::tcp == "ns3::TcpDctcp" ? MicroSeconds(100) : MilliSeconds(1)));
    Config::SetDefault("ns3::RttEstimator::InitialEstimation",
                       TimeValue(conf::tcp == "ns3::TcpDctcp" ? MicroSeconds(200) : Seconds(1)));

    // application layer settings
    Config::SetDefault("ns3::BulkSendApplication::SendSize", UintegerValue(UINT32_MAX));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue(conf::bandwidth));
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(conf::size));
    Config::SetDefault("ns3::OnOffApplication::OnTime",
                       StringValue("ns3::ConstantRandomVariable[Constant=1000]"));
    Config::SetDefault("ns3::OnOffApplication::OffTime",
                       StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    // simulation settings
    Time::SetResolution(Time::PS);
    RngSeedManager::SetSeed(Hash32(conf::seed));

    // initialize mpi
    if (conf::nullmsg)
    {
        GlobalValue::Bind("SimulatorImplementationType",
                          StringValue("ns3::NullMessageSimulatorImpl"));
    }
    else
    {
        GlobalValue::Bind("SimulatorImplementationType",
                          StringValue("ns3::DistributedSimulatorImpl"));
    }
    MpiInterface::Enable(&argc, &argv);
    conf::rank = MpiInterface::GetSystemId();
    conf::system = MpiInterface::GetSize();
}

void
SetupRouting()
{
    InternetStackHelper internet;
    if (conf::nix)
    {
        internet.SetRoutingHelper(Ipv4NixVectorHelper());
    }
    else if (conf::rip)
    {
        internet.SetRoutingHelper(RipHelper());
    }
    else
    {
        internet.SetRoutingHelper(Ipv4GlobalRoutingHelper());
    }
    internet.SetIpv6StackInstall(false);
    internet.InstallAll();
    LOG("\n- Setup the topology...");
}

void
InstallTraffic(map<uint32_t, Ptr<Node>>& hosts,
               map<Ptr<Node>, Ipv4Address>& addrs,
               double bisection)
{
    // output address for debugging
    LOG("\n- Calculating routes...");
    LOG("  Host  NodeId  System  Address");
    for (auto& p : hosts)
    {
        LOG("  " << left << setw(6) << p.first << setw(8) << p.second->GetId() << setw(8)
                 << p.second->GetSystemId() << addrs[p.second]);
    }
    if (!conf::nix)
    {
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    }

    // server applications
    PacketSinkHelper server(conf::socket, InetSocketAddress(Ipv4Address::GetAny(), conf::port));
    for (auto& p : hosts)
    {
        if (LOCAL(p.second->GetSystemId()))
        {
            server.Install(p.second).Start(Seconds(0));
        }
    }

    // calculate traffic
    LOG("\n- Generating traffic...");
    double bandwidth = bisection * DataRate(conf::bandwidth).GetBitRate() * 2;
    string victim;
    stringstream sin(conf::victim);
    vector<uint32_t> victims;
    while (getline(sin, victim, '-'))
    {
        victims.push_back(stoi(victim));
    }
    TrafficGenerator traffic(conf::cdf,
                             hosts.size(),
                             bandwidth * conf::load,
                             conf::incast,
                             victims);

    // install traffic (client applications)
    auto flow = traffic.GetFlow();
    while (get<0>(flow) < conf::time)
    {
        Ptr<Node> clientNode = hosts[get<1>(flow)];
        Ptr<Node> serverNode = hosts[get<2>(flow)];
        if (LOCAL(clientNode->GetSystemId()))
        {
            if (conf::socket != "ns3::TcpSocketFactory")
            {
                OnOffHelper client(conf::socket, InetSocketAddress(addrs[serverNode], conf::port));
                client.SetAttribute("MaxBytes", UintegerValue(get<3>(flow)));
                client.Install(clientNode).Start(Seconds(get<0>(flow)));
            }
            else
            {
                BulkSendHelper client(conf::socket,
                                      InetSocketAddress(addrs[serverNode], conf::port));
                client.SetAttribute("MaxBytes", UintegerValue(get<3>(flow)));
                client.Install(clientNode).Start(Seconds(get<0>(flow)));
            }
        }
        flow = traffic.GetFlow();
    }

    // traffic installation check
    LOG("  Expected data rate = " << bandwidth * conf::load / 1e9 << "Gbps");
    LOG("  Generated data rate = " << traffic.GetActualDataRate() / 1e9 << "Gbps");
    LOG("  Expected avg flow size = " << traffic.GetAvgFlowSize() / 1e6 << "MB");
    LOG("  Generated avg flow size = " << traffic.GetActualAvgFlowSize() / 1e6 << "MB");
    LOG("  Total flow count = " << traffic.GetFlowCount());
}

void
PrintProgress()
{
    LOG("  Progressed to " << Simulator::Now().GetSeconds() << "s");
    Simulator::Schedule(Seconds(conf::interval), PrintProgress);
}

void
StartSimulation()
{
    // install flow-monitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    if (conf::flowmon)
    {
        flowMonitor = flowHelper.InstallAll();
    }

    // print progress
    if (conf::interval)
    {
        Simulator::Schedule(Seconds(conf::interval), PrintProgress);
    }

    // start the simulation
    Simulator::Stop(Seconds(conf::time));
    LOG("\n- Start simulation...");
    auto start = system_clock::now();
    Simulator::Run();
    auto end = system_clock::now();
    auto time = duration_cast<duration<double>>(end - start).count();

    // output simulation statistics
    uint64_t eventCount = Simulator::GetEventCount();
    if (conf::flowmon)
    {
        uint64_t dropped = 0;
        uint64_t totalTx = 0;
        uint64_t totalRx = 0;
        uint64_t totalTxBytes = 0;
        uint64_t flowCount = 0;
        uint64_t finishedFlowCount = 0;
        double totalThroughput = 0;
        Time totalFct(0);
        Time totalFinishedFct(0);
        Time totalDelay(0);

        flowMonitor->CheckForLostPackets();
        for (auto& p : flowMonitor->GetFlowStats())
        {
            dropped = p.second.packetsDropped.size();
            if ((p.second.timeLastRxPacket - p.second.timeFirstTxPacket).GetTimeStep() > 0 &&
                p.second.txPackets && p.second.rxPackets)
            {
                totalTx += p.second.txPackets;
                totalRx += p.second.rxPackets;
                totalTxBytes += p.second.txBytes;
                totalFct += p.second.timeLastRxPacket - p.second.timeFirstTxPacket;
                if (p.second.txPackets - p.second.rxPackets == p.second.packetsDropped.size())
                {
                    totalFinishedFct += p.second.timeLastRxPacket - p.second.timeFirstTxPacket;
                    finishedFlowCount++;
                }
                totalDelay += p.second.delaySum;
                totalThroughput +=
                    (double)p.second.txBytes /
                    (p.second.timeLastRxPacket - p.second.timeFirstTxPacket).GetSeconds();
                flowCount++;
            }
        }

        double avgFct = (double)totalFct.GetMicroSeconds() / flowCount;
        double avgFinishedFct = (double)totalFinishedFct.GetMicroSeconds() / finishedFlowCount;
        double avgDelay = (double)totalDelay.GetMicroSeconds() / totalRx;
        double avgThroughput = totalThroughput / flowCount / 1e9 * 8;
        LOG("  Detected #flow = " << flowCount);
        LOG("  Finished #flow = " << finishedFlowCount);
        LOG("  Average FCT (all) = " << avgFct << "us");
        LOG("  Average FCT (finished) = " << avgFinishedFct << "us");
        LOG("  Average end to end delay = " << avgDelay << "us");
        LOG("  Average flow throughput = " << avgThroughput << "Gbps");
        LOG("  Network throughput = " << totalTxBytes / 1e9 * 8 / conf::time << "Gbps");
        LOG("  Total Tx packets = " << totalTx);
        LOG("  Total Rx packets = " << totalRx);
        LOG("  Dropped packets = " << dropped);
    }
    Simulator::Destroy();

    uint64_t eventCounts[conf::system];
    MPI_Gather(&eventCount,
               1,
               MPI_UNSIGNED_LONG_LONG,
               eventCounts,
               1,
               MPI_UNSIGNED_LONG_LONG,
               0,
               MpiInterface::GetCommunicator());
    LOG("\n- Done!");
    for (uint32_t i = 0; i < conf::system; i++)
    {
        LOG("  Event count of LP " << i << " = " << eventCounts[i]);
    }
    LOG("  Event count = " << accumulate(eventCounts, eventCounts + conf::system, 0ULL));
    LOG("  Simulation time = " << time << "s\n");
    MpiInterface::Disable();
}

int
main(int argc, char* argv[])
{
    Initialize(argc, argv);

    uint32_t hostId = 0;
    map<uint32_t, Ptr<Node>> hosts;
    map<Ptr<Node>, Ipv4Address> addrs;

    // calculate topo scales
    uint32_t nPod = conf::cluster ? conf::cluster : conf::k; // number of pods
    uint32_t nGroup = conf::k / 2;                           // number of group of core switches
    uint32_t nCore = conf::k / 2;                            // number of core switch in a group
    uint32_t nAgg = conf::k / 2;  // number of aggregation switch in a pod
    uint32_t nEdge = conf::k / 2; // number of edge switch in a pod
    uint32_t nHost = conf::k / 2; // number of hosts under a switch
    NodeContainer core[nGroup];
    NodeContainer agg[nPod];
    NodeContainer edge[nPod];
    NodeContainer host[nPod][nEdge];

    // create nodes
    for (uint32_t i = 0; i < nGroup; i++)
    {
        core[i].Create(nCore / 2, (2 * i) % conf::system);
        core[i].Create((nCore - 1) / 2 + 1, (2 * i + 1) % conf::system);
    }
    for (uint32_t i = 0; i < nPod; i++)
    {
        agg[i].Create(nAgg, i % conf::system);
    }
    for (uint32_t i = 0; i < nPod; i++)
    {
        edge[i].Create(nEdge, i % conf::system);
    }
    for (uint32_t i = 0; i < nPod; i++)
    {
        for (uint32_t j = 0; j < nEdge; j++)
        {
            host[i][j].Create(nHost, i % conf::system);
            for (uint32_t k = 0; k < nHost; k++)
            {
                hosts[hostId++] = host[i][j].Get(k);
            }
        }
    }

    SetupRouting();
    Ipv4AddressHelper addr;
    TrafficControlHelper red;
    PointToPointHelper p2p;
    red.SetRootQueueDisc("ns3::RedQueueDisc");

    // connect edge switches to hosts
    for (uint32_t i = 0; i < nPod; i++)
    {
        for (uint32_t j = 0; j < nEdge; j++)
        {
            string subnet = "10." + to_string(i) + "." + to_string(j) + ".0";
            addr.SetBase(subnet.c_str(), "255.255.255.0");
            for (uint32_t k = 0; k < nHost; k++)
            {
                Ptr<Node> node = host[i][j].Get(k);
                NetDeviceContainer ndc = p2p.Install(NodeContainer(node, edge[i].Get(j)));
                red.Install(ndc.Get(1));
                addrs[node] = addr.Assign(ndc).GetAddress(0);
            }
        }
    }

    // connect aggregate switches to edge switches
    for (uint32_t i = 0; i < nPod; i++)
    {
        for (uint32_t j = 0; j < nAgg; j++)
        {
            string subnet = "10." + to_string(i) + "." + to_string(j + nEdge) + ".0";
            addr.SetBase(subnet.c_str(), "255.255.255.0");
            for (uint32_t k = 0; k < nEdge; k++)
            {
                NetDeviceContainer ndc = p2p.Install(agg[i].Get(j), edge[i].Get(k));
                red.Install(ndc);
                addr.Assign(ndc);
            }
        }
    }

    // connect core switches to aggregate switches
    for (uint32_t i = 0; i < nGroup; i++)
    {
        for (uint32_t j = 0; j < nPod; j++)
        {
            string subnet = "10." + to_string(i + nPod) + "." + to_string(j) + ".0";
            addr.SetBase(subnet.c_str(), "255.255.255.0");
            for (uint32_t k = 0; k < nCore; k++)
            {
                NetDeviceContainer ndc = p2p.Install(core[i].Get(k), agg[j].Get(i));
                red.Install(ndc);
                addr.Assign(ndc);
            }
        }
    }

    InstallTraffic(hosts, addrs, nGroup * nCore * nPod / 2.0);
    StartSimulation();

    return 0;
}