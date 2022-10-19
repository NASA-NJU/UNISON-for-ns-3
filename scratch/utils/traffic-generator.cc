/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/random-variable-stream.h"
#include "ns3/data-rate.h"

#include <fstream>
#include <vector>

using namespace std;
using namespace ns3;

// random variable distribution
class Distribution
{
public:
  // load a distribution from a CDF file
  Distribution (string filename = "scratch/cdf/web-search.txt")
  {
    ifstream fin;
    fin.open (filename);
    while (!fin.eof ())
      {
        double x, cdf;
        fin >> x >> cdf;
        m_cdf.push_back (std::make_pair (x, cdf));
      }
    fin.close ();

    m_rand = CreateObject<UniformRandomVariable> ();
  }

  // expectation value of the distribution
  double
  Expectation ()
  {
    double ex = 0;
    for (uint32_t i = 1; i < m_cdf.size (); i++)
      {
        ex += (m_cdf[i].first + m_cdf[i - 1].first) / 2 * (m_cdf[i].second - m_cdf[i - 1].second);
      }
    return ex;
  }

  // get a random value from the distribution
  double
  Sample ()
  {
    double rand = m_rand->GetValue (0, 1);
    for (uint32_t i = 1; i < m_cdf.size (); i++)
      {
        if (rand <= m_cdf[i].second)
          {
            double slope =
                (m_cdf[i].first - m_cdf[i - 1].first) / (m_cdf[i].second - m_cdf[i - 1].second);
            return m_cdf[i - 1].first + slope * (rand - m_cdf[i - 1].second);
          }
      }
    return m_cdf[m_cdf.size () - 1].second;
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
  TrafficGenerator (string cdfFile, uint32_t hostTotal, double dataRate, double incastRatio,
                    vector<uint32_t> victims)
  {
    m_distribution = Distribution (cdfFile);

    m_currentTime = 0;
    m_averageInterval = m_distribution.Expectation () * 8 / dataRate;
    m_incastRatio = incastRatio;
    m_hostTotal = hostTotal;
    m_victims = victims;

    m_flowCount = 0;
    m_flowSizeTotal = 0;
    m_uniformRand = CreateObject<UniformRandomVariable> ();
    m_expRand = CreateObject<ExponentialRandomVariable> ();
  }

  // get one flow with incremental time and random src, dst and size
  tuple<double, uint32_t, uint32_t, uint32_t>
  GetFlow ()
  {
    uint32_t src, dst;
    if (m_uniformRand->GetValue (0, 1) < m_incastRatio)
      {
        dst = m_victims[m_uniformRand->GetInteger (0, m_victims.size () - 1)];
      }
    else
      {
        dst = m_uniformRand->GetInteger (0, m_hostTotal - 1);
      }
    do
      {
        src = m_uniformRand->GetInteger (0, m_hostTotal - 1);
    } while (src == dst);

    uint32_t flowSize = max ((uint32_t) round (m_distribution.Sample ()), 1U);
    m_currentTime += m_expRand->GetValue (m_averageInterval, 0);
    m_flowSizeTotal += flowSize;
    m_flowCount++;

    return make_tuple (m_currentTime, src, dst, flowSize);
  }

  double
  GetActualDataRate ()
  {
    return m_flowSizeTotal / m_currentTime * 8;
  }

  double
  GetAvgFlowSize ()
  {
    return m_distribution.Expectation ();
  }

  double
  GetActualAvgFlowSize ()
  {
    return m_flowSizeTotal / (double) m_flowCount;
  }

  uint32_t
  GetFlowCount ()
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
