/*
 *  com_error.c
 *
 *  Created by Martin Louis on 9/08/06.
 *  Copyright 2006 Romteck. All rights reserved.
 *
 */


#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>

////////////////////////////////////////////////////////////////////////////////
// Logs a connetion error.

const char *com_iface_error_string(int resolve)
{
	switch (resolve ? h_errno : errno)
	{
		case HOST_NOT_FOUND:		return "Host not found.";
		case TRY_AGAIN:				break;	// Not considered an error
		case NO_RECOVERY:			return "Non-recoverable error.";
		case NO_DATA:				return "Host is up but does not have an address.";
		case EBADF:
		case ENOTSOCK:				return "Bad socket descriptor.";
		case EADDRNOTAVAIL:			return "Address not available.";
		case EAFNOSUPPORT:			return "Address family not supported.";
		case EISCONN:				return "Socket already connected.";
		case ETIMEDOUT:				return "Connection timed out.";
		case ECONNREFUSED:			return "Connection refused.";
		case ENETUNREACH:			return "Network is unreachable.";
		case EADDRINUSE:			return "Address already in use.";
		case EFAULT:				return "Page fault in name parameter.";
		case EINPROGRESS:
		case EALREADY:				break;	// Should never happen since the socket is blocking
		case EACCES:				return "Destination is braodcast address.";
		case EOPNOTSUPP:			return "Operation not supported.";
		case EWOULDBLOCK:			return "Non-blocking socket would block.";
		case EMFILE:				return "Per-process descriptor table is full.";
		case ENFILE:				return "System file table is full.";
		default:					return "Undocumented error.";
	}

	return "";
}
