/*
 *                         OpenSplice DDS
 *
 *   This software and documentation are Copyright 2006 to 2009 PrismTech 
 *   Limited and its licensees. All rights reserved. See file:
 *
 *                     $OSPL_HOME/LICENSE 
 *
 *   for full copyright notice and license terms. 
 *
 */
/* Interface */
#include "nw_socket.h"
#include "nw__socket.h"

/* Implementation */
#include <string.h>       /* for memcmp and memset          */
#include "os_heap.h"
#include "os_socket.h"

/* Descendants */
#include "nw_socketBroadcast.h"
#include "nw_socketMulticast.h"
#include "nw_socketLoopback.h"

#include "nw__plugDataBuffer.h" /* for timestamp */
#include "nw__confidence.h"
#include "nw_socketMisc.h"
#include "nw_socketPartitions.h"
#include "nw_configuration.h"
#include "nw_profiling.h"
#include "nw_misc.h" /* for nw_stringDup and nw_dumpToString */
#include "nw_report.h"

#define SK_CONTROLPORT(portNr) ((portNr)+1)

/* ------------------------------ Debug function ---------------------------- */

#define NW_HEXDUMP(message, partitionId, data, length)    \
        NW_TRACE_3(Test, 6, "%s: %u bytes to partitionId %u\n", message, length, partitionId);


/* ------------------------------- main class ------------------------------- */

/* The structure of the data, to be extended by descendants */

NW_STRUCT(nw_socket) {
    char *name;
    /* Data socket, for sending and receiving data */
    os_socket socketData;
    /* Primary address, for destination checking */
    struct sockaddr_in sockAddrPrimary;
    /* Broadcast address corresponding to this interface */
    struct sockaddr_in sockAddrBroadcast;
    /* Default address to send data to */
    struct sockaddr_in sockAddrData;
    /* Control address, for sending and receiving protocol control messages */
    struct sockaddr_in sockAddrControl;
    /* Parameters idenfitifying socket for control messages (acks etc) */
    sk_bool supportsControl;
    os_socket socketControl;
    /* Caching for select statement */
    fd_set sockSet;
    int maxSockfd;
    /* Socket state and settings */
    int loopsback;
    /* List of alternative addresses for sending data */
    nw_socketPartitions partitions;
};



/* ---------------------------- getters/setters ----------------------------- */

sk_bool
nw_socketLoopsback(
    nw_socket sock)
{
    return sock->loopsback;
}


sk_address
nw_socketPrimaryAddress(
    nw_socket sock)
{
    sk_address result;

    NW_CONFIDENCE(sizeof(sock->sockAddrPrimary.sin_addr) == sizeof(sk_address));

    if (sock) {
        result = *((sk_address *)&sock->sockAddrPrimary.sin_addr);
    } else {
        result = 0;
    }

    return result;
}

sk_address
nw_socketBroadcastAddress(
    nw_socket sock)
{
    sk_address result;

    NW_CONFIDENCE(sizeof(sock->sockAddrBroadcast.sin_addr) == sizeof(sk_address));

    if (sock) {
        result = *((sk_address *)&sock->sockAddrBroadcast.sin_addr);
    } else {
        result = 0;
    }

    return result;
}

sk_address
nw_socketDataAddress(
    nw_socket sock)
{
    sk_address result;

    NW_CONFIDENCE(sizeof(sock->sockAddrData.sin_addr) == sizeof(sk_address));

    if (sock) {
        result = *((sk_address *)&sock->sockAddrData.sin_addr);
    } else {
        result = 0;
    }

    return result;
}

