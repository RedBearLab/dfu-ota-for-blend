//
//  AppDelegate.m
//  SimpleControls
//
//  Created by Cheong on 27/10/12.
//  Copyright (c) 2012 RedBearLab. All rights reserved.
//

#import "AppDelegate.h"

@implementation AppDelegate

@synthesize bluetoothManager;
@synthesize peripherals;
@synthesize selectedPeripheral;
@synthesize dfuController;
@synthesize PseudoTermApp;
@synthesize FirmwareReady = _FirmwareReady;

NSTimer *repeatTimer;

- (BOOL) applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)application
{
    return YES;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    dfuController = [[DFUController alloc] initWithDelegate:self];
    
    bluetoothManager = [[CBCentralManager alloc]initWithDelegate:self queue:nil];
    
    //virtualComport = [[VirtualComport alloc] initWithDelegate:self];
    PseudoTermApp = [[PseudoTermController alloc] initWithDelegate:self];
    
    self.isUploading = NO;
    self.isComportOpen = NO;
    self.FirmwareReady = NO;
    
    //[PseudoTermApp initPseudoTerm];
    //repeatTimer = [NSTimer scheduledTimerWithTimeInterval:(float)1.0 target:self selector:@selector(autoUpload:) userInfo:nil repeats:YES];
    //[btnSketch setHidden:YES];
}

- (void) autoUpload:(NSTimer *)timer
{
    if(self.FirmwareReady)
    {
        NSLog(@"autoUpload");
        [repeatTimer invalidate];
        
        [self btnUpload:btnUpload];
    }
}

- (void) setFirmwareReady:(bool)isFirmwareReady
{
    _FirmwareReady = isFirmwareReady;

    if(isFirmwareReady)
    {
        btnUpload.enabled = true;
        btnUpload.title = @"Upload";
    }
    else
    {
        btnUpload.enabled = false;
        btnUpload.title = @"Upload";
    }
}

- (IBAction)btnSketch:(id)sender
{
    if(!self.isComportOpen)
    {
        self.FirmwareReady = NO;
        //[virtualComport OpenSerialPort:"/tmp/tty.RBL-BLE1"];
        [PseudoTermApp initPseudoTerm];
        repeatTimer = [NSTimer scheduledTimerWithTimeInterval:(float)1.0 target:self selector:@selector(autoUpload:) userInfo:nil repeats:YES];
    }
    else
    {
        //[virtualComport CloseSerialPort:NO];
        [PseudoTermApp closePseudoTerm:NO];
        [repeatTimer invalidate];
    }
}

- (IBAction)btnSelectFile:(id)sender
{
    NSLog(@"Select file");
    
    // Create a File Open Dialog class.
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    
    // Set array of file types
    NSArray *fileTypesArray;
    fileTypesArray = [NSArray arrayWithObjects:@"hex", @"bin", nil];
    
    // Enable options in the dialog.
    [panel setCanChooseFiles:YES];
    [panel setCanChooseDirectories:NO];
    [panel setAllowsMultipleSelection:NO]; // yes if more than one dir is allowed
    [panel setAllowedFileTypes:fileTypesArray];
    [panel setAllowsOtherFileTypes:YES];
    
    if([panel runModal] == NSFileHandlingPanelOKButton)
    {
        NSArray *files = [panel URLs];
        
        [txtFile setStringValue:(NSString *)[[files objectAtIndex:0] filePathURL]];
        NSURL *url = [[files objectAtIndex:0] filePathURL];
        NSLog(@"File URL: %@", url);
        
        self.FirmwareReady = YES;
        [dfuController setFirmwareURL:url];
    }
    else
    {
        NSLog(@"Select file canceled");
        self.FirmwareReady = NO;
    }
}

- (IBAction)btnUpload:(id)sender
{
    if (!self.isUploading)
    {
        btnUpload.enabled = false;
        btnUpload.title=@"Scanning";
        
        [self findBLEPeripherals:2];
        
        [NSTimer scheduledTimerWithTimeInterval:(float)3.0 target:self selector:@selector(connectionTimer:) userInfo:nil repeats:NO];
        
        [indConnect startAnimation:self];
    }
    else
    {
        btnUpload.title=@"Upload";
        btnUpload.enabled = true;
        [dfuController cancelTransfer];
    }
}

- (int) findBLEPeripherals:(int) timeout
{
    if (bluetoothManager.state != CBCentralManagerStatePoweredOn)
    {
        NSLog(@"CoreBluetooth not correctly initialized !\r\n");
        return -1;
    }
    
    [bluetoothManager scanForPeripheralsWithServices:[NSArray arrayWithObject:[CBUUID UUIDWithString:@DFU_SERVICE_UUID]] options:nil]; // Start scanning
    
    [NSTimer scheduledTimerWithTimeInterval:(float)timeout target:self selector:@selector(scanTimer:) userInfo:nil repeats:NO];
    
    NSLog(@"scanForPeripheralsWith DFU Services");
    
    return 0; // Started scanning OK !
}

- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary *)advertisementData RSSI:(NSNumber *)RSSI
{
    if (!self.peripherals)
    {
        self.peripherals = [[NSMutableArray alloc] initWithObjects:peripheral,nil];
    }
    else
    {
        [self.peripherals addObject:peripheral];
    }
    
    NSLog(@"didDiscoverPeripheral");
}

- (void) scanTimer:(NSTimer *)timer
{
    NSLog(@"Stopped Scanning");
    [bluetoothManager stopScan];

    NSLog(@"Known peripherals : %lu",(unsigned long)[self.peripherals count]);
    if([self.peripherals count] != 0)
    {
        [self printPeripheralInfo];
    }
    else
    {
        btnUpload.title=@"Upload";
        btnUpload.enabled = true;
        [indConnect stopAnimation:self];
    }
}

