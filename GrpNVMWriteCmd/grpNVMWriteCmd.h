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

#ifndef _GRPNVMWRITECMD_H_
#define _GRPNVMWRITECMD_H_

#include "../group.h"

namespace GrpNVMWriteCmd {


/**
* This class implements NVM cmd set write test cases that don't require data
* to be written to media 1st to detect a successful outcome.
*/
class GrpNVMWriteCmd : public Group
{
public:
    GrpNVMWriteCmd(size_t grpNum, SpecRev specRev, ErrorRegs errRegs, int fd);
    virtual ~GrpNVMWriteCmd();
};

}   // namespace

#endif
