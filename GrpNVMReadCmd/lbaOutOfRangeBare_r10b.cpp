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

#include "boost/format.hpp"
#include "lbaOutOfRangeBare_r10b.h"
#include "globals.h"
#include "grpDefs.h"
#include "../Queues/iocq.h"
#include "../Queues/iosq.h"
#include "../Utils/io.h"
#include "../Cmds/read.h"

namespace GrpNVMReadCmd {

#define RD_NUM_BLKS                 2


LBAOutOfRangeBare_r10b::LBAOutOfRangeBare_r10b(int fd, string mGrpName,
    string mTestName, ErrorRegs errRegs) :
    Test(fd, mGrpName, mTestName, SPECREV_10b, errRegs)
{
    // 63 chars allowed:     xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    mTestDesc.SetCompliance("revision 1.0b, section 4,6");
    mTestDesc.SetShort(     "Issue read and cause SC=LBA Out of Range on bare namspcs");
    // No string size limit for the long description
    mTestDesc.SetLong(
        "For all bare namspcs from Identify.NN, determine Identify.NSZE; For "
        "each namspc cause many scenarios by issuing a single read cmd "
        "requesting 2 data blocks. 1) Issue cmd where 1st block starts at LBA "
        "(Identify.NSZE - 1), expect failure 2) Issue cmd where 1st block "
        "starts at LBA Identify.NSZE, expect failure. 3) Issue cmd where 1st "
        "block starts at 2nd to last max LBA value, expect success.");
}


LBAOutOfRangeBare_r10b::~LBAOutOfRangeBare_r10b()
{
    ///////////////////////////////////////////////////////////////////////////
    // Allocations taken from the heap and not under the control of the
    // RsrcMngr need to be freed/deleted here.
    ///////////////////////////////////////////////////////////////////////////
}


LBAOutOfRangeBare_r10b::
LBAOutOfRangeBare_r10b(const LBAOutOfRangeBare_r10b &other) : Test(other)
{
    ///////////////////////////////////////////////////////////////////////////
    // All pointers in this object must be NULL, never allow shallow or deep
    // copies, see Test::Clone() header comment.
    ///////////////////////////////////////////////////////////////////////////
}


LBAOutOfRangeBare_r10b &
LBAOutOfRangeBare_r10b::operator=(const LBAOutOfRangeBare_r10b &other)
{
    ///////////////////////////////////////////////////////////////////////////
    // All pointers in this object must be NULL, never allow shallow or deep
    // copies, see Test::Clone() header comment.
    ///////////////////////////////////////////////////////////////////////////
    Test::operator=(other);
    return *this;
}


void
LBAOutOfRangeBare_r10b::RunCoreTest()
{
    /** \verbatim
     * Assumptions:
     * 1) Test CreateResources_r10b has run prior.
     * \endverbatim
     */
    uint64_t nsze;
    char work[256];
    ConstSharedIdentifyPtr namSpcPtr;

    // Lookup objs which were created in a prior test within group
    SharedIOSQPtr iosq = CAST_TO_IOSQ(gRsrcMngr->GetObj(IOSQ_GROUP_ID));
    SharedIOCQPtr iocq = CAST_TO_IOCQ(gRsrcMngr->GetObj(IOCQ_GROUP_ID));

    vector<uint32_t> bare = gInformative->GetBareNamespaces();
    for (size_t i = 0; i < bare.size(); i++) {

        namSpcPtr = gInformative->GetIdentifyCmdNamspc(bare[i]);
        if (namSpcPtr == Identify::NullIdentifyPtr) {
            throw FrmwkEx(HERE, "Identify namspc struct #%d doesn't exist",
                bare[i]);
        }
        nsze = namSpcPtr->GetValue(IDNAMESPC_NSZE);

        LOG_NRM("Create memory to contain read payload");
        SharedMemBufferPtr readMem = SharedMemBufferPtr(new MemBuffer());
        uint64_t lbaDataSize = namSpcPtr->GetLBADataSize();
        readMem->Init(RD_NUM_BLKS * lbaDataSize);

        LOG_NRM("Create a read cmd to read data from namspc %d", bare[i]);
        SharedReadPtr readCmd = SharedReadPtr(new Read());
        send_64b_bitmask prpBitmask = (send_64b_bitmask)
            (MASK_PRP1_PAGE | MASK_PRP2_PAGE | MASK_PRP2_LIST);
        readCmd->SetPrpBuffer(prpBitmask, readMem);
        readCmd->SetNSID(bare[i]);
        readCmd->SetNLB(RD_NUM_BLKS - 1);    // convert to 0-based value

        LOG_NRM("Issue cmd where 1st block starts at LBA (Identify.NSZE-2)");
        snprintf(work, sizeof(work), "nsze-2.%01d", (uint32_t)i);
        readCmd->SetSLBA(nsze - 2);
        IO::SendAndReapCmd(mGrpName, mTestName, DEFAULT_CMD_WAIT_ms, iosq,
            iocq, readCmd, work, true);

        LOG_NRM("Issue cmd where 1st block starts at LBA (Identify.NSZE-1)");
        snprintf(work, sizeof(work), "nsze-1.%01d", (uint32_t)i);
        readCmd->SetSLBA(nsze - 1);
        SendCmdToHdw(iosq, iocq, readCmd, work);

        LOG_NRM("Issue cmd where 1st block starts at LBA (Identify.NSZE)");
        snprintf(work, sizeof(work), "nsze.%01d", (uint32_t)i);
        readCmd->SetSLBA(nsze);
        SendCmdToHdw(iosq, iocq, readCmd, work);
    }
}


void
LBAOutOfRangeBare_r10b::SendCmdToHdw(SharedSQPtr sq, SharedCQPtr cq,
    SharedCmdPtr cmd, string qualify)
{
    uint32_t numCE;
    uint32_t isrCount;
    uint32_t isrCountB4;
    string work;
    uint16_t uniqueId;


    if ((numCE = cq->ReapInquiry(isrCountB4, true)) != 0) {
        cq->Dump(
            FileSystem::PrepDumpFile(mGrpName, mTestName, "cq",
            "notEmpty"), "Test assumption have not been met");
        throw FrmwkEx(HERE, "Require 0 CE's within CQ %d, not upheld, found %d",
            cq->GetQId(), numCE);
    }

    LOG_NRM("Send the cmd to hdw via SQ %d", sq->GetQId());
    sq->Send(cmd, uniqueId);
    work = str(boost::format(
        "Just B4 ringing SQ %d doorbell, dump entire SQ") % sq->GetQId());
    sq->Dump(FileSystem::PrepDumpFile(mGrpName, mTestName,
        "sq." + cmd->GetName(), qualify), work);
    sq->Ring();


    LOG_NRM("Wait for the CE to arrive in CQ %d", cq->GetQId());
    if (cq->ReapInquiryWaitSpecify(DEFAULT_CMD_WAIT_ms, 1, numCE, isrCount)
        == false) {

        work = str(boost::format(
            "Unable to see any CE's in CQ %d, dump entire CQ") % cq->GetQId());
        cq->Dump(
            FileSystem::PrepDumpFile(mGrpName, mTestName, "cq." +
            cmd->GetName(), qualify), work);
        throw FrmwkEx(HERE, "Unable to see CE for issued cmd");
    } else if (numCE != 1) {
        work = str(boost::format(
            "Unable to see any CE's in CQ %d, dump entire CQ") % cq->GetQId());
        cq->Dump(
            FileSystem::PrepDumpFile(mGrpName, mTestName, "cq." +
            cmd->GetName(), qualify), work);
        throw FrmwkEx(HERE, "1 cmd caused %d CE's to arrive in CQ %d",
            numCE, cq->GetQId());
    }
    work = str(boost::format("Just B4 reaping CQ %d, dump entire CQ") %
        cq->GetQId());
    cq->Dump(FileSystem::PrepDumpFile(mGrpName, mTestName,
        "cq." + cmd->GetName(), qualify), work);

    // throws if an error occurs
    IO::ReapCE(cq, numCE, isrCount, mGrpName, mTestName, qualify,
        CESTAT_LBA_OUT_RANGE);

    // Single cmd submitted on empty ASQ should always yield 1 IRQ on ACQ
    if (gCtrlrConfig->IrqsEnabled() && cq->GetIrqEnabled() &&
        (isrCount != (isrCountB4 + 1))) {

        throw FrmwkEx(HERE,
            "CQ using IRQ's, but IRQ count not expected (%d != %d)",
            isrCount, (isrCountB4 + 1));
    }
}

}   // namespace

