/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "utils/common.cc"

// (packed-id, src, dst, sport, dport, seq, ack, flags, size) -> (send-time, pkt-size, src-host, path, recv-time)
typedef tuple<uint64_t, Ipv4Address, Ipv4Address, uint16_t, uint16_t, SequenceNumber32,
              SequenceNumber32, uint8_t, uint32_t>
    PacketKey;
map<PacketKey, tuple<Time, uint32_t, uint32_t, vector<pair<uint32_t, uint32_t>>, Time>> packets;

bool
CheckPacket (const Ipv4Header &hd)
{
  if (hd.GetFragmentOffset () > 0)
    {
      return false;
    }
  if (hd.GetProtocol () != 6)
    {
      return false;
    }
  return true;
}

PacketKey
GetPacketKey (const Ipv4Header &hd, Ptr<const Packet> p)
{
  TcpHeader tcpHdr;
  p->PeekHeader (tcpHdr);
  uint64_t packetId = p->GetUid ();
  Ipv4Address src = hd.GetSource ();
  Ipv4Address dst = hd.GetDestination ();
  uint16_t sport = tcpHdr.GetSourcePort ();
  uint16_t dport = tcpHdr.GetDestinationPort ();
  SequenceNumber32 seq = tcpHdr.GetSequenceNumber ();
  SequenceNumber32 ack = tcpHdr.GetAckNumber ();
  uint8_t flags = tcpHdr.GetFlags ();
  uint32_t size = p->GetSize ();
  return make_tuple (packetId, src, dst, sport, dport, seq, ack, flags, size);
}

void
PacketSend (const Ipv4Header &hd, Ptr<const Packet> p, uint32_t oif)
{
  if (CheckPacket (hd))
    {
      PacketKey key = GetPacketKey (hd, p);
#ifdef NS3_MTP
      MtpInterface::CriticalSection cs;
#endif
      packets[key] = make_tuple (Simulator::Now (), p->GetSize (), Simulator::GetContext () - 20,
                                 vector<pair<uint32_t, uint32_t>> (), Time ());
    }
}

void
PacketForward (const Ipv4Header &hd, Ptr<const Packet> p, uint32_t oif)
{
  if (CheckPacket (hd))
    {
      PacketKey key = GetPacketKey (hd, p);
#ifdef NS3_MTP
      MtpInterface::CriticalSection cs;
#endif
      if (packets.find (key) != packets.end ())
        {
          get<3> (packets[key]).push_back (make_pair (Simulator::GetContext (), oif - 1));
        }
    }
}

void
PacketReceived (const Ipv4Header &hd, Ptr<const Packet> p, uint32_t oif)
{
  if (CheckPacket (hd))
    {
      PacketKey key = GetPacketKey (hd, p);
#ifdef NS3_MTP
      MtpInterface::CriticalSection cs;
#endif
      if (packets.find (key) != packets.end ())
        {
          get<4> (packets[key]) = Simulator::Now ();
        }
    }
}

void
PacketTraceToCSV (string filename)
{
  ofstream fout;
  fout.open (filename);
  fout << "index,timestamp (sec),pkt len (byte),priority,src_pc,path,etime\n";
  uint32_t index = 0;
  for (auto &p : packets)
    {
      if (get<4> (p.second) != Time ())
        {
          string path = "";
          for (auto &q : get<3> (p.second))
            {
              path += to_string (q.first) + "_" + to_string (q.second) + "-";
            }
          path = path.substr (0, path.size () - 1);
          fout << index++ << ',' << setprecision (9) << get<0> (p.second).GetSeconds () << ','
               << get<1> (p.second) << ",0," << get<2> (p.second) << ',' << path << ','
               << setprecision (9) << get<4> (p.second).GetSeconds () << '\n';
        }
    }
  fout.close ();
}