int
nw_socketSetSendBufferSize(
    nw_socket sock,
    int bufSize)
{
    int result = SK_TRUE;
    socklen_t optLen;
    os_result retVal;

    if (sock != NULL) {
        /* Set sendbuffer option */
        optLen = (os_uint32)sizeof(bufSize);

        retVal = os_sockSetsockopt(sock->socketData,
                            SOL_SOCKET, SO_SNDBUF,
                            (void *)&bufSize, optLen);
        SK_REPORT_SOCKFUNC(2, retVal,
                           "set socket sendbuffer size", "setsockopt");

        if ((retVal == os_resultSuccess) && (sock->supportsControl)) {
            retVal = os_sockSetsockopt(sock->socketControl,
                                SOL_SOCKET, SO_SNDBUF,
                                (void *)&bufSize, optLen);
            SK_REPORT_SOCKFUNC(2, retVal,
                               "set socket sendbuffer size", "setsockopt");
        }
        if (retVal != os_resultSuccess) {
            result = SK_FALSE;
        }
    } else {
        result = SK_FALSE;
    }

    return result;
}


int
nw_socketSetReceiveBufferSize(
    nw_socket sock,
    int bufSize)
{
    int result = SK_TRUE;
    socklen_t optLen;
    int actualSize;
    os_result retVal;

    if (sock != NULL) {
        /* Set receivebuffer option */
        optLen = (socklen_t)sizeof(bufSize);
        retVal = os_sockSetsockopt(sock->socketData,
                            SOL_SOCKET, SO_RCVBUF,
                            (const void *)&bufSize, optLen);
        SK_REPORT_SOCKFUNC(2, retVal,
                           "set socket receivebuffer size", "setsockopt");

        if (retVal == os_resultSuccess) {
            /* The following lines are for tracing purposes only */
            actualSize = 0;
            retVal = os_sockGetsockopt(sock->socketData, SOL_SOCKET, SO_RCVBUF,
                (void *)&actualSize, &optLen);
            SK_REPORT_SOCKFUNC(4, retVal,
                               "get socket receivebuffer size", "getsockopt");
            NW_TRACE_2(Receive, 5, "Receive buffer size set. Requested: %d, actual: %d",
                bufSize, actualSize);

            if ((retVal == os_resultSuccess) && (sock->supportsControl)) {
                retVal = os_sockSetsockopt(sock->socketControl,
                                    SOL_SOCKET, SO_RCVBUF,
                                    (const void *)&bufSize, optLen);
                SK_REPORT_SOCKFUNC(2, retVal,
                                   "set socket receivebuffer size", "setsockopt");
            }
        }
        if (retVal != os_resultSuccess) {
            result = SK_FALSE;
        }
    } else {
        result = SK_FALSE;
    }

    return result;
}


int
nw_socketSetBroadcastOption(
    nw_socket sock,
    int enableBroadcast)
{
    int result = SK_TRUE;
    socklen_t optLen;
    os_result retVal;

    if (sock != NULL) {
        /* Set broadcast option */
        optLen = (socklen_t)sizeof(enableBroadcast);
        retVal = os_sockSetsockopt(sock->socketData,
                            SOL_SOCKET, SO_BROADCAST,
                            (const void *)&enableBroadcast, optLen);
        SK_REPORT_SOCKFUNC(2, retVal,
                           "set socket broadcast option", "setsockopt");
        if ((retVal == os_resultSuccess) && (sock->supportsControl)) {
            retVal = os_sockSetsockopt(sock->socketControl,
                                SOL_SOCKET, SO_BROADCAST,
                                (const void *)&enableBroadcast, optLen);
            SK_REPORT_SOCKFUNC(2, retVal,
                               "set socket broadcast option", "setsockopt");
        }
        if (retVal != os_resultSuccess) {
            result = SK_FALSE;
        }
    } else {
        result = SK_FALSE;
    }

    return result;
}

int
nw_socketSetTOS(
    nw_socket sock,
    int tos)
{
    int result = SK_TRUE;
    socklen_t optLen;
    os_result retVal;

    if (sock != NULL) {
        /* Set broadcast option */
        optLen = (socklen_t)sizeof(tos);
        retVal = os_sockSetsockopt(sock->socketData,
                            IPPROTO_IP, IP_TOS,
                            (const void *)&tos, optLen);
        SK_REPORT_SOCKFUNC(2, retVal,
                           "set socket type of service", "setsockopt");
        if ((retVal == os_resultSuccess) && (sock->supportsControl)) {
            retVal = os_sockSetsockopt(sock->socketControl,
                                IPPROTO_IP, IP_TOS,
                                (const void *)&tos, optLen);
            SK_REPORT_SOCKFUNC(2, retVal,
                               "set socket type of service", "setsockopt");
        }
        if (retVal != os_resultSuccess) {
            result = SK_FALSE;
        }
    } else {
        result = SK_FALSE;
    }

    return result;
}


