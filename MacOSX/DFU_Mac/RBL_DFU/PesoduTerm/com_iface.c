/*
 *  com_iface.c
 *
 *  Created by Martin Louis on 9/08/06.
 *  Copyright 2006 Martin Louis. All rights reserved.
 *
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "com_iface.h"
#include "com_pterm.h"


/////////////////////////////////////////////////////////////////
// Configuration

#define DEFAULT_SERIAL_BAUD_RATE	38400	// Baud rate used when none is specified on serial
#define DEFAULT_SERIAL_DATABITS		8		// Number of data bits used when none is specified on serial
#define DEFAULT_SERIAL_PARITY		'N'		// Parity used when none is specified on serial
#define DEFAULT_SERIAL_STOPBITS		1		// Number of stop bits used when none is specified on serial
#define MAX_CONFIG_ARGS				8

#define LOCK_LIST				pthread_mutex_lock(&list_mutex)
#define UNLOCK_LIST				pthread_mutex_unlock(&list_mutex)
#define LOCK_CONN_LIST(x)		pthread_mutex_lock(&(x)->sock_server.conn_list_mutex)
#define UNLOCK_CONN_LIST(x)		pthread_mutex_unlock(&(x)->sock_server.conn_list_mutex)


/////////////////////////////////////////////////////////////////
// Public data
void (*com_iface_default_receive_handler)(com_iface_t *, void *, size_t) = NULL;
void (*com_iface_default_connect_handler)(com_iface_t *, int) = NULL;

/////////////////////////////////////////////////////////////////
// Private data
static int initialised = 0;
static com_iface_t *first_iface = 0;
static com_iface_t *last_iface  = 0;
static pthread_mutex_t	list_mutex;		// Mutex controls access to list

// Initialise the com interfaces
static int initialise(void)
{
	// Initialise the list access mutex
	initialised = (pthread_mutex_init(&list_mutex, NULL) == 0) ? 1 : 0;

	return initialised ? 0 : -1;
}

// returns 1 if argument is a valid interface pointer, 0 if not
int com_iface_is_valid(com_iface_t *iface)
{
	com_iface_t *check_iface;
	int is_valid = 0;
	
	if (!iface)
		return 0;

	LOCK_LIST;
	for (check_iface = first_iface; check_iface; check_iface = check_iface->next)
	{
		if (iface == check_iface)
		{
			is_valid = 1;
			break;
		}	
	}
	UNLOCK_LIST;
	
	return is_valid;
}

// Create a communications interface
com_iface_t *com_iface_open(void)
{
	com_iface_t *iface = NULL;

	// Initialise interfaces id not already done
	if (!initialised)
	{
		if (initialise() != 0)
		{
			printf("Failed to initialise communications interfaces\r\n");
			return NULL;
		}
	}

    iface = com_iface_pterm_open();

	return iface;
}

// Sends data through an interface
// Returns number of characters sent.
int com_iface_send(com_iface_t *iface, void *buf, ssize_t size)
{
	// Sanity check
	if (!initialised || !iface || !buf || (size <= 0))
		return 0;
    
	com_iface_pterm_send(iface, buf, size);
    
	return 0;
}

// Tells a single interfaces thread to close the interface.
// Returns 0 if successful, -1 if not.
int com_iface_close(com_iface_t *iface)
{
	// Sanity check
	if (!initialised || !iface)
		return -1;

	com_iface_pterm_close(iface);
	
	// Not recognised
	return -1;
}

// Allocates a blank interface.
com_iface_t *com_iface_alloc(com_iface_t *parent)
{
	com_iface_t *iface;

	// Initialise interfaces id not already done
	if (!initialised)
	{
		if (initialise() != 0)
		{
			printf("Failed to initialise communications interface\r\n");
			return NULL;
		}
	}

	// Allocate interface
	if ((iface = (com_iface_t *)malloc(sizeof(com_iface_t))) == 0)
	{
		printf("Failed to allocate communications interface\r\n");
		return NULL;
	}

	// Allocate RX buffer
	if ((iface->rx_buf = (unsigned char *)malloc(sizeof(unsigned char) * COM_IFACE_RX_BUF_SIZE)) == 0)
	{
		free(iface);
		printf("Failed to allocate communications interface.\n");
		return NULL;
	}

	iface->name = NULL;
	iface->parent = parent;
	iface->close_request  = 0;
	iface->thread_running = 0;
	iface->connections = 0;
	iface->receive_handler  = parent ? parent->receive_handler : com_iface_default_receive_handler;
	iface->connection_handler  = parent ? parent->connection_handler : com_iface_default_connect_handler;
	
	if (parent)
	{
		iface->tag = parent->tag;
		iface->user_data = parent->user_data;
	}

	// Add to list
	LOCK_LIST;
	iface->next = 0;
	iface->prev = last_iface;
	last_iface = iface;
	if (iface->prev)
		iface->prev->next = iface;
	else
		first_iface = iface;
	UNLOCK_LIST;
	
	return iface;
}

// Allocates a blank interface.
int com_iface_free(com_iface_t *iface)
{
	if (!com_iface_is_valid(iface))
		return -1;
		
	// Take interface from list
	LOCK_LIST;
	if (iface->prev)
		iface->prev->next = iface->next;
	else
		first_iface = iface->next;
	if (iface->next)
		iface->next->prev = iface->prev;
	else
		last_iface = iface->prev;

	// release resources
	if (iface->name)
		free((void *)iface->name);
	if (iface->rx_buf)
		free(iface->rx_buf);
	free(iface);		
	UNLOCK_LIST;
	
	// If the list is empty, destroy the mutex
	LOCK_LIST;
	if (first_iface == 0)
	{
		pthread_mutex_destroy(&list_mutex);
		initialised = 0;
	}
	else
		UNLOCK_LIST;
	
	// All done
	return 0;
}

// Sets a default receive handler that gets assigned to all newly opened ports.
void com_iface_set_default_receive_handler(void (*fnc)(com_iface_t *, void *, size_t))
{
	com_iface_default_receive_handler = fnc;
}

// Sets a default connect handler that gets assigned to all newly opened ports.
void com_iface_set_default_connect_handler(void (*fnc)(com_iface_t *, int))
{
	com_iface_default_connect_handler = fnc;
}

// Interface iteration functions (read only).
com_iface_t *com_iface_get_first(void)				{ return first_iface; }
com_iface_t *com_iface_get_last(void)				{ return last_iface; }
com_iface_t *com_iface_get_next(com_iface_t *iface)	{ return iface ? iface->next : NULL; }
void		 com_iface_lock(void)					{ LOCK_LIST; }
void		 com_iface_unlock(void)					{ UNLOCK_LIST; }

