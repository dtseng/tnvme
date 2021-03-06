/*
 * Copyright (c) 2011, Intel Corporation.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include "tnvme.h"
#include "tnvmeHelpers.h"
#include "tnvmeParsers.h"
#include "version.h"
#include "globals.h"
#include "Utils/kernelAPI.h"
#include "Utils/fileSystem.h"


// ------------------------------EDIT HERE---------------------------------
#include "GrpPciRegisters/grpPciRegisters.h"
#include "GrpCtrlRegisters/grpCtrlRegisters.h"
#include "GrpBasicInit/grpBasicInit.h"
#include "GrpResets/grpResets.h"
#include "GrpQueues/grpQueues.h"
#include "GrpNVMReadCmd/grpNVMReadCmd.h"
#include "GrpNVMWriteCmd/grpNVMWriteCmd.h"
#include "GrpNVMWriteReadCombo/grpNVMWriteReadCombo.h"
#include "GrpNVMFlushCmd/grpNVMFlushCmd.h"
#include "GrpInterrupts/grpInterrupts.h"
#include "GrpGeneralCmds/grpGeneralCmds.h"
#include "GrpNVMWriteUncorrectCmd/grpNVMWriteUncorrectCmd.h"
#include "GrpNVMDatasetMgmtCmd/grpNVMDatasetMgmtCmd.h"
#include "GrpNVMCompareCmd/grpNVMCompareCmd.h"
#include "GrpAdminDeleteIOCQCmd/grpAdminDeleteIOCQCmd.h"
#include "GrpAdminDeleteIOSQCmd/grpAdminDeleteIOSQCmd.h"

void
InstantiateGroups(vector<Group *> &groups, struct CmdLine &cl)
{
    // Appending new groups is the most favorable action here
    groups.push_back(new GrpPciRegisters::GrpPciRegisters(groups.size(), cl.rev, cl.errRegs, gDutFd));
    groups.push_back(new GrpCtrlRegisters::GrpCtrlRegisters(groups.size(), cl.rev, cl.errRegs, gDutFd));
    groups.push_back(new GrpBasicInit::GrpBasicInit(groups.size(), cl.rev, cl.errRegs, gDutFd));
    groups.push_back(new GrpResets::GrpResets(groups.size(), cl.rev, cl.errRegs, gDutFd));
    groups.push_back(new GrpGeneralCmds::GrpGeneralCmds(groups.size(), cl.rev, cl.errRegs, gDutFd));
    groups.push_back(new GrpQueues::GrpQueues(groups.size(), cl.rev, cl.errRegs, gDutFd));
    groups.push_back(new GrpNVMReadCmd::GrpNVMReadCmd(groups.size(), cl.rev, cl.errRegs, gDutFd));
    groups.push_back(new GrpNVMWriteCmd::GrpNVMWriteCmd(groups.size(), cl.rev, cl.errRegs, gDutFd));
    groups.push_back(new GrpNVMWriteReadCombo::GrpNVMWriteReadCombo(groups.size(), cl.rev, cl.errRegs, gDutFd));
    groups.push_back(new GrpNVMFlushCmd::GrpNVMFlushCmd(groups.size(), cl.rev, cl.errRegs, gDutFd));
    groups.push_back(new GrpInterrupts::GrpInterrupts(groups.size(), cl.rev, cl.errRegs, gDutFd));
    groups.push_back(new GrpNVMWriteUncorrectCmd::GrpNVMWriteUncorrectCmd(groups.size(), cl.rev, cl.errRegs, gDutFd));
    groups.push_back(new GrpNVMDatasetMgmtCmd::GrpNVMDatasetMgmtCmd(groups.size(), cl.rev, cl.errRegs, gDutFd));
    groups.push_back(new GrpNVMCompareCmd::GrpNVMCompareCmd(groups.size(), cl.rev, cl.errRegs, gDutFd));
    groups.push_back(new GrpAdminDeleteIOCQCmd::GrpAdminDeleteIOCQCmd(groups.size(), cl.rev, cl.errRegs, gDutFd));
    groups.push_back(new GrpAdminDeleteIOSQCmd::GrpAdminDeleteIOSQCmd(groups.size(), cl.rev, cl.errRegs, gDutFd));
}
// ------------------------------EDIT HERE---------------------------------


#define BASE_DUMP_DIR           "./Dump"
#define NO_DEVICES              "no devices found"
#define INFORM_GRPNUM           0


void Usage(void);
void DestroySingletons();
bool ExecuteTests(struct CmdLine &cl, vector<Group *> &groups);
bool BuildSingletons(struct CmdLine &cl);
void DestroyTestFoundation(vector<Group *> &groups);
bool BuildTestFoundation(vector<Group *> &groups, struct CmdLine &cl);
void ReportTestResults(size_t numIters, int numPass, int numFail, int numSkip);


void
Usage(void) {
    //80->  xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    printf("%s (revision %d.%d)\n", APPNAME, VER_MAJOR, VER_MINOR);
    printf("  -h(--help)                          Display this help\n");
    printf("  -v(--rev) <spec>                    All options are forced to target spec'd\n");
    printf("                                      NVME revision {1.0b}; dflt=1.0b\n");
    printf("  -s(--summary)                       Summarize all groups and tests\n");
    printf("  -a(--detail) [<grp> | <grp>:<test>] Detailed group and test description for:\n");
    printf("                                      {all | spec'd_group | test_within_group}\n");
    printf("  -t(--test) [<grp> | <grp>:<test>]   Execute tests for:\n");
    printf("                                      {all | spec'd_group | test_within_group}\n");
    printf("  -l(--list)                          List all devices available for test\n");
    printf("  -d(--device) <name>                 Device to open for testing: /dev/node\n");
    printf("                                      dflt=(1st device listed in --list)\n");
    printf("  -z(--reset)                         Ctrl'r level reset via CC.EN\n");
    printf("  -p(--loop) <count>                  Loop test execution <count> times; dflt=1\n");
    printf("  -k(--skiptest) <filename>           A file contains a list of tests to skip\n");
    printf("  -u(--dump) <dirname>                Pass the base dump directory path.\n");
    printf("                                      dflt=\"%s\"\n", BASE_DUMP_DIR);
    printf("  -i(--ignore)                        Ignore detected errors; An error causes\n");
    printf("                                      the next test within the next group to\n");
    printf("                                      execute, not next test within same group.\n");
    printf("  -e(--error) <STS:PXDS:AERUCES:CSTS> Set reg bitmask for bits indicating error\n");
    printf("                                      state after each test completes.\n");
    printf("                                      Value=0 indicates ignore all errors.\n");
    printf("                                      Require base 16 values.\n");
    printf("  -g(--golden) <filename>             A file contains the golden identify data\n");
    printf("                                      to which the DUT's reported identify data\n");
    printf("                                      is compared; Must be only option.\n");
    printf("                      --- Advanced/Debug Options Follow ---\n");
    printf("  -q(--queues) <ncqr:nsqr>            Write <ncqr> and <nsqr> as values by the\n");
    printf("                                      Set Features, ID=7. Must be only option.\n");
    printf("                                      Option missing indicates tnvme to read\n");
    printf("                                      and learn a previous set value.\n");
    printf("                                      until next power cycle. Requires base 16\n");
    printf("  -f(--format) <filename>             A file contains admin cmd set-format NVM\n");
    printf("                                      cmd instructions to send to device\n");
    printf("                                      Must be only option. Ex: format.xml file\n");
    printf("  -r(--rmmap) <space:off:size:acc>    Read memmap'd I/O registers from\n");
    printf("                                      <space>={PCI | BAR01} at <off> bytes\n");
    printf("                                      from start of space for <size> bytes\n");
    printf("                                      access width <acc>={l | w | b} type\n");
    printf("                                      <off:size:acc> require base 16 values\n");
    printf("  -w(--wmmap) <space:off:siz:val:acc> Write <val> data to memmap'd I/O reg from\n");
    printf("                                      <space>={PCI | BAR01} at <off> bytes\n");
    printf("                                      from start of space for <siz> bytes\n");
    printf("                                      access width <acc>={l | w | b} type\n");
    printf("                                      (Require: <size> < 8)\n");
    printf("                                      <offset:size> requires base 16 values\n");
}


int
main(int argc, char *argv[])
{
    int c;
    int idx = 0;
    int exitCode = 0;   // assume success
    char *endptr;
    long tmp;
    string work;
    vector<Group *> groups;
    vector<string> devices;
    struct CmdLine cmdLine;
    struct dirent *dirEntry;
    bool deviceFound = false;
    bool accessingHdw = true;
    uint64_t regVal = 0;
    const char *short_opt = "hslzia::t::v:p:d:k:f:r:w:q:e:u:g:";
    static struct option long_opt[] = {
        // {name,           has_arg,            flag,   val}
        {   "help",         no_argument,        NULL,   'h'},
        {   "summary",      no_argument,        NULL,   's'},
        {   "rev",          required_argument,  NULL,   'v'},
        {   "detail",       optional_argument,  NULL,   'a'},
        {   "test",         optional_argument,  NULL,   't'},
        {   "device",       required_argument,  NULL,   'd'},
        {   "rmmap" ,       required_argument,  NULL,   'r'},
        {   "wmmap" ,       required_argument,  NULL,   'w'},
        {   "reset",        no_argument,        NULL,   'z'},
        {   "ignore",       no_argument,        NULL,   'i'},
        {   "loop",         required_argument,  NULL,   'p'},
        {   "skiptest",     required_argument,  NULL,   'k'},
        {   "format",       required_argument,  NULL,   'f'},
        {   "list",         no_argument,        NULL,   'l'},
        {   "queues",       required_argument,  NULL,   'q'},
        {   "error",        required_argument,  NULL,   'e'},
        {   "dump",         required_argument,  NULL,   'u'},
        {   "golden",       required_argument,  NULL,   'g'},
        {   NULL,           no_argument,        NULL,    0}
    };

    // defaults if not spec'd on cmd line
    cmdLine.rev = SPECREV_10b;
    cmdLine.detail.req = false;
    cmdLine.test.req = false;
    cmdLine.reset = false;
    cmdLine.summary = false;
    cmdLine.ignore = false;
    cmdLine.loop = 1;
    cmdLine.device = NO_DEVICES;
    cmdLine.rmmap.req = false;
    cmdLine.wmmap.req = false;
    cmdLine.numQueues.req = false;
    cmdLine.numQueues.ncqr = 0;
    cmdLine.numQueues.nsqr = 0;
    cmdLine.format.req = false;
    cmdLine.format.cmds.empty();
    cmdLine.golden.req = false;
    cmdLine.golden.cmds.empty();
    cmdLine.errRegs.sts = (STS_SSE | STS_STA | STS_RMA | STS_RTA);
    cmdLine.errRegs.pxds = (PXDS_TP | PXDS_FED);
    cmdLine.errRegs.aeruces = 0;
    cmdLine.errRegs.csts = CSTS_CFS;
    cmdLine.dump = BASE_DUMP_DIR;

    if (argc == 1) {
        printf("%s is a compliance test suite for NVM Express hardware.\n",
            APPNAME);
        exit(0);
    }

    // Disable buffering stdout, risk not seeing statements merged with dump dir
    setbuf(stdout, NULL);

    // Seek for all possible devices that this app may commune
    DIR *devDir = opendir("/dev");
    if (devDir == NULL) {
        printf("Unable to open system /dev directory\n");
        exit(1);
    }
    while ((dirEntry = readdir(devDir)) != NULL) {
        work = dirEntry->d_name;
        if (work.find("nvme") != string::npos)
            devices.push_back("/dev/" + work);
    }
    if (devices.size())
        cmdLine.device = devices[0];    // Default to 1st element listed

    // Report what we are about to process
    work = "";
    for (int i = 0; i < argc; i++)
        work += argv[i] + string(" ");
    printf("Parsing cmd line: %s\n", work.c_str());


    // Parse cmd line options
    while ((c = getopt_long(argc, argv, short_opt, long_opt, &idx)) != -1) {
        switch (c) {

        case 'v':
            if (strcmp("1.0a", optarg) == 0) {
                cmdLine.rev = SPECREV_10b;
            }
            break;

        case 'a':
            if (ParseTargetCmdLine(cmdLine.detail, optarg) == false) {
                printf("Unable to parse --detail cmd line\n");
                exit(1);
            }
            accessingHdw = false;
            break;

        case 't':
            if (ParseTargetCmdLine(cmdLine.test, optarg) == false) {
                printf("Unable to parse --test cmd line\n");
                exit(1);
            }
            break;

        case 'k':
            if (ParseSkipTestCmdLine(cmdLine.skiptest, optarg) == false) {
                printf("Unable to parse --skiptest cmd line\n");
                exit(1);
            }
            break;

        case 'g':
            if (ParseGoldenCmdLine(cmdLine.golden, optarg) == false) {
                printf("Unable to parse --golden cmd line\n");
                exit(1);
            }
            break;

        case 'f':
            if (ParseFormatCmdLine(cmdLine.format, optarg) == false) {
                printf("Unable to parse --format cmd line\n");
                exit(1);
            }
            break;

        case 'r':
            if (ParseRmmapCmdLine(cmdLine.rmmap, optarg) == false) {
                printf("Unable to parse --rmmap cmd line\n");
                exit(1);
            }
            break;

        case 'w':
            if (ParseWmmapCmdLine(cmdLine.wmmap, optarg) == false) {
                printf("Unable to parse --wmmap cmd line\n");
                exit(1);
            }
            break;

        case 'e':
            if (ParseErrorCmdLine(cmdLine.errRegs, optarg) == false) {
                printf("Unable to parse --error cmd line\n");
                exit(1);
            }
            break;

        case 'd':
            work = optarg;
            for (size_t i = 0; i < devices.size(); i++) {
                if (work.compare(devices[i]) == 0) {
                    cmdLine.device = work;
                    deviceFound = true;
                    break;
                }
            }
            if (deviceFound == false) {
                printf("/dev/%s is not among possible devices "
                    "which can be tested\n", work.c_str());
                exit(1);
            }
            break;

        case 'p':
            tmp = strtol(optarg, &endptr, 10);
            if (*endptr != '\0') {
                printf("Unrecognized --loop <count>=%s\n", optarg);
                exit(1);
            } else if (tmp <= 0) {
                printf("Negative/zero values for --loop are unproductive\n");
                exit(1);
            }
            cmdLine.loop = tmp;
            break;

        case 'l':
            printf("Devices available for test:\n");
            if (devices.size() == 0) {
                printf("none\n");
            } else {
                for (size_t i = 0; i < devices.size(); i++) {
                    printf("%s\n", devices[i].c_str());
                }
            }
            exit(0);

        case 'q':
            if (ParseQueuesCmdLine(cmdLine.numQueues, optarg) == false) {
                printf("Unable to parse --queues cmd line\n");
                exit(1);
            }
            break;

        case 's':
            cmdLine.summary = true;
            accessingHdw = false;
            break;

        case 'u':
            cmdLine.dump = optarg;
            break;

        default:
        case 'h':   Usage();                            exit(0);
        case '?':   Usage();                            exit(1);
        case 'z':   cmdLine.reset = true;               break;
        case 'i':   cmdLine.ignore = true;              break;
        }
    }

    if (optind < argc) {
        printf("Unable to parse all cmd line arguments: %d of %d: ",
            optind, argc);
        while (optind < argc)
            printf("%s ", argv[optind++]);
        printf("\n");
        Usage();
        exit(1);
    }

    try {   // Everything below has the ability to throw exceptions

        // Instantiates and initializes all globals defined within globals.h
        if (BuildTestFoundation(groups, cmdLine) == false) {
            printf("Unable to build the test foundation\n");
            exit(1);
        }

        // Accessing hdw requires specific checks and inquiries before running
        if (accessingHdw) {
            if (FileSystem::SetRootDumpDir(cmdLine.dump) == false) {
                printf("Unable to establish \"%s\" dump directory\n",
                    cmdLine.dump.c_str());
                exit(1);
            }

            if (BuildSingletons(cmdLine) == false) {
                printf("Unable to instantiate mandatory framework objects\n");
                exit(1);
            }

            printf("Checking for unintended device under low powered states\n");
            if (gRegisters->Read(PCISPC_PMCS, regVal) == false) {
                printf("Mandatory PMCAP PCI capabilities is missing\n");
                exit(1);
            } else if (regVal & 0x03) {
                printf("PCI power state not fully operational\n");
            }
        }

        // Process the user's cmd line parameters
        if (cmdLine.golden.req) {
            if ((exitCode = !CompareGolden(cmdLine.golden))) {
                printf("FAILURE: Comparing golden data\n");
            } else {
                printf("SUCCESS: Comparing golden data\n");
            }
        } else if (cmdLine.format.req) {
            if ((exitCode = !FormatDevice(cmdLine.format))) {
                printf("FAILURE: Formatting device\n");
            } else {
                printf("SUCCESS: Formatting device\n");
            }
        } else if (cmdLine.numQueues.req) {
            if ((exitCode = !SetFeaturesNumberOfQueues(cmdLine.numQueues))) {
                printf("FAILURE: Setting number of queues\n");
            } else {
                printf("SUCCESS: Setting number of queues\n");
            }
        } else if (cmdLine.summary) {
            for (size_t i = 0; i < groups.size(); i++) {
                FORMAT_GROUP_DESCRIPTION(work, groups[i])
                printf("%s\n", work.c_str());
                printf("%s", groups[i]->GetGroupSummary(false).c_str());
            }

        } else if (cmdLine.detail.req) {
            if (cmdLine.detail.t.group == UINT_MAX) {
                for (size_t i = 0; i < groups.size(); i++) {
                    FORMAT_GROUP_DESCRIPTION(work, groups[i])
                    printf("%s\n", work.c_str());
                    printf("%s", groups[i]->GetGroupSummary(true).c_str());
                }

            } else {    // user spec'd a group they are interested in
                if (cmdLine.detail.t.group >= groups.size()) {
                    printf("Specified test group %ld does not exist\n",
                        cmdLine.detail.t.group);
                } else {
                    for (size_t i = 0; i < groups.size(); i++) {
                        if (i == cmdLine.detail.t.group) {
                            FORMAT_GROUP_DESCRIPTION(work, groups[i])
                            printf("%s\n", work.c_str());

                            if ((cmdLine.detail.t.xLev == UINT_MAX) ||
                                (cmdLine.detail.t.yLev == UINT_MAX) ||
                                (cmdLine.detail.t.zLev == UINT_MAX)) {
                                // Want info on all tests within group
                                printf("%s",
                                    groups[i]->GetGroupSummary(true).c_str());
                            } else {
                                // Want info on spec'd test within group
                                printf("%s", groups[i]->GetTestDescription(true,
                                    cmdLine.detail.t).c_str());
                                break;
                            }
                        }
                    }
                }
            }
        } else if (cmdLine.rmmap.req) {
            uint8_t *value = new uint8_t[cmdLine.rmmap.size];
            gRegisters->Read(cmdLine.rmmap.space, cmdLine.rmmap.size,
                cmdLine.rmmap.offset, cmdLine.rmmap.acc, value);
        } else if (cmdLine.wmmap.req) {
            gRegisters->Write(cmdLine.wmmap.space, cmdLine.wmmap.size,
                cmdLine.wmmap.offset, cmdLine.wmmap.acc,
                (uint8_t *)(&cmdLine.wmmap.value));
        } else if (cmdLine.reset) {
            if ((exitCode = !gCtrlrConfig->SetState(ST_DISABLE_COMPLETELY))) {
                printf("FAILURE: reset\n");
            } else {
                printf("SUCCESS: reset\n");
            }
            // At this point we cannot enable the ctrlr because that requires
            // ACQ/ASQ's to be created, ctrlr simply won't become ready w/o them
        } else if (cmdLine.test.req) {
            if ((exitCode = !ExecuteTests(cmdLine, groups))) {
                printf("FAILURE: testing\n");
            } else {
                printf("SUCCESS: testing\n");
            }
        }
    } catch (...) {
        LOG_ERR("An unforeseen exception has been caught");
    }

    // cleanup duties
    DestroyTestFoundation(groups);
    DestroySingletons();
    cmdLine.skiptest.clear();
    devices.clear();
    exit(exitCode);
}


bool BuildSingletons(struct CmdLine &cl)
{
    // Create globals/singletons here, which all tests objects will need

    // The Register singleton should be created 1st because all other Singletons
    // use it directly to become init'd or they rely on it heavily soon after.
    gRegisters = Registers::GetInstance(gDutFd, cl.rev);
    if (gRegisters == NULL) {
        LOG_ERR("Unable to create framework obj: gRegisters");
        return false;
    }

    // The CtrlrConfig singleton should be created 2nd because it's subject base
    // class is used by just about every other object in the framework to learn
    // of state changes within the ctrlr. Disabling the ctrlr is extremely
    // destructive of all resources in user space and in kernel space.
    gCtrlrConfig = CtrlrConfig::GetInstance(gDutFd, cl.rev);
    if (gCtrlrConfig == NULL) {
        LOG_ERR("Unable to create framework obj: gCtrlrConfig");
        return false;
    }

    // Create the remaining objects at random...
    gRsrcMngr = RsrcMngr::GetInstance(gDutFd, cl.rev);
    if (gRsrcMngr == NULL) {
        LOG_ERR("Unable to create framework obj: gRsrcMngr");
        return false;
    }
    gCtrlrConfig->Attach(*gRsrcMngr);

    gInformative = Informative::GetInstance(gDutFd, cl.rev);
    if (gInformative == NULL) {
        LOG_ERR("Unable to create framework obj: gInformative");
        return false;
    }

    return true;
}


void DestroySingletons()
{
    // Destroy in reverse order as was created
    RsrcMngr::KillInstance();
    Informative::KillInstance();
    CtrlrConfig::KillInstance();
    Registers::KillInstance();
}


/**
 * A function to perform the necessary duties to create the objects, interact
 * with the OS which will allow testing to commence according to the cmd line
 * options presented to this app.
 * @param groups Pass the structure to contain the test objects
 * @param cl Pass the cmd line args
 * @return true upon success, otherwise false
 */