int
nw_socketSetDontRouteOption(
    nw_socket sock,
    int disableRouting)
{
    int result = SK_TRUE;
    socklen_t optLen;
    os_result retVal;

    if (sock != NULL) {
        /* Set broadcast option */
        optLen = (socklen_t)sizeof(disableRouting);
        retVal = os_sockSetsockopt(sock->socketData,
                            SOL_SOCKET, SO_DONTROUTE,
                            (const void *)&disableRouting, optLen);
        SK_REPORT_SOCKFUNC(2, retVal,
                           "set socket dontroute option", "setsockopt");
        if ((retVal == os_resultSuccess) && (sock->supportsControl)){
            retVal = os_sockSetsockopt(sock->socketControl,
                                SOL_SOCKET, SO_DONTROUTE,
                                (const void *)&disableRouting, optLen);
            SK_REPORT_SOCKFUNC(2, retVal,
                               "set socket dontroute option", "setsockopt");
        }
        if (retVal != os_resultSuccess) {
            result = SK_FALSE;
        }
    } else {
        result = SK_FALSE;
    }

    return result;
}

sk_bool
nw_socketGetDataSocket(
    nw_socket sock,
    os_socket *socket)
{
    sk_bool result = SK_FALSE;

    NW_CONFIDENCE(sock != NULL);
    NW_CONFIDENCE(socket != NULL);

    if ((sock != NULL) && (socket != NULL)) {
        *socket = sock->socketData;
        result = SK_TRUE;
    }

    return result;
}

sk_bool
nw_socketGetControlSocket(
    nw_socket sock,
    os_socket *socket)
{
    sk_bool result = SK_FALSE;

    NW_CONFIDENCE(sock != NULL);
    NW_CONFIDENCE(socket != NULL);

    if ((sock != NULL) && (socket != NULL)) {
        result = sock->supportsControl;
        if (result) {
             *socket = sock->socketControl;
        }
    }

    return result;
}

int
nw_socketBind(
    nw_socket sock)
{
    int result = SK_TRUE;
    os_result retVal = os_resultSuccess;
    int optVal;
    socklen_t optLen;
    struct sockaddr_in bindAddress;

    if (sock != NULL) {
        /* Avoid already in use error messages */
        optLen = (os_uint32)sizeof(optVal);
        optVal = 0;
        retVal = os_sockSetsockopt(sock->socketData,
                            SOL_SOCKET, SO_REUSEADDR,
                            (void *)&optVal, optLen);
        SK_REPORT_SOCKFUNC(2, retVal,
                           "set socket reuse option", "setsockopt");

        bindAddress = sock->sockAddrData;
        bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
        if (retVal == os_resultSuccess) {
            retVal = os_sockBind(sock->socketData,
                (const struct sockaddr *)&bindAddress,
                (socklen_t)sizeof(bindAddress));
/*          NOTE: This used to be:
 *          retVal = os_sockBind(sock->socketData,
 *                (const struct sockaddr *)&sock->sockAddrData,
 *                (socklen_t)sizeof(sock->sockAddrData)); */
            SK_REPORT_SOCKFUNC(2, retVal,
                           "bind socket", "bind");

            if ((retVal == os_resultSuccess) && (sock->supportsControl)) {
                retVal = os_sockSetsockopt(sock->socketControl,
                                SOL_SOCKET, SO_REUSEADDR,
                                (void *)&optVal, optLen);
                SK_REPORT_SOCKFUNC(2, retVal,
                               "set socket reuse option", "setsockopt");

                bindAddress = sock->sockAddrControl;
                bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
                if (retVal == os_resultSuccess) {
                    retVal = os_sockBind(sock->socketControl,
                        (const struct sockaddr *)&bindAddress,
                        (socklen_t)sizeof(bindAddress));
/*                  NOTE: This used to be:
 *                  retVal = os_sockBind(sock->socketControl,
 *                    (const struct sockaddr *)&sock->sockAddrControl,
 *                    (socklen_t)sizeof(sock->sockAddrControl)); */
                    SK_REPORT_SOCKFUNC(2, retVal,
                                   "bind socket", "bind");
                }
            }
        }
        if (retVal != os_resultSuccess) {
            result = SK_FALSE;
        }
    } else {
       result = SK_FALSE;
    }

    return result;
}



