/*
 * Copyright (c) 2020 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 1999-2008 Mark D. Hill and David A. Wood
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MEM_RUBY_COMMON_MACHINEID_HH__
#define __MEM_RUBY_COMMON_MACHINEID_HH__

#include <iostream>
#include <string>

#include "base/cprintf.hh"
#include "mem/ruby/protocol/MachineType.hh"

struct MachineID
{
    MachineID() : type(MachineType_NUM), num(0) { }
    MachineID(MachineType mach_type, NodeID node_id)
        : type(mach_type), num(node_id) { }

    MachineType type;
    //! range: 0 ... number of this machine's components in system - 1
    NodeID num;

    MachineType getType() const { return type; }
    NodeID getNum() const { return num; }

    NodeID getNodeID() const { return MachineType_base_number(type) + num; }

    bool isValid() const { return type != MachineType_NUM; }
};

inline std::string
MachineIDToString(MachineID machine)
{
    return csprintf("%s_%d", MachineType_to_string(machine.type), machine.num);
}

inline bool
operator==(const MachineID & obj1, const MachineID & obj2)
{
    return (obj1.type == obj2.type && obj1.num == obj2.num);
}

inline bool
operator!=(const MachineID & obj1, const MachineID & obj2)
{
    return (obj1.type != obj2.type || obj1.num != obj2.num);
}

namespace std {
    template<>
    struct hash<MachineID> {
        inline size_t operator()(const MachineID& id) const {
            size_t hval = MachineType_base_level(id.type) << 16 | id.num;
            return hval;
        }
    };
}

// Output operator declaration
std::ostream& operator<<(std::ostream& out, const MachineID& obj);

inline std::ostream&
operator<<(std::ostream& out, const MachineID& obj)
{
    if ((obj.type < MachineType_NUM) && (obj.type >= MachineType_FIRST)) {
        out << MachineType_to_string(obj.type);
    } else {
        out << "NULL";
    }
    out << "-";
    out << obj.num;
    out << std::flush;
    return out;
}

#endif // __MEM_RUBY_COMMON_MACHINEID_HH__
