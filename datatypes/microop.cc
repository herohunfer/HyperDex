// Copyright (c) 2012, Cornell University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of HyperDex nor the names of its contributors may be
//       used to endorse or promote products derived from this software without
//       specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#define __STDC_LIMIT_MACROS

// HyperDex
#include "datatypes/microop.h"

microop :: microop()
    : attr(UINT16_MAX)
    , action()
    , arg1()
    , arg1_datatype()
    , arg2()
    , arg2_datatype()
{
}

microop :: ~microop() throw ()
{
}

bool
operator < (const microop& lhs, const microop& rhs)
{
    return lhs.attr < rhs.attr;
}

e::buffer::packer
operator << (e::buffer::packer lhs, const microop& rhs)
{
    uint8_t action = static_cast<uint8_t>(rhs.action);
    uint16_t arg1_datatype = static_cast<uint16_t>(rhs.arg1_datatype);
    uint16_t arg2_datatype = static_cast<uint16_t>(rhs.arg2_datatype);
    lhs = lhs << rhs.attr << action
              << rhs.arg1 << arg1_datatype
              << rhs.arg2 << arg2_datatype;
    return lhs;
}

e::buffer::unpacker
operator >> (e::buffer::unpacker lhs, microop& rhs)
{
    uint8_t action;
    uint16_t arg1_datatype;
    uint16_t arg2_datatype;
    lhs = lhs >> rhs.attr >> action
              >> rhs.arg1 >> arg1_datatype
              >> rhs.arg2 >> arg2_datatype;
    rhs.action = static_cast<microaction>(action);
    rhs.arg1_datatype = static_cast<hyperdatatype>(arg1_datatype);
    rhs.arg2_datatype = static_cast<hyperdatatype>(arg2_datatype);
    return lhs;
}

size_t
pack_size(const microop& m)
{
    return sizeof(uint16_t)
         + sizeof(uint8_t)
         + sizeof(uint32_t) + m.arg1.size() + sizeof(uint16_t)
         + sizeof(uint32_t) + m.arg2.size() + sizeof(uint16_t);
}
