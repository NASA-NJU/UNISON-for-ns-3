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
  uint32_t nPod = conf::n; // number of pods
  uint32_t nLevel = conf::level; // number of levels
  uint32_t nSwitch = 1; // number of switch of the same level in a pod
  uint32_t nHost = conf::n; // number of hosts in a pod
  for (uint32_t i = 0; i < nLevel - 2; i++)
    {
      nSwitch *= conf::n;
      nHost *= conf::n;
    }
  NodeContainer sw[nPod][nLevel], host[nPod];

  // create nodes
  for (uint32_t i = 0; i < nPod; i++)
    {
      for (uint32_t j = 0; j < nLevel; j++)
        {
          sw[i][j].Create (nSwitch, RANK (i % conf::system));
        }
    }
  for (uint32_t i = 0; i < nPod; i++)
    {
      host[i].Create (nHost, RANK (i % conf::system));
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

  // connect switches to hosts
  uint32_t step = 1;
  for (uint32_t i = 0; i < nLevel; i++)
    {
      for (uint32_t j = 0; j < nPod; j++)
        {
          for (uint32_t k = 0; k < nSwitch; k++)
            {
              uint32_t switchId = j * nSwitch + k;
              uint32_t beginHostId = switchId % step + (switchId - switchId % step) * conf::n;
              string subnet = "10." + to_string (i) + "." + to_string (switchId) + ".0";
              addr.SetBase (subnet.c_str (), "255.255.255.0");
              for (uint32_t m = 0; m < conf::n; m++)
                {
                  uint32_t currentHostId = beginHostId + m * step;
                  Ptr<Node> node = hosts[currentHostId];
                  NetDeviceContainer ndc = p2p.Install (node, sw[j][i].Get (k));
                  red.Install (ndc.Get (1));
                  if (i == 0)
                    {
                      addrs[node] = addr.Assign (ndc).GetAddress (0);
                    }
                  else
                    {
                      addr.Assign (ndc);
                    }
                  LOG ("  Switch (" << i << ", " << switchId << ") is connected to host "
                                    << currentHostId);
                }
            }
        }
      step *= conf::n;
    }

  InstallTraffic (hosts, addrs, hosts.size () / 2.0);
  StartSimulation ();

  return 0;
}