void
nw_socketAddPartition(
    nw_socket sock,
    sk_partitionId partitionId,
    const char *addressString,
    sk_bool connected)
{
    sk_address address;

    if (strcmp(addressString, NWCF_BROADCAST_EXPR) == 0) {
        address = nw_socketBroadcastAddress(sock);
    } else {
        address = sk_stringToAddress(addressString, NWCF_DEF(Address));
    }
    nw_socketPartitionsAdd(sock->partitions, partitionId, address, connected);

    /* Do any multicast related actions if needed */
    if (partitionId != 0 && connected) {
        /* No need to add first the default partition,
           that already happened with socket initialization */
        nw_socketMulticastAddPartition(sock, addressString);
    }
}


int
nw_socketPrimaryAddressCompare(
    nw_socket sock,
    sk_address toCompare)
{
    static sk_address zeroAddress = 0;
    int result = 0;

    NW_CONFIDENCE(sizeof(sock->sockAddrPrimary.sin_addr) == sizeof(sk_address));

    if (sock) {
        /* First check 'wildcard' (zeroes only) */
        result = (toCompare == zeroAddress);
        if (!result) {
            /* No wildcard, then compare bytes */
            result = (toCompare == *((sk_address *)&sock->sockAddrPrimary.sin_addr));
        }
    }

    return result;
}

/* ------------------------------- constructor ------------------------------ */


/* private */

