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
  uint32_t hostNum = conf::system ? (conf::row * conf::col - 1) / conf::system + 1
                                  : conf::row * conf::col; // number of host in a LP
  uint32_t hostTotal = conf::row * conf::col;

  // create nodes
  for (uint32_t i = 0; i < hostTotal; i++)
    {
      hosts[hostId++] = CreateObject<Node> (RANK (i / hostNum));
    }

  SetupRouting ();
  Ipv4AddressHelper addr;
  TrafficControlHelper red;
  PointToPointHelper p2p;
  red.SetRootQueueDisc ("ns3::RedQueueDisc");

  // connect hosts
  for (uint32_t i = 0; i < conf::row; i++)
    {
      for (uint32_t j = 0; j < conf::col; j++)
        {
          uint32_t currentId = i * conf::col + j;
          uint32_t rightId = (j == conf::col - 1) ? currentId - j : currentId + 1;
          uint32_t downId = (i == conf::row - 1) ? j : currentId + conf::col;
          Ptr<Node> current = hosts[currentId];
          NetDeviceContainer ndcRow = p2p.Install (current, hosts[rightId]);
          NetDeviceContainer ndcCol = p2p.Install (current, hosts[downId]);
          red.Install (ndcRow);
          red.Install (ndcCol);
          string subnet = "10." + to_string (i) + "." + to_string (j) + ".0";
          addr.SetBase (subnet.c_str (), "255.255.255.0");
          addrs[current] = addr.Assign (ndcRow).GetAddress (0);
          addr.Assign (ndcCol);
        }
    }

  InstallTraffic (hosts, addrs, 2 * min (conf::row, conf::col));
  StartSimulation ();

  return 0;
}
