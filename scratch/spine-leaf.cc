/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "utils/common.cc"

int
main (int argc, char *argv[])
{
  Initialize (argc, argv);

  uint32_t hostId = 0;
  map<uint32_t, Ptr<Node>> hosts;
  map<Ptr<Node>, Ipv4Address> addrs;

  // calculate topo scales
  uint32_t nSpine =
      conf::system ? (conf::spine - 1) / conf::system + 1 : conf::spine; // number of spines in a LP
  uint32_t nLeaf =
      conf::system ? (conf::leaf - 1) / conf::system + 1 : conf::leaf; // number of leaves in a LP
  uint32_t nHost = conf::host; // number of hosts in a leaf
  NodeContainer spine, leaf, host[conf::leaf];

  // create nodes
  for (uint32_t i = 0; i < conf::spine; i++)
    {
      Ptr<Node> node = CreateObject<Node> (RANK (i / nSpine));
      spine.Add (node);
    }
  for (uint32_t i = 0; i < conf::leaf; i++)
    {
      Ptr<Node> node = CreateObject<Node> (RANK (i / nLeaf));
      leaf.Add (node);
    }
  for (uint32_t i = 0; i < conf::leaf; i++)
    {
      host[i].Create (nHost, RANK (i / nLeaf));
      for (uint32_t j = 0; j < nHost; j++)
        {
          hosts[hostId++] = host[i].Get (j);
        }
    }

  SetupRouting ();
  Ipv4AddressHelper addr;
  TrafficControlHelper red;
  PointToPointHelper p2p;
  red.SetRootQueueDisc ("ns3::RedQueueDisc");

  // connect hosts to leaves
  for (uint32_t i = 0; i < conf::leaf; i++)
    {
      string subnet = "10.254." + to_string (i) + ".0";
      addr.SetBase (subnet.c_str (), "255.255.255.0");
      for (uint32_t j = 0; j < nHost; j++)
        {
          Ptr<Node> node = host[i].Get (j);
          NetDeviceContainer ndc = p2p.Install (node, leaf.Get (i));
          red.Install (ndc.Get (1));
          addrs[node] = addr.Assign (ndc).GetAddress (0);
        }
    }

  // connect leaves to spines
  for (uint32_t i = 0; i < conf::spine; i++)
    {
      for (uint32_t j = 0; j < conf::leaf; j++)
        {
          NetDeviceContainer ndc = p2p.Install (spine.Get (i), leaf.Get (j));
          red.Install (ndc);
          string subnet = "10." + to_string (i) + "." + to_string (j) + ".0";
          addr.SetBase (subnet.c_str (), "255.255.255.0");
          addr.Assign (ndc);
        }
    }

  InstallTraffic (hosts, addrs, conf::spine * conf::leaf / 2.0);
  StartSimulation ();

  return 0;
}