static nw_socket
nw_socketNew(
    nw_bool receiving,
    const char *defaultAddress,
    sk_portNr portNr,
    nw_bool supportsControl,
    const char *name)
{
    nw_socket result = NULL;
    int success = SK_TRUE;
    char *addressLookingFor;
    sk_addressType addressType;
    sk_portNr portNrControl = SK_CONTROLPORT(portNr);
    c_ulong bufSizeRequested;
    c_ulong TOSRequested;

    result = (nw_socket)os_malloc((os_uint32)sizeof(*result));
    if (result != NULL) {
        result->name = nw_stringDup(name);
        result->supportsControl = supportsControl;
        result->socketData = os_sockNew(AF_INET, SOCK_DGRAM);
        SK_REPORT_SOCKFUNC(2, os_resultSuccess,
                           "socket creation", "socket");
        if (supportsControl) {
            result->socketControl = os_sockNew(AF_INET, SOCK_DGRAM);
            SK_REPORT_SOCKFUNC(2, os_resultSuccess,
                               "socket creation", "socket");
            
            if (result->socketData > result->socketControl) {
                result->maxSockfd = result->socketData;
            } else {
                result->maxSockfd = result->socketControl;
            }
        } else {
            result->maxSockfd = result->socketData;
        }
        FD_ZERO(&result->sockSet);

        addressLookingFor = NWCF_SIMPLE_PARAM(String, NWCF_ROOT(General), Interface);
        addressType = sk_getAddressType(defaultAddress);
        switch (addressType) {
            case SK_TYPE_BROADCAST:
                nw_socketGetDefaultBroadcastInterface(addressLookingFor,
                    result->socketData, &result->sockAddrPrimary,
                    &result->sockAddrBroadcast);
                result->sockAddrData = result->sockAddrBroadcast;
                /* Broadcast sockets usually loop back and can not be stopped
                 * from that */
                result->loopsback = SK_TRUE;
                NW_TRACE_1(Test, 4, "Using broadcast address %s for default partition",
                    inet_ntoa(result->sockAddrData.sin_addr));
            break;
            case SK_TYPE_MULTICAST:
                nw_socketGetDefaultMulticastInterface(addressLookingFor,
                    result->socketData, &result->sockAddrPrimary,
                    &result->sockAddrBroadcast);
                result->sockAddrData.sin_addr.s_addr = sk_stringToAddress(
                    defaultAddress, NWCF_DEF(Address));
                /* We can stop multicasting from looping back though */
                result->loopsback = SK_FALSE;
            break;
            case SK_TYPE_LOOPBACK:
                nw_socketGetDefaultLoopbackAddress(result->socketData,
                    &result->sockAddrPrimary);
                result->sockAddrData = result->sockAddrPrimary;
                result->sockAddrBroadcast = result->sockAddrPrimary;
                result->loopsback = SK_TRUE;
            break;
            default:
            break;
        }
        os_free(addressLookingFor);

        result->sockAddrPrimary.sin_family = AF_INET;
        result->sockAddrPrimary.sin_port = htons(0); /* Don't care */
        result->sockAddrData.sin_family = AF_INET;
        result->sockAddrData.sin_port = htons(portNr);
        result->sockAddrControl = result->sockAddrData;
        result->sockAddrControl.sin_port = htons(portNrControl);
        result->partitions = nw_socketPartitionsNew();

        if (receiving) {
            /* Set option to avoid sendbuffer */
            success = success && nw_socketSetSendBufferSize(result, 0);
            /* Set option for custom receive buffer size */
            bufSizeRequested = NWCF_SIMPLE_SUBPARAM(ULong, name, Rx, ReceiveBufferSize);
            success = success && nw_socketSetReceiveBufferSize(result, (int)bufSizeRequested);

            /* Bind to socket */
            success = success && nw_socketBind(result);

            if (success) {
                if (!supportsControl) {
                    NW_REPORT_INFO_3(2, "Created and bound receiving socket \"%s\" "
                                        "for interface %s, port %u",
                                     name,
                                     inet_ntoa(result->sockAddrPrimary.sin_addr), portNr);
                } else {
                    NW_REPORT_INFO_4(2, "Created and bound receiving socket \"%s\" "
                                        "for interface %s, ports %u and %u",
                                     name,
                                     inet_ntoa(result->sockAddrPrimary.sin_addr),
                                     portNr, portNr+1);
                }
                NW_TRACE_1(Test, 1, "Creation and binding of receiving "
                                  "multicast socket \"%s\" succeeded.",
                                  name);
            }
        } else {
            /* Set option to avoid receivebuffer */
            success = success && nw_socketSetReceiveBufferSize(result, 0);
            /* Set option for avoiding routing to other interfaces */
#if ! defined OS_VXWORKS_DEFS_H && ! defined INTEGRITY
            success = success && nw_socketSetDontRouteOption(result, SK_TRUE);
#endif
            /* Set option for custom TOS */
            TOSRequested = NWCF_SIMPLE_SUBPARAM(ULong, name, Tx, DiffServField);
            success = success && nw_socketSetTOS(result, (int)TOSRequested);


            if (success) {
                    NW_REPORT_INFO_3(2, "Created sending socket \"%s\"for "
                                        "interface %s, port %u",
                                     name,
                                     inet_ntoa(result->sockAddrPrimary.sin_addr), portNr);
            } else {
                    NW_REPORT_INFO_4(2, "Created sending socket \"%s\"for "
                                        "interface %s, ports %u and %u",
                                     name,
                                     inet_ntoa(result->sockAddrPrimary.sin_addr),
                                     portNr, portNr+1);
            }
            NW_TRACE_1(Test, 1, "Creation of sending multicast socket \"%s\" succeeded.",
                              name);
        }

        switch (addressType) {
            case SK_TYPE_BROADCAST: nw_socketBroadcastInitialize(result); break;
            case SK_TYPE_MULTICAST: nw_socketMulticastInitialize(result); break;
            default: break;
        }

    }

    return result;
}


