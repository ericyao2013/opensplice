/*
 *                         OpenSplice DDS
 *
 *   This software and documentation are Copyright 2006 to TO_YEAR PrismTech
 *   Limited, its affiliated companies and licensors. All rights reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */
#ifndef V_DATAREADERSAMPLE_H
#define V_DATAREADERSAMPLE_H

/** \file kernel/include/v_dataReaderSample.h
 *  \brief This file defines the interface
 *
 */

#include "v_kernel.h"
#include "v_message.h"
#include "v_readerSample.h"
#include "v_dataReaderEntry.h"
#include "os_if.h"

#ifdef OSPL_BUILD_CORE
#define OS_API OS_API_EXPORT
#else
#define OS_API OS_API_IMPORT
#endif
/* !!!!!!!!NOTE From here no more includes are allowed!!!!!!! */

/**
 * \brief The <code>v_dataReaderSample</code> cast method.
 *
 * This method casts an object to a <code>v_dataReaderSample</code> object.
 * Before the cast is performed, if compiled with the NDEBUG flag not set,
 * the type of the object is checked to be <code>v_dataReaderSample</code> or
 * one of its subclasses.
 */
#define  v_dataReaderSample(o)         (C_CAST(o,v_dataReaderSample))

#define  v_dataReaderSampleTemplate(o) ((v_dataReaderSampleTemplate)(o))

#define  v_dataReaderSampleState(o)    (v_readerSampleState (o))

#define v_dataReaderSample_t(scope) \
        c_class(c_resolve(c_getBase(scope), \
                          "kernelModuleI::v_dataReaderSample"))

#define v_dataReaderSampleStateTest(_this, mask) v_readerSampleTestState(_this, mask)

#define v_dataReaderSampleStateTestNot(_this, mask) v_readerSampleTestStateNot(_this, mask)

#define v_dataReaderSampleInstance(o) \
        ((v_dataReaderInstance)c_typeActualType(v_readerSampleInstance(v_dataReaderSample(o))))

#define v_dataReaderSampleIndex(o) \
        ((v_index)v_dataReaderSampleInstance(o)->index)

#define v_dataReaderSampleTopic(o) \
        (v_dataReaderSampleIndex(o)->entry ? \
         v_dataReaderEntryTopic(v_dataReaderSampleIndex(o)->entry) : NULL )

#define v_dataReaderSampleMessage(_this) \
        v_message(v_dataReaderSampleTemplate(_this)->message)

#define v_dataReaderSampleMessageStateTest(_this,_mask) \
        v_stateTest(v_nodeState(v_dataReaderSampleMessage(_this)),_mask)

#define v_dataReaderSampleInstanceStateTest(_this,_mask) \
        v_dataReaderInstanceStateTest(v_dataReaderSampleInstance(_this),_mask)

OS_API v_actionResult
v_dataReaderSampleRead(
    v_dataReaderSample _this,
    v_readerSampleAction action,
    c_voidp arg);

OS_API v_actionResult
v_dataReaderSampleTake(
    v_dataReaderSample _this,
    v_readerSampleAction action,
    c_voidp arg);

#undef OS_API

#endif
