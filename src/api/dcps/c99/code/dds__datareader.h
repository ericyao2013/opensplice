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

#ifndef DDS__DATAREADER_H
#define DDS__DATAREADER_H

#include <dds.h>

int
dds_datareader_delete(
    dds_entity_t e);

int
dds_datareader_get_listener(
    dds_entity_t e,
    dds_readerlistener_t *listener);

int
dds_datareader_set_listener(
    dds_entity_t e,
    const dds_readerlistener_t *listener);


#endif /* DDS__DATAREADER_H */