/* public */

nw_socket
nw_socketSendNew(
    const char *defaultAddress,
    sk_portNr portNr,
    sk_bool supportsControl,
    const char *pathName)
{
    return nw_socketNew(SK_FALSE, defaultAddress, portNr, supportsControl,
        pathName);
}


/* public */

nw_socket
nw_socketReceiveNew(
    const char *defaultAddress,
    sk_portNr portNr,
    sk_bool supportsControl,
    const char *pathName)
{
    return nw_socketNew(SK_TRUE, defaultAddress, portNr, supportsControl,
        pathName);
}


/* -------------------------------- destructor ------------------------------ */

void
nw_socketFree(
    nw_socket sock)
{
    os_result retVal;

    if (sock) {
        retVal = os_sockFree(sock->socketData);
        SK_REPORT_SOCKFUNC(2, retVal,
            "release socket resources", "close");
        if ((retVal == os_resultSuccess) && (sock->supportsControl)) {
            retVal = os_sockFree(sock->socketControl);
            SK_REPORT_SOCKFUNC(2, retVal,
                "release socket resources", "close");
        }
        os_free(sock->name);
        os_free(sock);
    }
    /* Not interested in any result */
    /* return result */
}


/* ------------------------------- public methods --------------------------- */

sk_length
nw_socketSendData(
    nw_socket sock,
    void *buffer,
    sk_length length)
{
    sk_length result = 0;
    os_int32 sendRes;
    int sendToSucceeded;

    NW_CONFIDENCE(sock != NULL);

    NW_HEXDUMP("nw_socketSendData", 0, buffer, length);
    /* Do the writing */
    NW_PROF_LAPSTART(SendTo);
    sendRes = os_sockSendto(sock->socketData, buffer, length,
                     (const struct sockaddr *)&sock->sockAddrData,
                     (socklen_t)sizeof(sock->sockAddrData)
                     );
    NW_PROF_LAPSTOP(SendTo);

    if (sendRes > 0) {
        SK_REPORT_SOCKFUNC(6, os_resultSuccess,
                           "sending data to the socket", "sendto");
        sendToSucceeded = SK_TRUE;
    } else {
        SK_REPORT_SOCKFUNC(6, os_resultFail,
                           "sending data to the socket", "sendto");
        sendToSucceeded = SK_FALSE;
    }

    if (sendToSucceeded) {
        result = (sk_length)sendRes;      
    }

    return result;
}


sk_length
nw_socketSendDataTo(
    nw_socket sock,
    sk_address receiverAddress,
    void *buffer,
    sk_length length)
{
    sk_length result = 0;
    os_int32 sendRes;
    int sendToSucceeded;
    struct sockaddr_in sockAddrP2P;

    NW_CONFIDENCE(sock != NULL);
    NW_CONFIDENCE(sizeof(receiverAddress) == sizeof(sockAddrP2P.sin_addr));
    
    NW_HEXDUMP("nw_socketSendDataTo", 0, buffer, length);
    /* Do the writing */
    NW_PROF_LAPSTART(SendTo);
    sockAddrP2P = sock->sockAddrData;
    sockAddrP2P.sin_addr.s_addr = (in_addr_t)receiverAddress;
    sendRes = os_sockSendto(sock->socketData, buffer, length,
                     (const struct sockaddr *)&sockAddrP2P,
                     (socklen_t)sizeof(sockAddrP2P)
                     );
    NW_PROF_LAPSTOP(SendTo);

    if (sendRes > 0) {
        SK_REPORT_SOCKFUNC(6, os_resultSuccess,
                               "sending data to the socket", "sendto");
        sendToSucceeded = SK_TRUE;
    } else {
        SK_REPORT_SOCKFUNC(6, os_resultFail,
                               "sending data to the socket", "sendto");
        sendToSucceeded = SK_FALSE;
    }

    if (sendToSucceeded) {
        result = (sk_length)sendRes;    
    }

    return result;
}

