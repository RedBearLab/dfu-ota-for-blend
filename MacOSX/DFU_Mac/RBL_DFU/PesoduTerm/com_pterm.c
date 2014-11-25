/*
 *  com_pterm.c
 *
 *  Created by Martin Louis on 17/11/06.
 *  Copyright 2006 Martin Louis. All rights reserved.
 *
 */

#include "com_pterm.h"

#include <unistd.h>
#include <sys/ioctl.h>
#include <paths.h>
#include <termios.h>
#include <sysexits.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>

#import <IOKit/IOTypes.h>
#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOBSD.h>

#import <IOKit/IOKitLib.h>
#import <IOKit/serial/IOSerialKeys.h>


/////////////////////////////////////////////////////////////////
// Configuration
static void *receive_thread(void *thread_arg);

/////////////////////////////////////////////////////////////////
// External functions
extern com_iface_t *com_iface_alloc(com_iface_t *parent);
extern void			com_iface_free(com_iface_t *iface);

/////////////////////////////////////////////////////////////////
// Creates a pseudo-terminal pair interface and spawns associated thread

com_iface_t *com_iface_pterm_open(void)
{
	com_iface_t *iface;
	char *slave_name;

	if ((iface = com_iface_alloc(NULL)) == NULL)
	{
		printf("Failed to allocate memory for pseudo terminal port.\r\n");
    	return NULL;
	}
		
	// Initialise
	iface->pterm.slave_name = NULL;
	iface->pterm.fd = -1;

	// Open device
	if ((iface->pterm.fd = posix_openpt(O_RDWR | O_NOCTTY)) == -1)
	{
		printf("Unable to open master pseudo-terminal device.\r\n");
		com_iface_pterm_close(iface);
		return NULL;
	}
	
	// Get slave device
	if ((slave_name = ptsname(iface->pterm.fd)) == NULL)
	{
		printf("Unable to get slave name for pseudo-terminal.\r\n");
		com_iface_pterm_close(iface);
		return NULL;
	}

	// Set the intrface name
	asprintf((char **)&iface->name, "pseudo-term:%s", slave_name);
	iface->pterm.slave_name = strdup(slave_name);
	
	// Change slave mode and owner
	if (grantpt(iface->pterm.fd) != 0)
	{
		printf("Unable to set owner and mode on communications interface '%s'.\r\n", iface->name);
		com_iface_pterm_close(iface);
		return NULL;
	}

	// Unlock slave
	if (unlockpt(iface->pterm.fd) != 0)
	{
		printf("Unable to unlock on communications interface '%s'.\r\n", iface->name);
		com_iface_pterm_close(iface);
		return NULL;
	}
	
	// Spawn handler thread
	if (pthread_create(&iface->thread, NULL, receive_thread, iface) == 0)
    {
		iface->thread_running = 1;
    }
	else
	{
		printf("Unable to start communications interface '%s' (Thread failed).\r\n", iface->name);
		com_iface_pterm_close(iface);
	}
	
	return iface;
}

/////////////////////////////////////////////////////////////////
// Closes a pseudo terminal interface and spawns associated thread

int com_iface_pterm_close(com_iface_t *iface)
{
	// Sanity checks
	if (!iface)
		return -1;
		
	// If interface has an associated thread, ask it to close.
	if (iface->thread_running)
	{
		// Ask thread to stop (will remove itself from interface list
		iface->close_request = 1;
		
		// Wait for thread
		pthread_join(iface->thread, NULL);
	}

	// Restore original settings and close the device
	if (iface->pterm.fd != -1)
	{
		close(iface->pterm.fd);
		iface->pterm.fd = -1;
	}
	
	// Release stuff from iface->pterm.
	if (iface->pterm.slave_name)
		free((void *)iface->pterm.slave_name);
	iface->pterm.slave_name = NULL;
	
	//com_iface_free(iface);// Should free after thread exiting while loop
	return 0;
}



/////////////////////////////////////////////////////////////////
// Sends on a serial interface
ssize_t com_iface_pterm_send(com_iface_t *iface, void *buf, size_t size)
{
	ssize_t len;

	if (!iface || !buf || (size <= 0))
		return 0;

	if (iface->pterm.fd >= 0)
	{
		if ((len = write(iface->pterm.fd, buf, size)) < 0)
			return 0;
		return len;
	}

	return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Receive thread
static void *receive_thread(void *thread_arg)
{
	com_iface_t *iface;
	ssize_t len;

	iface = (com_iface_t *)thread_arg;
	
	// Make sure we have an interface
	if (iface)
	{
		iface->connections++;
		if (iface->connection_handler)
			iface->connection_handler(iface, 1);
			
		// Keep reading stuff while we are
		while (!iface->close_request)
		{
			fd_set fds; 
			struct timeval timeout;
			int result;
			
			// Create a file descriptor set for accepting
			// so we don't block indefinately
			FD_ZERO(&fds);
			FD_SET(iface->pterm.fd, &fds);
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
			
			// Wait for data to be ready or timeout
			result = select(iface->pterm.fd + 1, &fds, NULL, NULL, &timeout);
			
			// Check for timeout
			if (result == 0)
				continue;
			
			// Check for error
			if (result < 0)
				break;

			// Read a bunch of characters
			len = read(iface->pterm.fd, iface->rx_buf, COM_IFACE_RX_BUF_SIZE);
			if (len > 0)
			{
				// Send the received data to host application
				if (iface->receive_handler)
					iface->receive_handler(iface, iface->rx_buf, len);
			}
		}

		iface->connections--;
		if (iface->connection_handler)
        {
			iface->connection_handler(iface, 0);
        }
	}

	iface->thread_running = 0;
    com_iface_free(iface);
    
    printf("free pseudo terminal\r\n");
    
	pthread_exit(0);
}