- (void) printPeripheralInfo
{
    CBPeripheral *p = [self.peripherals objectAtIndex:0];
    if (p.UUID != NULL)
    {
        printf("------------------------------------\n");
        printf("Peripheral Info :\n");
        CFStringRef s = CFUUIDCreateString(NULL, p.UUID);
        printf("UUID : %s\n",CFStringGetCStringPtr(s, 0));
        printf("Name : %s\n",[p.name cStringUsingEncoding:NSStringEncodingConversionAllowLossy]);
        printf("-------------------------------------\r\n");
    }
    else
    {
        [indConnect stopAnimation:self];
        btnUpload.title=@"Upload";
        btnUpload.enabled = true;
        printf("UUID : NULL\n");
    }
}

-(void) connectionTimer:(NSTimer *)timer
{
    if (self.peripherals.count > 0)
    {
        bluetoothManager.delegate = self;

        // Set the peripheral
        dfuController.peripheral = [self.peripherals objectAtIndex:0];
        selectedPeripheral = [self.peripherals objectAtIndex:0];
        
        NSLog(@"Connecting");
        btnUpload.title=@"Connecting";
        
        [bluetoothManager connectPeripheral:selectedPeripheral options:[NSDictionary dictionaryWithObject:[NSNumber numberWithBool:YES] forKey:CBConnectPeripheralOptionNotifyOnDisconnectionKey]];
    }
    else
    {
        [indConnect stopAnimation:self];
    }
}

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral
{
    btnUpload.title=@"Connected";
    
    // Peripheral has connected. Discover required services
    [dfuController didConnect];
    
    [indConnect stopAnimation:self];
}

-(void)didChangeState:(DFUControllerState) state;
{
    // Scanner uses other queue to send events. We must edit UI in the main queue
    dispatch_async(dispatch_get_main_queue(), ^{
        if (state == IDLE)
        {
            self.isUploading= YES;
            
            btnUpload.title=@"Uploading";
            btnUpload.enabled = true;
            
            [dfuController startTransfer];
        }
    });
}

-(void)didUpdateProgress:(float)percent
{
    [progress setDoubleValue:percent];
    if(percent ==0.0)
    {
        [progress setHidden:YES];
    }
    else
    {
        [progress setHidden:NO];
    }
}

-(void)didFinishTransfer
{
    NSLog(@"Transfer finished!");
    self.isUploading = NO;
    self.FirmwareReady = NO;
    btnUpload.title=@"Upload";
}

-(void)didCancelTransfer
{
    NSLog(@"Transfer cancelled!");
    self.isUploading = NO;
    self.FirmwareReady = NO;
    btnUpload.title=@"Upload";
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central
{
    if (central.state == CBCentralManagerStatePoweredOn) {
        // TODO
    }
    else
    {
        // TODO
        NSLog(@"Bluetooth not ON");
    }
}

- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error
{
    // Notify DFU controller about link loss
    [self.dfuController didDisconnect:error];
}

-(void)didDisconnect:(NSError *)error
{
    NSLog(@"Transfer terminated!");
    self.isUploading = NO;
    self.FirmwareReady = NO;
    btnUpload.title=@"Upload";
}

-(void)didErrorOccurred:(DFUTargetResponse)error
{
    NSLog(@"Error occurred: %d", error);
    self.isUploading = NO;
    self.FirmwareReady = NO;
    btnUpload.title=@"Upload";
    
    NSString* message = nil;
    switch (error)
    {
        case CRC_ERROR:
            message = @"CRC error";
            break;
                
        case DATA_SIZE_EXCEEDS_LIMIT:
            message = @"Data size exceeds limit";
            break;
                
        case INVALID_STATE:
            message = @"Device is in the invalid state";
            break;
                
        case NOT_SUPPORTED:
            message = @"Operation not supported. Image file may be to large.";
            break;
                
        case OPERATION_FAILED:
            message = @"Operation failed";
            break;
                
        case DEVICE_NOT_SUPPORTED:
            message = @"Device is not supported. Check if it is in the DFU state.";
            [bluetoothManager cancelPeripheralConnection:selectedPeripheral];
            selectedPeripheral = nil;
            break;
                
        default:
            message = @"Unknown error.";
            break;
    }
    
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:message];
    [alert runModal];
    NSLog(@"%@", message);
}

- (void) didOpenComport:(int) status
{
    if(status == 0)
    {
        NSLog(@"didOpenComport successfully.");
        
        btnSelectFile.enabled = false;
        btnSketch.title = @"Disassociate";
        btnUpload.title = @"Receiving";
        self.isComportOpen = YES;
    }
    else if(status == -1)
    {
        NSLog(@"Create comport error!");
    }
    else if(status == -2)
    {
        [PseudoTermApp closePseudoTerm:NO];
        NSLog(@"Create link error!");
    }
    else
    {
        NSLog(@"Unknown error!");
    }
}

- (void) didReceiveData:(NSData *) FirmwareData status:(bool) status
{
    if(YES == status)
    {
        NSLog(@"didReceiveData");
        
        [dfuController setFirmwareBuffer:FirmwareData];
        
        //[virtualComport CloseSerialPort:YES];
        [PseudoTermApp closePseudoTerm:YES];
    }
    else
    {
        NSLog(@"Cancel receiving firmware");
    }
}

- (void) didCloseComport:(bool) status
{
    NSLog(@"didCloseComport");
    btnSelectFile.enabled = true;
    btnSketch.title = @"Associate";
    
    self.isComportOpen = NO;
    
    if(status == YES)
    {
        self.FirmwareReady = YES;
    }
    else
    {
        self.FirmwareReady = NO;
    }
}

@end