sk_length
nw_socketSendDataToPartition(
    nw_socket sock,
    sk_partitionId partitionId,
    void *buffer,
    sk_length length)
{
    sk_length result = 0;
    os_int32 sendRes;
    int sendToSucceeded;
    sk_address partitionAddress;
    struct sockaddr_in sockAddrForPartition;
    nw_bool found;

    NW_CONFIDENCE(sock != NULL);

    NW_HEXDUMP("nw_socketSendDataToPartition", partitionId, buffer, length);
    /* Do the writing */
    NW_PROF_LAPSTART(SendTo);

    found = nw_socketPartitionsLookup(sock->partitions, partitionId,
        &partitionAddress);
    NW_CONFIDENCE(found);

    sockAddrForPartition = sock->sockAddrData;
    sockAddrForPartition.sin_addr.s_addr = (in_addr_t)partitionAddress;

    NW_STAMP(nw_plugDataBuffer(buffer),NW_BUF_TIMESTAMP_SEND);

    sendRes = os_sockSendto(sock->socketData, buffer, length,
                     (const struct sockaddr *)&sockAddrForPartition,
                     (socklen_t)sizeof(sockAddrForPartition)
                     );
    NW_PROF_LAPSTOP(SendTo);

    if (sendRes > 0) {
        SK_REPORT_SOCKFUNC(5, os_resultSuccess,
                               "sending data to the socket", "sendto");
        sendToSucceeded = SK_TRUE;
    } else {
        SK_REPORT_SOCKFUNC(1, os_resultFail,
                               "sending data to the socket", "sendto");
        sendToSucceeded = SK_FALSE;
    }

    if (sendToSucceeded) {
        result = (sk_length)sendRes;       
    }

    return result;
}


sk_length
nw_socketSendControl(
    nw_socket sock,
    void *buffer,
    sk_length length)
{
    sk_length result = 0;
    os_int32 sendRes;
    int sendToSucceeded;

    NW_CONFIDENCE(sock != NULL);

    /* Control message */
    NW_CONFIDENCE(sock->supportsControl);
    NW_HEXDUMP("nw_socketSendControl", 0, buffer, length);
    sendRes = os_sockSendto(sock->socketControl, buffer, length,
                     (const struct sockaddr *)&sock->sockAddrControl,
                     (socklen_t)sizeof(sock->sockAddrControl)
                     );
    if (sendRes > 0) {
        SK_REPORT_SOCKFUNC(6, os_resultSuccess,
                       "sending control message to the socket", "sendto");
        sendToSucceeded = SK_TRUE;
    } else {
        SK_REPORT_SOCKFUNC(6, os_resultFail,
                       "sending control message to the socket", "sendto");
        sendToSucceeded = SK_FALSE;
    }

    if (sendToSucceeded) {
        result = (sk_length)sendRes;
    }

    return result;
}

sk_length
nw_socketSendControlTo(
    nw_socket sock,
    sk_address receiverAddress,
    void *buffer,
    sk_length length)
{
    sk_length result = 0;
    os_int32 sendRes;
    int sendToSucceeded;
    struct sockaddr_in sockAddrP2P;

    NW_CONFIDENCE(sock != NULL);
    NW_CONFIDENCE(sizeof(receiverAddress) == sizeof(sockAddrP2P.sin_addr));

    /* Control message */
    NW_CONFIDENCE(sock->supportsControl);
    NW_HEXDUMP("nw_socketSendControlTo", 0, buffer, length);
    sockAddrP2P = sock->sockAddrControl;
    sockAddrP2P.sin_addr.s_addr = (in_addr_t)receiverAddress;
    sendRes = os_sockSendto(sock->socketControl, buffer, length,
                     (const struct sockaddr *)&sockAddrP2P,
                     (socklen_t)sizeof(sockAddrP2P)
                     );

    if (sendRes > 0) {
        SK_REPORT_SOCKFUNC(6, os_resultSuccess,
                               "sending control message to the socket", "sendto");
        sendToSucceeded = SK_TRUE;
    } else {
        SK_REPORT_SOCKFUNC(6, os_resultFail,
                               "sending control message to the socket", "sendto");
        sendToSucceeded = SK_FALSE;
    }

    if (sendToSucceeded) {
        result = (sk_length)sendRes;
    }

    return result;
}


