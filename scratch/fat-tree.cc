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
      for (uint32_t j = 0; j < nPod; j++)
        {
          string subnet = "10." + to_string (i + nPod) + "." + to_string (j) + ".0";
          addr.SetBase (subnet.c_str (), "255.255.255.0");
          for (uint32_t k = 0; k < nCore; k++)
            {
              NetDeviceContainer ndc = p2p.Install (core[i].Get (k), agg[j].Get (i));
              red.Install (ndc);
              addr.Assign (ndc);
            }
        }
    }

  InstallTraffic (hosts, addrs, nGroup * nCore * nPod / 2.0);
  StartSimulation ();

  return 0;
}
