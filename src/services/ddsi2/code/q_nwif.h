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
#ifndef Q_NWIF_H
#define Q_NWIF_H

#include "os_socket.h"
#include "c_base.h"
#include "q_protocol.h" /* for nn_locator_t */

#if defined (__cplusplus)
extern "C" {
#endif

#define INET6_ADDRSTRLEN_EXTENDED (INET6_ADDRSTRLEN + 8) /* 13: '[' + ']' + ':' + PORT */
#define MAX_INTERFACES 128
struct nn_interface {
  os_sockaddr_storage addr;
  os_sockaddr_storage netmask;
  os_uint if_index;
  unsigned mc_capable: 1;
  unsigned point_to_point: 1;
  char *name;
};

struct nn_group_membership;

void nn_loc_to_address (os_sockaddr_storage *dst, const nn_locator_t *src);
void nn_address_to_loc (nn_locator_t *dst, const os_sockaddr_storage *src, os_int32 kind);

char *sockaddr_to_string_no_port (char addrbuf[INET6_ADDRSTRLEN_EXTENDED], const os_sockaddr_storage *src);
char *sockaddr_to_string_with_port (char addrbuf[INET6_ADDRSTRLEN_EXTENDED], const os_sockaddr_storage *src);
char *locator_to_string_no_port (char addrbuf[INET6_ADDRSTRLEN_EXTENDED], const nn_locator_t *loc);
char *locator_to_string_with_port (char addrbuf[INET6_ADDRSTRLEN_EXTENDED], const nn_locator_t *loc);
void print_sockerror (const char *msg);
int make_socket (os_socket *socket, unsigned short port, c_bool stream, c_bool reuse);
int find_own_ip (const char *requested_address);
unsigned short get_socket_port (os_socket socket);
struct nn_group_membership *new_group_membership (void);
void free_group_membership (struct nn_group_membership *mship);
int join_mcgroups (struct nn_group_membership *mship, os_socket socket, const os_sockaddr_storage *srcip, const os_sockaddr_storage *mcip);
int leave_mcgroups (struct nn_group_membership *mship, os_socket socket, const os_sockaddr_storage *srcip, const os_sockaddr_storage *mcip);
void sockaddr_set_port (os_sockaddr_storage *addr, unsigned short port);
unsigned short sockaddr_get_port (const os_sockaddr_storage *addr);

#if defined (__cplusplus)
}
#endif


#endif /* Q_NWIF_H */

/* SHA1 not available (unoffical build.) */