bool
BuildTestFoundation(vector<Group *> &groups, struct CmdLine &cl)
{
    int ret;
    struct metrics_driver driverMetrics;
    struct flock fdlock = {F_WRLCK, SEEK_SET, 0, 0, 0};


    DestroyTestFoundation(groups);

    // Open and lock access to the device requested for testing. The mutually
    // exclusive write lock is expected to warrant off other instances of this
    // app from choosing to test against the same device. An app which has
    // attained a lock on the target device may have multiple threads which
    // could cause testing corruption, and therefore a single threaded device
    // interaction model is needed. No more than 1 test can occur at any time
    // to any device and all tests must be single threaded.
    if (cl.device.compare(NO_DEVICES) == 0) {
        LOG_ERR("There are no devices present");
        return false;
    }
    if ((gDutFd = open(cl.device.c_str(), O_RDWR)) == -1) {
        if ((errno == EACCES) || (errno == EAGAIN)) {
            LOG_ERR("%s may need permission set for current user",
                cl.device.c_str());
        }
        LOG_ERR("device=%s: %s", cl.device.c_str(), strerror(errno));
        return false;
    }
    if (fcntl(gDutFd, F_SETLK, &fdlock) == -1) {
        if ((errno == EACCES) || (errno == EAGAIN))
            LOG_ERR("%s has been locked by another process", cl.device.c_str());
        LOG_ERR("%s", strerror(errno));
    }

    // Validate the dnvme was compiled with the same version of API as tnvme
    ret = ioctl(gDutFd, NVME_IOCTL_GET_DRIVER_METRICS, &driverMetrics);
    if (ret < 0) {
        LOG_ERR("Unable to extract driver version information");
        return false;
    }
    LOG_NRM("tnvme binary: v/%d.%d", VER_MAJOR, VER_MINOR);
    LOG_NRM("tnvme compiled against dnvme API: v/%d.%d.%d",
        ((API_VERSION >> 16) & 0xFF),
        ((API_VERSION >> 8) & 0xFF),
        ((API_VERSION >> 0) & 0xFF));
    LOG_NRM("dnvme API residing within kernel: v/%d.%d.%d",
        ((driverMetrics.api_version >> 16) & 0xFF),
        ((driverMetrics.api_version >> 8) & 0xFF),
        ((driverMetrics.api_version >> 0) & 0xFF));
    if (driverMetrics.api_version != API_VERSION) {
        LOG_ERR("dnvme vs tnvme version mismatch, refusing to execute");
        return false;
    }

    InstantiateGroups(groups, cl);
    return true;
}


