//
//  com_pterm.h
//  SimpleControls
//
//  Created by redbear on 14-8-17.
//  Copyright (c) 2014年 RedBearLab. All rights reserved.
//

#ifndef SimpleControls_com_pterm_h
#define SimpleControls_com_pterm_h

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include "com_iface.h"

com_iface_t *com_iface_pterm_open(void);
ssize_t com_iface_pterm_send(com_iface_t *iface, void *buf, size_t size);
int com_iface_pterm_close(com_iface_t *iface);

#endif
