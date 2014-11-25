//
//  AppDelegate.h
//  SimpleControls
//
//  Created by Cheong on 27/10/12.
//  Copyright (c) 2012 RedBearLab. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "IntelHex2BinConverter.h"
#import "DFUController.h"
#import "PseudoTermController.h"

@interface AppDelegate : NSObject <NSApplicationDelegate, CBCentralManagerDelegate, CBPeripheralDelegate, DFUControllerDelegate, PseudoTermDelegate>
{
    IBOutlet NSButton *btnUpload;
    IBOutlet NSButton *btnSelectFile;
    IBOutlet NSButton *btnSketch;
    IBOutlet NSProgressIndicator *indConnect;
    IBOutlet NSTextField *txtFile;
    IBOutlet NSProgressIndicator *progress;
}

@property (assign) IBOutlet NSWindow *window;

@property (strong, nonatomic) CBCentralManager *bluetoothManager;
@property (strong, nonatomic) NSMutableArray *peripherals;
@property (strong, nonatomic) CBPeripheral *selectedPeripheral;
@property DFUController *dfuController;
@property PseudoTermController *PseudoTermApp;
@property BOOL isUploading;
@property BOOL isComportOpen;
@property (nonatomic) bool FirmwareReady;

@end