/**
 * Tear down that which has been created by BuildTestFoundation()
 * @param groups Pass the structure to contain the test objects, if the
 *               structure is not empty the function will abort.
 */
void
DestroyTestFoundation(vector<Group *> &groups)
{
    // Deallocate heap usage
    for (size_t i = 0; i < groups.size(); i++) {
        delete groups.back();
        groups.pop_back();
    }

    // If it fails what do we do? ignore it for now
    if (gDutFd != -1) {
        if (close(gDutFd) == -1)
            LOG_ERR("%s", strerror(errno));
        gDutFd = -1;
    }
}


/**
 * A function to execute the desired test case(s). Param ignore
 * indicates that when an error is reported from a test case, it is ignored to
 * the point that the next test case will be allowed to run, if and only if,
 * there are more tests which could have run if the test passed. The error
 * itself won't be lost. because the end return value from this function will
 * indicate an error was detected even though it was ignored.
 * @param cl Pass the cmd line parameters
 * @param groups Pass all groups being considered for execution
 * @return true upon success, false if failures/errors detected;
 */
bool
ExecuteTests(struct CmdLine &cl, vector<Group *> &groups)
{
    size_t iLoop;
    int numPassed = 0;
    int numFailed = 0;
    int numSkipped = 0;
    bool allTestsPass = true;    // assuming success until we find otherwise
    bool allHaveRun = false;
    bool thisTestPass;
    TestIteratorType testIter;
    vector<TestRef> skipNothing;


    if ((cl.test.t.group != UINT_MAX) && (cl.test.t.group >= groups.size())) {
        LOG_ERR("Specified test group does not exist");
        goto ABORT_OUT;
    } else if (VerifySpecCompatibility(cl.rev) == false) {
        LOG_ERR("Targeted compliance revision incompatible with hdw");
        goto ABORT_OUT;
    }

    LOG_NRM("Attempting to satisfy target test: %ld:%ld.%ld.%ld",
        cl.test.t.group, cl.test.t.xLev, cl.test.t.yLev, cl.test.t.zLev);

    for (iLoop = 0; iLoop < cl.loop; iLoop++) {
        LOG_NRM("Start loop execution #%ld", iLoop);
        for (size_t iGrp = 0; iGrp < groups.size(); iGrp++) {
            allHaveRun = false;

            if (FileSystem::CleanDumpDir() == false)
                LOG_WARN("Unable to cleanup dump between group runs");

            // Now handle anything spec'd in the --test <cmd line option>
            if (cl.test.t.group == UINT_MAX) {
                // Run all tests within all groups

                LOG_NRM("Executing a new group, start from known point");
                if (gCtrlrConfig->SetState(ST_DISABLE_COMPLETELY) == false)
                    goto ABORT_OUT;

                testIter = groups[iGrp]->GetTestIterator();
                while (allHaveRun == false) {
                    thisTestPass = true;

                    switch (groups[iGrp]->RunTest(testIter, cl.skiptest)) {
                    case Group::TR_SUCCESS:
                        numPassed++;
                        break;
                    case Group::TR_FAIL:
                        allTestsPass = false;
                        thisTestPass = false;
                        numFailed++;
                        break;
                    case Group::TR_SKIPPING:
                        numSkipped++;
                        break;
                    case Group::TR_NOTFOUND:
                        allHaveRun = true;
                        break;
                    }
                    if ((cl.ignore == false) && (allTestsPass == false)) {
                        goto EARLY_OUT;
                    } else if (cl.ignore && (thisTestPass == false)) {
                        LOG_NRM("Detected error, but forced to ignore");
                        break;  // continue with next test in next group
                    }
                }

            } else if ((cl.test.t.xLev == UINT_MAX) ||
                       (cl.test.t.yLev == UINT_MAX) ||
                       (cl.test.t.zLev == UINT_MAX)) {
                // Run all tests within spec'd group

                if (iGrp == cl.test.t.group) {
                    LOG_NRM("Executing a new group, start from known point");
                    if (gCtrlrConfig->SetState(ST_DISABLE_COMPLETELY) == false)
                        goto ABORT_OUT;

                    testIter = groups[iGrp]->GetTestIterator();
                    while (allHaveRun == false) {
                        thisTestPass = true;

                        switch (groups[iGrp]->RunTest(testIter, cl.skiptest)) {
                        case Group::TR_SUCCESS:
                            numPassed++;
                            break;
                        case Group::TR_FAIL:
                            allTestsPass = false;
                            thisTestPass = false;
                            numFailed++;
                            break;
                        case Group::TR_SKIPPING:
                            numSkipped++;
                            break;
                        case Group::TR_NOTFOUND:
                            allHaveRun = true;
                            break;
                        }
                        if ((cl.ignore == false) && (allTestsPass == false)) {
                            goto EARLY_OUT;
                        } else if (cl.ignore && (thisTestPass == false)) {
                            LOG_NRM("Detected error, but forced to ignore");
                            break;  // continue with next test in next group
                        }
                    }
                    break;  // check if more loops must occur
                }

            } else {
                // Run spec'd test within spec'd group
                if (iGrp == cl.test.t.group) {
                    LOG_NRM("Executing a new group, start from known point");
                    if (gCtrlrConfig->SetState(ST_DISABLE_COMPLETELY) == false)
                        goto ABORT_OUT;

                    // Individual tests may have dependencies to automate.
                    TestRef cfgDepend;
                    TestIteratorType seqDepend;
                    bool runTargetTest = groups[iGrp]->GetTestDependency(
                        cl.test.t, cfgDepend, seqDepend);
                    if (runTargetTest == false)
                        goto ABORT_OUT;

                    // Does this test have a configuration dependency?
                    if (runTargetTest && !(cfgDepend == cl.test.t)) {
                        thisTestPass = true;

                        switch (groups[iGrp]->RunTest(cfgDepend, cl.skiptest)) {
                        case Group::TR_SUCCESS:
                            numPassed++;
                            break;
                        case Group::TR_FAIL:
                            LOG_NRM(
                                "Unable to run targeted test due to failure");
                            runTargetTest = false;
                            allTestsPass = false;
                            thisTestPass = false;
                            numFailed++;
                            break;
                        case Group::TR_SKIPPING:
                            LOG_NRM(
                                "Unable to run targ'd test due to skipping");
                            runTargetTest = false;
                            numSkipped++;
                            break;
                        case Group::TR_NOTFOUND:
                            LOG_ERR("Internal programming error, ambiguous");
                            goto ABORT_OUT;
                        }
                        if ((cl.ignore == false) && (allTestsPass == false)) {
                            goto EARLY_OUT;
                        } else if (cl.ignore && (thisTestPass == false)) {
                            LOG_NRM("Detected error, but forced to ignore");
                            LOG_NRM("Error forces skipping of targ'd test");
                        }
                    }

                    // Sequences dependency may only run after config dependency
                    if (runTargetTest && (seqDepend != (TestIteratorType)-1)) {

                        // Iterate sequence up to but not including targeted tst
                        TestIteratorType targetedTestIter;
                        if (groups[iGrp]->TestRefToIterator(cl.test.t,
                            targetedTestIter) == false) {
                            goto ABORT_OUT;
                        }

                        // Loop through all of the the sequence dependencies
                        while (seqDepend < targetedTestIter) {
                            thisTestPass = true;

                            switch (groups[iGrp]->RunTest(seqDepend,
                                cl.skiptest)) {

                            case Group::TR_SUCCESS:
                                numPassed++;
                                break;
                            case Group::TR_FAIL:
                                LOG_NRM(
                                    "Unable to run targ'd test due to failure");
                                runTargetTest = false;
                                allTestsPass = false;
                                thisTestPass = false;
                                numFailed++;
                                break;
                            case Group::TR_SKIPPING:
                                LOG_NRM("Unable to run targ'd test due to "
                                    "skipping");
                                runTargetTest = false;
                                numSkipped++;
                                break;
                            case Group::TR_NOTFOUND:
                                LOG_ERR("Internal program error, ambiguous");
                                goto ABORT_OUT;
                            }
                            if ((cl.ignore == false) &&
                                (allTestsPass == false)) {
                                goto EARLY_OUT;
                            } else if (cl.ignore && (thisTestPass == false)) {
                                LOG_NRM("Detected error, but forced to ignore");
                                LOG_NRM("Error forces skipping of targ'd test");
                                break;  // check if more loops must occur
                            }
                        }
                    }

                    // Finally OK to run targeted test requested on the cmd line
                    if (runTargetTest) {
                        thisTestPass = true;

                        switch (groups[iGrp]->RunTest(cl.test.t, cl.skiptest)) {
                        case Group::TR_SUCCESS:
                            numPassed++;
                            break;
                        case Group::TR_FAIL:
                            allTestsPass = false;
                            thisTestPass = false;
                            numFailed++;
                            break;
                        case Group::TR_SKIPPING:
                            numSkipped++;
                            break;
                        case Group::TR_NOTFOUND:
                            LOG_NRM("Requested test was not found");
                            goto ABORT_OUT;
                            break;
                        }
                        if ((cl.ignore == false) && (allTestsPass == false)) {
                            goto EARLY_OUT;
                        } else if (cl.ignore && (thisTestPass == false)) {
                            LOG_NRM("Detected error, but forced to ignore");
                        }
                    }
                    break;  // check if more loops must occur
                }
            }
        }

        // Report each iteration results
        ReportTestResults(iLoop, numPassed, numFailed, numSkipped);
    }
    return allTestsPass;

EARLY_OUT:
    ReportTestResults(iLoop, numPassed, numFailed, numSkipped);
    return allTestsPass;

ABORT_OUT:
    LOG_NRM("Iteration SUMMARY  : Testing aborted");
    return false;
}


void
ReportTestResults(size_t numIters, int numPass, int numFail, int numSkip)
{
    LOG_NRM("Iteration SUMMARY passed : %d", numPass);
    if (numFail) {
        LOG_NRM("                  failed : %d  <----------", numFail);
    } else {
        LOG_NRM("                  failed : 0");
    }
    LOG_NRM("                  skipped: %d", numSkip);
    LOG_NRM("                  total  : %d", numPass+numFail+numSkip);
    LOG_NRM("Stop loop execution #%ld", numIters);
}

