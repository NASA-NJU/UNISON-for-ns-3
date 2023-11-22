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
#include "ns3/mtp-module.h"
#include "ns3/test.h"

#include <sstream>

using namespace ns3;

class MtpTestCase : public ExampleAsTestCase
{
  public:
    /**
     * \copydoc ns3::ExampleAsTestCase::ExampleAsTestCase
     *
     * \param [in] postCmd The post processing command
     */
    MtpTestCase(const std::string name,
                const std::string program,
                const std::string dataDir,
                const std::string args = "",
                const std::string postCmd = "",
                const bool shouldNotErr = true);

    /** Destructor */
    ~MtpTestCase() override
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

MtpTestCase::MtpTestCase(const std::string name,
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
MtpTestCase::GetCommandTemplate() const
{
    std::stringstream ss;
    ss << "%s " << m_args;
    return ss.str();
}

std::string
MtpTestCase::GetPostProcessingCommand() const
{
    std::string command(m_postCmd);
    return command;
}

class MtpTestSuite : public TestSuite
{
  public:
    /**
     * \copydoc MpiTestCase::MpiTestCase
     *
     * \param [in] duration Amount of time this test takes to execute
     *             (defaults to QUICK).
     */
    MtpTestSuite(const std::string name,
                 const std::string program,
                 const std::string dataDir,
                 const std::string args = "",
                 const std::string postCmd = "",
                 const TestDuration duration = QUICK,
                 const bool shouldNotErr = true)
        : TestSuite(name, EXAMPLE)
    {
        AddTestCase(new MtpTestCase(name, program, dataDir, args, postCmd, shouldNotErr), duration);
    }

}; // class MtpTestSuite

static MtpTestSuite g_mtpFatTree1("mtp-fat-tree",
                                  "fat-tree-mtp",
                                  NS_TEST_SOURCEDIR,
                                  "--bandwidth=100Mbps --thread=4 --flowmon=true",
                                  "| grep -v 'Simulation time'",
                                  TestCase::TestDuration::QUICK);

static MtpTestSuite g_mtpFatTree2("mtp-fat-tree-incast",
                                  "fat-tree-mtp",
                                  NS_TEST_SOURCEDIR,
                                  "--bandwidth=100Mbps --incast=1 --thread=4 --flowmon=true",
                                  "| grep -v 'Simulation time'",
                                  TestCase::TestDuration::QUICK);

static MtpTestSuite g_mtpTcpValidation1("mtp-tcp-validation-dctcp-10ms",
                                        "tcp-validation-mtp",
                                        NS_TEST_SOURCEDIR,
                                        "--firstTcpType=dctcp --linkRate=50Mbps --baseRtt=10ms "
                                        "--queueUseEcn=1 --stopTime=15s --validate=dctcp-10ms",
                                        "",
                                        TestCase::TestDuration::QUICK);

static MtpTestSuite g_mtpTcpValidation2("mtp-tcp-validation-dctcp-80ms",
                                        "tcp-validation-mtp",
                                        NS_TEST_SOURCEDIR,
                                        "--firstTcpType=dctcp --linkRate=50Mbps --baseRtt=80ms "
                                        "--queueUseEcn=1 --stopTime=40s --validate=dctcp-80ms",
                                        "",
                                        TestCase::TestDuration::QUICK);

static MtpTestSuite g_mtpTcpValidation3(
    "mtp-tcp-validation-cubic-50ms-no-ecn",
    "tcp-validation-mtp",
    NS_TEST_SOURCEDIR,
    "--firstTcpType=cubic --linkRate=50Mbps --baseRtt=50ms --queueUseEcn=0 --stopTime=20s "
    "--validate=cubic-50ms-no-ecn",
    "",
    TestCase::TestDuration::QUICK);

static MtpTestSuite g_mtpTcpValidation4("mtp-tcp-validation-cubic-50ms-ecn",
                                        "tcp-validation-mtp",
                                        NS_TEST_SOURCEDIR,
                                        "--firstTcpType=cubic --linkRate=50Mbps --baseRtt=50ms "
                                        "--queueUseEcn=1 --stopTime=20s --validate=cubic-50ms-ecn",
                                        "",
                                        TestCase::TestDuration::QUICK);