int
main (int argc, char *argv[])
{
  Initialize (argc, argv);

  uint32_t hostId = 0;
  map<uint32_t, Ptr<Node>> hosts;
  map<Ptr<Node>, Ipv4Address> addrs;

  // calculate topo scales
  uint32_t nPod = conf::cluster ? conf::cluster : conf::k; // number of pods
  uint32_t nGroup = conf::k / 2; // number of group of core switches
  uint32_t nCore = conf::k / 2; // number of core switch in a group
  uint32_t nAgg = conf::k / 2; // number of aggregation switch in a pod
  uint32_t nEdge = conf::k / 2; // number of edge switch in a pod
  uint32_t nHost = conf::k / 2; // number of hosts under a switch
  NodeContainer core[nGroup], agg[nPod], edge[nPod], host[nPod][nEdge];

  // create nodes
  for (uint32_t i = 0; i < nGroup; i++)
    {
      core[i].Create (nCore / 2, RANK ((2 * i) % conf::system));
      core[i].Create ((nCore - 1) / 2 + 1, RANK ((2 * i + 1) % conf::system));
    }
  for (uint32_t i = 0; i < nPod; i++)
    {
      agg[i].Create (nAgg, RANK (i % conf::system));
    }
  for (uint32_t i = 0; i < nPod; i++)
    {
      edge[i].Create (nEdge, RANK (i % conf::system));
    }
  for (uint32_t i = 0; i < nPod; i++)
    {
      for (uint32_t j = 0; j < nEdge; j++)
        {
          host[i][j].Create (nHost, RANK (i % conf::system));
          for (uint32_t k = 0; k < nHost; k++)
            {
              hosts[hostId++] = host[i][j].Get (k);
            }
        }
    }

  SetupRouting ();
  Ipv4AddressHelper addr;
  TrafficControlHelper red;
  PointToPointHelper p2p;
  red.SetRootQueueDisc ("ns3::RedQueueDisc");

  // connect edge switches to hosts
  for (uint32_t i = 0; i < nPod; i++)
    {
      for (uint32_t j = 0; j < nEdge; j++)
        {
          string subnet = "10." + to_string (i) + "." + to_string (j) + ".0";
          addr.SetBase (subnet.c_str (), "255.255.255.0");
          for (uint32_t k = 0; k < nHost; k++)
            {
              Ptr<Node> node = host[i][j].Get (k);
              NetDeviceContainer ndc = p2p.Install (NodeContainer (node, edge[i].Get (j)));
              red.Install (ndc.Get (1));

              addrs[node] = addr.Assign (ndc).GetAddress (0);
            }
        }
    }

  // connect aggregate switches to edge switches
  for (uint32_t i = 0; i < nPod; i++)
    {
      for (uint32_t j = 0; j < nAgg; j++)
        {
          string subnet = "10." + to_string (i) + "." + to_string (j + nEdge) + ".0";
          addr.SetBase (subnet.c_str (), "255.255.255.0");
          for (uint32_t k = 0; k < nEdge; k++)
            {
              NetDeviceContainer ndc = p2p.Install (agg[i].Get (j), edge[i].Get (k));
              red.Install (ndc);
              addr.Assign (ndc);
            }
        }
    }

  // connect core switches to aggregate switches
  for (uint32_t i = 0; i < nGroup; i++)
    {
      for (uint32_t j = 0; j < nCore; j++)
        {
          string subnet = "10." + to_string (i + nPod) + "." + to_string (j) + ".0";
          addr.SetBase (subnet.c_str (), "255.255.255.0");
          for (uint32_t k = 0; k < nPod; k++)
            {
              NetDeviceContainer ndc = p2p.Install (core[i].Get (j), agg[k].Get (i));
              red.Install (ndc);
              addr.Assign (ndc);
            }
        }
    }

  // set traces to record every packets' path
  for (uint32_t i = 0; i < nGroup; i++)
    {
      for (uint32_t j = 0; j < nCore; j++)
        {
          LOG ("  Core switch (" << i << ", " << j << ") id = " << core[i].Get (j)->GetId ());
          core[i].Get (j)->GetObject<Ipv4L3Protocol> ()->TraceConnectWithoutContext (
              "UnicastForward", MakeCallback (PacketForward));
        }
    }
  for (uint32_t i = 0; i < nPod; i++)
    {
      for (uint32_t j = 0; j < nAgg; j++)
        {
          LOG ("  Agg switch (" << i << ", " << j << ") id = " << agg[i].Get (j)->GetId ());
          agg[i].Get (j)->GetObject<Ipv4L3Protocol> ()->TraceConnectWithoutContext (
              "UnicastForward", MakeCallback (PacketForward));
        }
    }
  for (uint32_t i = 0; i < nPod; i++)
    {
      for (uint32_t j = 0; j < nEdge; j++)
        {
          LOG ("  Edge switch (" << i << ", " << j << ") id = " << edge[i].Get (j)->GetId ());
          edge[i].Get (j)->GetObject<Ipv4L3Protocol> ()->TraceConnectWithoutContext (
              "UnicastForward", MakeCallback (PacketForward));
        }
    }
  for (uint32_t i = 0; i < nPod; i++)
    {
      for (uint32_t j = 0; j < nEdge; j++)
        {
          for (uint32_t k = 0; k < nHost; k++)
            {
              LOG ("  Host (" << i << ", " << j << ", " << k
                              << ") id = " << host[i][j].Get (k)->GetId ());
              host[i][j].Get (k)->GetObject<Ipv4L3Protocol> ()->TraceConnectWithoutContext (
                  "SendOutgoing", MakeCallback (PacketSend));
              host[i][j].Get (k)->GetObject<Ipv4L3Protocol> ()->TraceConnectWithoutContext (
                  "LocalDeliver", MakeCallback (PacketReceived));
            }
        }
    }

  InstallTraffic (hosts, addrs, nGroup * nCore * nPod / 2.0);
  StartSimulation ();
  PacketTraceToCSV ("results/trace-" + to_string (conf::k) + "-" + to_string(conf::cluster) + ".csv");

  return 0;
}
