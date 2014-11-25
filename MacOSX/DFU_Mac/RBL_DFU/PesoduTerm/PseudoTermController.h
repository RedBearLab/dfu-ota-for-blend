//
//  PseudoTermControoler.h
//  SimpleControls
//
//  Created by redbear on 14-8-13.
//  Copyright (c) 2014年 RedBearLab. All rights reserved.
//

#ifndef SimpleControls_PseudoTermControoler_h
#define SimpleControls_PseudoTermControoler_h

#import <Cocoa/Cocoa.h>
#include "com_iface.h"

// Definitions for AVR901 protocol

#define SOFTWARE_IDENTIFIER          "CATERINA"

/** Version major of the CDC bootloader. */
#define BOOTLOADER_VERSION_MAJOR     0x01
/** Version minor of the CDC bootloader. */
#define BOOTLOADER_VERSION_MINOR     0x00

#define SPM_PAGESIZE                 (128)

#define AVR_SIGNATURE_1               0x1E
#define AVR_SIGNATURE_2               0x95
#define AVR_SIGNATURE_3               0x87

@protocol PseudoTermDelegate <NSObject>

- (void) didOpenComport:(int) status;

- (void) didCloseComport:(bool) status;

- (void) didReceiveData:(NSData *) FirmwareData status:(bool) status;

@end

@interface PseudoTermController : NSObject
{
    bool status;
    bool recDone;
    NSData* FirmwareData;
}

@property id<PseudoTermDelegate> delegate;

- (PseudoTermController *) initWithDelegate:(id<PseudoTermDelegate>) delegate;
- (void) initPseudoTerm;
- (void) ParsePacket:(uint8_t *)rxBuf :(size_t)size;
- (void) closePseudoTerm:(bool) state;

@end

#endif
