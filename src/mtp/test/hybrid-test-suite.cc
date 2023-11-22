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

#include "ns3/example-as-test.h"
#include "ns3/mpi-module.h"
#include "ns3/mtp-module.h"
#include "ns3/test.h"

#include <sstream>

using namespace ns3;

class HybridTestCase : public ExampleAsTestCase
{
  public:
    /**
     * \copydoc ns3::ExampleAsTestCase::ExampleAsTestCase
     *
     * \param [in] postCmd The post processing command
     */
    HybridTestCase(const std::string name,
                   const std::string program,
                   const std::string dataDir,
                   const std::string args = "",
                   const std::string postCmd = "",
                   const bool shouldNotErr = true);

    /** Destructor */
    ~HybridTestCase() override
    {
    }

    /**
     * Produce the `--command-template` argument
     *
     * \returns The `--command-template` string.
     */
    std::string GetCommandTemplate() const override;

    /**
     * Remove time statistics
     *
     * \returns The post processing command
     */
    std::string GetPostProcessingCommand() const override;

  private:
    /** The post processing command. */
    std::string m_postCmd;
};

HybridTestCase::HybridTestCase(const std::string name,
                               const std::string program,
                               const std::string dataDir,
                               const std::string args /* = "" */,
                               const std::string postCmd /* = "" */,
                               const bool shouldNotErr /* = true */)
    : ExampleAsTestCase(name, program, dataDir, args, shouldNotErr),
      m_postCmd(postCmd)
{
}

std::string
HybridTestCase::GetCommandTemplate() const
{
    std::stringstream ss;
    ss << "mpirun -np 2 %s " << m_args;
    return ss.str();
}

std::string
HybridTestCase::GetPostProcessingCommand() const
{
    std::string command(m_postCmd);
    return command;
}

class HybridTestSuite : public TestSuite
{
  public:
    /**
     * \copydoc MpiTestCase::MpiTestCase
     *
     * \param [in] duration Amount of time this test takes to execute
     *             (defaults to QUICK).
     */
    HybridTestSuite(const std::string name,
                    const std::string program,
                    const std::string dataDir,
                    const std::string args = "",
                    const std::string postCmd = "",
                    const TestDuration duration = QUICK,
                    const bool shouldNotErr = true)
        : TestSuite(name, EXAMPLE)
    {
        AddTestCase(new HybridTestCase(name, program, dataDir, args, postCmd, shouldNotErr),
                    duration);
    }

}; // class HybridTestSuite

static HybridTestSuite g_hybridFatTree1("hybrid-fat-tree",
                                        "fat-tree-hybrid",
                                        NS_TEST_SOURCEDIR,
                                        "--bandwidth=100Mbps --thread=2",
                                        "| grep -v 'Simulation time' | grep -v 'Event count'",
                                        TestCase::TestDuration::QUICK);

static HybridTestSuite g_hybridFatTree2("hybrid-fat-tree-incast",
                                        "fat-tree-hybrid",
                                        NS_TEST_SOURCEDIR,
                                        "--bandwidth=100Mbps --incast=1 --thread=2",
                                        "| grep -v 'Simulation time' | grep -v 'Event count'",
                                        TestCase::TestDuration::QUICK);

static HybridTestSuite g_hybridSimple("hybrid-simple",
                                      "simple-hybrid",
                                      NS_TEST_SOURCEDIR,
                                      ""
                                      "",
                                      "",
                                      TestCase::TestDuration::QUICK);
