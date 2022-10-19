/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <regex>
#include "utils/common.cc"

int
main (int argc, char **argv)
{
  Initialize (argc, argv);

  map<uint32_t, Ptr<Node>> hosts;
  map<Ptr<Node>, Ipv4Address> addrs;

  // create nodes
  regex nodePattern (" *<node id=\"(\\d+)\"> *");
  ifstream fin (conf::topo);
  string line;
  while (getline (fin, line))
    {
      smatch matches;
      if (regex_match (line, matches, nodePattern))
        {
          Ptr<Node> node = CreateObject<Node> ();
          hosts[stoi (matches[1].str ())] = node;
          LOG ("  Create node " << matches[1]);
        }
    }

  SetupRouting ();
  Ipv4AddressHelper addr;
  TrafficControlHelper red;
  PointToPointHelper p2p;
  addr.SetBase ("10.0.0.0", "255.255.255.0");

  // create edges
  fin.clear ();
  fin.seekg (ios::beg);
  regex edgePattern (" *<edge source=\"(\\d+)\" target=\"(\\d+)\"> *");
  while (getline (fin, line))
    {
      smatch matches;
      if (regex_match (line, matches, edgePattern))
        {
          uint32_t src = stoi (matches[1].str ());
          uint32_t dst = stoi (matches[2].str ());
          NetDeviceContainer ndc = p2p.Install (hosts[src], hosts[dst]);
          red.Install (ndc);
          addrs[hosts[src]] = addr.Assign (ndc).GetAddress (0);
          addrs[hosts[dst]] = addr.Assign (ndc).GetAddress (1);
          addr.NewNetwork ();
          LOG ("  Create link (" << src << ", " << dst << ")");
        }
    }

  InstallTraffic (hosts, addrs, hosts.size () / 2);
  StartSimulation ();

  return 0;
}