sk_length
nw_socketReceive(
    nw_socket sock,
    sk_address *senderAddress,
    void *buffer,
    sk_length length,
    os_time *timeOut)
{
    sk_length result = 0;
    os_int32 recvRes = 0;
    os_int32 selectRes;
    struct sockaddr_in sockAddr;
    int fromLen = (int)sizeof(sockAddr);
    int ownMessage;
    nw_bool readDone = FALSE;
    os_time tmpTimeOut = *timeOut;
#ifdef NW_DEBUGGING
    nw_bool control = FALSE;
#endif

    NW_CONFIDENCE(sock != NULL);
    NW_CONFIDENCE(senderAddress != NULL);

    *senderAddress = 0;
    FD_SET(sock->socketData, &sock->sockSet);
    if (sock->supportsControl) {
        FD_SET(sock->socketControl, &sock->sockSet);
    }

    /* Stop all profiling here because the select is a blocking call */
    /* Use tmpTimeOut becuase select modifies timeOut param under Linux */
    selectRes = os_sockSelect(sock->maxSockfd+1, &sock->sockSet, NULL, NULL, &tmpTimeOut);

    if (selectRes > 0) {
        if (sock->supportsControl) {
            if (FD_ISSET(sock->socketControl, &sock->sockSet)) {
                NW_CONFIDENCE(sock->supportsControl);
                recvRes = os_sockRecvfrom(sock->socketControl, buffer, length,
                                   (struct sockaddr *)&sockAddr, (socklen_t *)&fromLen);
                readDone = TRUE;
#ifdef NW_DEBUGGING
                control = TRUE;
#endif
            }
        }
        if (readDone != TRUE) {
            NW_CONFIDENCE(FD_ISSET(sock->socketData, &sock->sockSet));
            recvRes = os_sockRecvfrom(sock->socketData, buffer, length,
                               (struct sockaddr *)&sockAddr, (socklen_t *)&fromLen);
        }
        
        if (recvRes > 0) {
            if (sock->loopsback) {
#ifdef NW_LOOPBACK
                if (nw_configurationUseLoopback()) {
                    /* Loopback always simulates that data comes from the network */
                    ownMessage = SK_FALSE;
                } else {
                    ownMessage = (memcmp(&sockAddr.sin_addr,
                                         &sock->sockAddrPrimary.sin_addr,
                                         (os_uint32)sizeof(sockAddr.sin_addr)) == 0);
                }
#else
                ownMessage = (memcmp(&sockAddr.sin_addr,
                                     &sock->sockAddrPrimary.sin_addr,
                                     (os_uint32)sizeof(sockAddr.sin_addr)) == 0);
#endif
            } else {
                ownMessage = SK_FALSE;
            }
            if (!ownMessage) {
                result = (sk_length)recvRes;
                NW_CONFIDENCE(sizeof(*senderAddress) == sizeof(sockAddr.sin_addr.s_addr));
                *senderAddress = (sk_address)sockAddr.sin_addr.s_addr;
#ifdef NW_DEBUGGING
                if (control) {
                    NW_HEXDUMP("nw_socketReceiveControl", 0, buffer, result);
                } else {
                    NW_HEXDUMP("nw_socketReceiveData", 0, buffer, result);
                }
#endif
                /* Resume profiling because we have actually received something
                 * relevant */

               SK_REPORT_SOCKFUNC(6, os_resultSuccess,
                              "receiving data from socket", "recvfrom");
            }
        } else {
           SK_REPORT_SOCKFUNC(6, os_resultFail,
                              "receiving data from socket", "recvfrom");
        }
    } else {
        if (selectRes < 0) {
            SK_REPORT_SOCKFUNC(6, os_resultFail,
                           "receiving data from socket", "select");
        }
    }
    return result;
}