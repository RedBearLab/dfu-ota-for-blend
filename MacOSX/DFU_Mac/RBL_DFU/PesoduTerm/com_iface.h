/*
 *  com.h
 *
 *  Created by Martin Louis on 9/08/06.
 *  Copyright 2006 Martin Louis. All rights reserved.
 *
 *  com_iface.c handles the interface to a communication channel.
 *  It opens the appropriate device and spawns a thread
 *  to read this device in the background. It passes received
 *  data to a function installed by the host application and 
 *  accepts requests to send data.
 */

#ifndef COM_H
#define COM_H

#include <pthread.h>
#include <termios.h>

// When defined will spawn a new thread and iface for each new TCP connection.
// When undefed listen thread will select on all of the connections.
#undef COM_THREAD_PER_CONNECTION

#define COM_IFACE_RX_BUF_SIZE			256

typedef enum
{
	comsigDTR,		// Read / write
	comsigDSR,		// Read only
	comsigRTS,		// Read / write
	comsigCTS,		// Read only
	comsigCD,		// Read only
	comsigRI		// Read only
} com_sig_t;



// Context for a com interface
typedef struct com_iface_s
{
	struct com_iface_s *next;				// Link to next interface
	struct com_iface_s *prev;				// Link to previous interface
	const char		   *name;				// Name of port
	struct com_iface_s *parent;				// Link to parent if it is a connection spawned from a server
	int					close_request;		// Is set to tell interface thread to close
	int					thread_running;		// Flag indicates that a handler thread is running
	int					connections;		// Number connections. Normally 1 but when iface is server, is num clients
	unsigned char 		*rx_buf;			// Receive buffer
	void (*receive_handler)(struct com_iface_s *, void *, size_t);
	void (*connection_handler)(struct com_iface_s *, int);
	
	// User fields. These are NEVER touched.
	// Application can use this to track interfaces
	// or associate custom data with them.
	// EXCEPTION: When an asychronous TCP server
	// accepts a connection and creates a new
	// child port, its tag and user_data is
	// copied from its parent.
	int					tag;
	void			   *user_data;

	// Thread management
	pthread_t			thread;				// Thread that controls the task

	// Interface specific data
	union
	{		
		// State data for pseudo terminals
		struct
		{
			const char			*slave_name;		// Serial device
			int					fd;					// File descriptor for reading and writing
		} pterm;
	};

} com_iface_t;


// Public data
void (*com_iface_default_receive_handler)(com_iface_t *, void *, size_t);
void (*com_iface_default_connect_handler)(com_iface_t *, int);

// returns 1 if argument is a valid interface pointer, 0 if not
int com_iface_is_valid(com_iface_t *iface);

// Interface open functions a communications interface
com_iface_t *com_iface_open(void);

// Sends data through an interface
int com_iface_send(com_iface_t *iface, void *buf, ssize_t size);

// Assigns default handler functions. These are assigned to all newly opened ports.
void com_iface_set_default_receive_handler(void (*fnc)(com_iface_t *, void *, size_t));
void com_iface_set_default_connect_handler(void (*fnc)(com_iface_t *, int));

// Tells a single interfaces thread to close the interface.
// Returns 0 if successful, -1 if not.
int com_iface_close(com_iface_t *);

// Interface iteration functions (read only).
com_iface_t *com_iface_get_first(void);
com_iface_t *com_iface_get_last(void);
com_iface_t *com_iface_get_next(com_iface_t *iface);
void		 com_iface_lock(void);
void		 com_iface_unlock(void);


#endif	// COM_H
