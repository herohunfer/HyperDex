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

// e
#include <e/array_ptr.h>
#include <e/endian.h>

// e
#include <map>

// HyperDex
#include "datatypes/alltypes.h"
#include "datatypes/compare.h"
#include "datatypes/set.h"
#include "datatypes/sort.h"
#include "datatypes/step.h"
#include "datatypes/write.h"

static bool
validate_map(bool (*step_key)(const uint8_t** ptr, const uint8_t* end, e::slice* elem),
             bool (*step_val)(const uint8_t** ptr, const uint8_t* end, e::slice* elem),
             bool (*compare_key_less)(const e::slice& lhs, const e::slice& rhs),
             const e::slice& map)
{
    const uint8_t* ptr = map.data();
    const uint8_t* end = map.data() + map.size();
    e::slice key;
    e::slice val;
    e::slice old;
    bool has_old = false;

    while (ptr < end)
    {
        if (!step_key(&ptr, end, &key))
        {
            return false;
        }

        if (!step_val(&ptr, end, &val))
        {
            return false;
        }

        if (has_old)
        {
            if (!compare_key_less(old, key))
            {
                return false;
            }
        }

        old = key;
        has_old = true;
    }

    return ptr == end;
}

#define VALIDATE_MAP(KEY_T, VAL_T) \
    bool \
    validate_as_map_ ## KEY_T ## _ ## VAL_T(const e::slice& value) \
    { \
        return validate_map(step_ ## KEY_T, step_ ## VAL_T, compare_ ## KEY_T, value); \
    }

VALIDATE_MAP(string, string)
VALIDATE_MAP(string, int64)
VALIDATE_MAP(string, float)
VALIDATE_MAP(int64, string)
VALIDATE_MAP(int64, int64)
VALIDATE_MAP(int64, float)
VALIDATE_MAP(float, string)
VALIDATE_MAP(float, int64)
VALIDATE_MAP(float, float)

static bool
apply_map_microop(uint8_t* (*apply_pod)(const e::slice& old_value,
                                        const microop* ops, size_t num_ops,
                                        uint8_t* writeto, microerror* error),
                  std::map<e::slice, e::slice>* map,
                  e::array_ptr<uint8_t>* scratch,
                  const microop* ops, microerror* error)
{
    std::map<e::slice, e::slice>::iterator it = map->find(ops->arg2);
    e::slice old_value("", 0);

    if (it != map->end())
    {
        old_value = it->second;
    }

    *scratch = new uint8_t[old_value.size() + sizeof(uint32_t) + ops->arg1.size()];
    uint8_t* write_to = apply_pod(old_value, ops, 1, scratch->get(), error);
    (*map)[ops->arg2] = e::slice(scratch->get(), write_to - scratch->get());
    return write_to != NULL;
}

static uint8_t*
apply_map(bool (*step_key)(const uint8_t** ptr, const uint8_t* end, e::slice* elem),
          bool (*step_val)(const uint8_t** ptr, const uint8_t* end, e::slice* elem),
          bool (*validate_key)(const e::slice& elem),
          bool (*validate_val)(const e::slice& elem),
          bool (*compare_pair_less)(const std::pair<e::slice, e::slice>& lhs,
                                    const std::pair<e::slice, e::slice>& rhs),
          uint8_t* (*write_key)(uint8_t* writeto, const e::slice& elem),
          uint8_t* (*write_val)(uint8_t* writeto, const e::slice& elem),
          uint8_t* (*apply_pod)(const e::slice& old_value,
                                const microop* ops, size_t num_ops,
                                uint8_t* writeto, microerror* error),
          hyperdatatype container, hyperdatatype keyt, hyperdatatype valt,
          const e::slice& old_value,
          const microop* ops, size_t num_ops,
          uint8_t* writeto, microerror* error)
{
    std::map<e::slice, e::slice> map;
    const uint8_t* ptr = old_value.data();
    const uint8_t* end = old_value.data() + old_value.size();
    e::slice key;
    e::slice val;

    while (ptr < end)
    {
        if (!step_key(&ptr, end, &key))
        {
            *error = MICROERR_MALFORMED;
            return NULL;
        }

        if (!step_val(&ptr, end, &val))
        {
            *error = MICROERR_MALFORMED;
            return NULL;
        }

        map.insert(std::make_pair(key, val));
    }

    e::array_ptr<e::array_ptr<uint8_t> > scratch;
    scratch = new e::array_ptr<uint8_t>[num_ops];

    for (size_t i = 0; i < num_ops; ++i)
    {
        switch (ops[i].action)
        {
            case OP_SET:
                if (ops[i].arg1_datatype == HYPERDATATYPE_MAP_GENERIC &&
                    ops[i].arg1.size() == 0)
                {
                    map.clear();
                    continue;
                }
                else if (ops[i].arg1_datatype == HYPERDATATYPE_MAP_GENERIC)
                {
                    *error = MICROERR_MALFORMED;
                    return NULL;
                }

                if (container != ops[i].arg1_datatype)
                {
                    *error = MICROERR_WRONGTYPE;
                    return NULL;
                }

                map.clear();
                ptr = ops[i].arg1.data();
                end = ops[i].arg1.data() + ops[i].arg1.size();

                while (ptr < end)
                {
                    if (!step_key(&ptr, end, &key))
                    {
                        *error = MICROERR_MALFORMED;
                        return NULL;
                    }

                    if (!step_val(&ptr, end, &val))
                    {
                        *error = MICROERR_MALFORMED;
                        return NULL;
                    }

                    map.insert(std::make_pair(key, val));
                }

                break;
            case OP_MAP_ADD:
                if (keyt != ops[i].arg2_datatype)
                {
                    *error = MICROERR_WRONGTYPE;
                    return NULL;
                }

                if (!validate_key(ops[i].arg2))
                {
                    *error = MICROERR_MALFORMED;
                    return NULL;
                }

                if (valt != ops[i].arg1_datatype)
                {
                    *error = MICROERR_WRONGTYPE;
                    return NULL;
                }

                if (!validate_val(ops[i].arg1))
                {
                    *error = MICROERR_MALFORMED;
                    return NULL;
                }

                map[ops[i].arg2] = ops[i].arg1;
                break;
            case OP_MAP_REMOVE:
                if (keyt != ops[i].arg2_datatype)
                {
                    *error = MICROERR_WRONGTYPE;
                    return NULL;
                }

                if (!validate_key(ops[i].arg2))
                {
                    *error = MICROERR_MALFORMED;
                    return NULL;
                }

                map.erase(ops[i].arg2);
                break;
            case OP_STRING_APPEND:
            case OP_STRING_PREPEND:
            case OP_NUM_ADD:
            case OP_NUM_SUB:
            case OP_NUM_MUL:
            case OP_NUM_DIV:
            case OP_NUM_MOD:
            case OP_NUM_AND:
            case OP_NUM_OR:
            case OP_NUM_XOR:
                if (keyt != ops[i].arg2_datatype)
                {
                    *error = MICROERR_WRONGTYPE;
                    return NULL;
                }

                if (!validate_key(ops[i].arg2))
                {
                    *error = MICROERR_MALFORMED;
                    return NULL;
                }

                if (!apply_map_microop(apply_pod, &map, &scratch[i], ops + i, error))
                {
                    return NULL;
                }

                break;
            case OP_FAIL:
            case OP_LIST_LPUSH:
            case OP_LIST_RPUSH:
            case OP_SET_ADD:
            case OP_SET_REMOVE:
            case OP_SET_INTERSECT:
            case OP_SET_UNION:
            default:
                *error = MICROERR_WRONGACTION;
                return NULL;
        }
    }

    std::vector<std::pair<e::slice, e::slice> > v(map.begin(), map.end());
    std::sort(v.begin(), v.end(), compare_pair_less);

    for (size_t i = 0; i < v.size(); ++i)
    {
        writeto = write_key(writeto, v[i].first);
        writeto = write_val(writeto, v[i].second);
    }

    return writeto;
}

// This wrapper is needed because "apply_string operates on string attributes,
// which do not encode the size before the string because every attribute has an
// implicit "size" argument.  However, when applying the string to something in
// a map, we need to also encode the size.
uint8_t*
wrap_apply_string(const e::slice& old_value,
                  const microop* ops, size_t num_ops,
                  uint8_t* writeto, microerror* error)
{
    uint8_t* original_writeto = writeto;
    writeto = apply_string(old_value, ops, num_ops, writeto + sizeof(uint32_t), error);
    e::pack32le(static_cast<uint32_t>(writeto - original_writeto - sizeof(uint32_t)), original_writeto);
    return writeto;
}

template<bool (*compare_less)(const e::slice& lhs, const e::slice& rhs)>
static bool
cmp_pair_first(const std::pair<e::slice, e::slice>& lhs,
               const std::pair<e::slice, e::slice>& rhs)
{
    return compare_less(lhs.first, rhs.first);
}

#define APPLY_MAP(KEY_T, VAL_T, KEY_TC, VAL_TC, WRAP_PREFIX) \
    uint8_t* \
    apply_map_ ## KEY_T ## _ ## VAL_T(const e::slice& old_value, \
                           const microop* ops, size_t num_ops, \
                           uint8_t* writeto, microerror* error) \
    { \
        return apply_map(step_ ## KEY_T, step_ ## VAL_T, \
                         validate_as_ ## KEY_T, validate_as_ ## VAL_T, \
                         cmp_pair_first<compare_ ## KEY_T>, \
                         write_ ## KEY_T, write_ ## VAL_T, \
                         apply_ ## VAL_T, \
                         HYPERDATATYPE_MAP_ ## KEY_TC ## _ ## VAL_TC, \
                         HYPERDATATYPE_ ## KEY_TC, \
                         HYPERDATATYPE_ ## VAL_TC, \
                         old_value, ops, num_ops, writeto, error); \
    }

APPLY_MAP(string, string, STRING, STRING, wrap_apply_)
APPLY_MAP(string, int64, STRING, INT64, apply_)
APPLY_MAP(string, float, STRING, FLOAT, apply_)
APPLY_MAP(int64, string, INT64, STRING, wrap_apply_)
APPLY_MAP(int64, int64, INT64, INT64, apply_)
APPLY_MAP(int64, float, INT64, FLOAT, apply_)
APPLY_MAP(float, string, FLOAT, STRING, wrap_apply_)
APPLY_MAP(float, int64, FLOAT, INT64, apply_)
APPLY_MAP(float, float, FLOAT, FLOAT, apply_)
