#import <Foundation/NSRunLoop.h>
#import "PseudoTermController.h"
//#import "NSFileManager.h"

static com_iface_t *iface_a = NULL;
static PseudoTermController *PesudoTerm = nil;

uint8_t Firmware[28 * 1024];
uint32_t FirmwareSize = 0;
uint32_t CurrAddress = 0;

static void receive_data(com_iface_t *iface, void *buf, size_t size);
static void connected(com_iface_t *iface, int connect);

@implementation PseudoTermController
@synthesize delegate = _delegate;

- (PseudoTermController *) initWithDelegate:(id<PseudoTermDelegate>) delegate
{
	if ((self = [super init]) != NULL)
	{
        _delegate = delegate;
        status = NO;
        recDone = NO;
		PesudoTerm = self;
	}

	return self;
}

- (void) initPseudoTerm
{
    NSLog(@"Initiallising pseudo terminal......");
    
    // Set up default callbackds for com ports
	com_iface_set_default_receive_handler(receive_data);
	com_iface_set_default_connect_handler(connected);
    
	com_iface_t **iface_p;
	
	// Work out which interface and settings we need to use.
    iface_p = &iface_a;
	
	// Close port if open
	if (*iface_p)
	{
		com_iface_close(*iface_p);
		*iface_p = NULL;
	}
    
	// Open port
	if ( (*iface_p = com_iface_open() ) == NULL)
	{
		// Failed to open        
        [self.delegate didOpenComport: -1];
        
		return;
	}
    
    NSString *str;
    str = [[NSString alloc] initWithUTF8String:iface_a->pterm.slave_name];
    NSLog(@"Pseudo: %@", str);
    
    NSFileManager *filemgr;
    filemgr = [NSFileManager defaultManager];
    
    if ([filemgr fileExistsAtPath: @"/tmp/tty.RBL-BLE" ] == YES)
    {
        NSLog(@"remove /tmp/tty.RBL-BLE");
        [filemgr removeItemAtPath: @"/tmp/tty.RBL-BLE" error: NULL];
    }
    if ([filemgr createSymbolicLinkAtPath: @"/tmp/tty.RBL-BLE" withDestinationPath: str error: NULL] == YES)
    {
        NSLog (@"Link successful");
        [self.delegate didOpenComport: 0];
    }
    else
    {
        NSLog (@"Link failed");
        [self.delegate didOpenComport: -2];
    }
    
    
}

- (void) ParsePacket:(uint8_t *)rxBuf :(size_t)size
{
    printf("---------- Packet length %zd ---------- \r\n", size);
    printf("Packet data: ");
    for(uint8_t i=0; i<size; i++)
    {
        printf("%d, ", rxBuf[i]);
    }
    printf("\r\n");
    switch (rxBuf[0])
    {
            // Finish receiving firmware
        case 'E':
        {
            uint8_t buf = '\r';
            com_iface_send(iface_a, &buf, 1);
            recDone = YES;
            status = YES;
            printf("Finish receiving firmware, size %u\r\n", FirmwareSize);
            break;
        }
            // Send confirmation byte back to the host
        case 'T':
        case 'P':
        case 'L':
        case 'e':
        case 'l':
        {
            printf("Send confirmation byte back to the host %d\r\n", rxBuf[0]);
            uint8_t buf = '\r';
            com_iface_send(iface_a, &buf, 1);
            break;
        }
            // Return ATMEGA128 part code - this is only to allow AVRProg to use the bootloader
        case 't':
        {
            printf("Return ATMEGA128 part code\r\n");
            uint8_t buf[2] = {0x44, 0x00};
            com_iface_send(iface_a, buf, 2);
            break;
        }
            // Indicate auto-address increment is supported
        case 'a':
        {
            printf("Indicate auto-address increment is supported\r\n");
            uint8_t buf = 'Y';
            com_iface_send(iface_a, &buf, 1);
            break;
        }
            // Set the current address to that given by the host
        case 'A':
        {
            CurrAddress  = rxBuf[1] << 9;
            CurrAddress |= rxBuf[2] << 1;
            printf("Set the current address %d\r\n", CurrAddress);
            if(CurrAddress == 0x00)
            {
                
            }
            uint8_t buf = '\r';
            com_iface_send(iface_a, &buf, 1);
            break;
        }
            // Indicate serial programmer back to the host
        case 'p':
        {
            printf("Indicate serial programmer back to the host\r\n");
            uint8_t buf = 'S';
            com_iface_send(iface_a, &buf, 1);
            break;
        }
            // Software identify
        case 'S':
        {
            printf("Read software identify\r\n");
            com_iface_send(iface_a, SOFTWARE_IDENTIFIER, strlen(SOFTWARE_IDENTIFIER));
            break;
        }
            // Bootloader version
        case 'V':
        {
            printf("Read Bootloader version\r\n");
            uint8_t buf[2] = {'0'+BOOTLOADER_VERSION_MAJOR, '0'+BOOTLOADER_VERSION_MINOR};
            com_iface_send(iface_a, buf, 2);
            break;
        }
            // Device signature
        case 's':
        {
            printf("Read Device signature\r\n");
            uint8_t buf[3] = {AVR_SIGNATURE_3, AVR_SIGNATURE_2, AVR_SIGNATURE_1};
            com_iface_send(iface_a, buf, 3);
            break;
        }
            // Send block size to the host
        case 'b':
        {
            printf("Send block size to the host\r\n");
            uint8_t buf[3] = {'Y', SPM_PAGESIZE >> 8, SPM_PAGESIZE & 0xFF};
            com_iface_send(iface_a, buf, 3);
            break;
        }
            // Read or Write firmware data
        case 'g':
        case 'B':
        {
            uint16_t BlockSize;
            uint8_t MemoryType;
            
            BlockSize = rxBuf[1] << 8;
            BlockSize |= rxBuf[2];
            MemoryType = rxBuf[3];
            
            if ((MemoryType != 'E') && (MemoryType != 'F'))
            {
                /* Send error byte back to the host */
                uint8_t buf = '?';
                com_iface_send(iface_a, &buf, 1);
                break;
            }
            
            // Read firmware command
            if(rxBuf[0] == 'g')
            {
                // Read flash
                if(MemoryType == 'F')
                {
                    printf("Read flash data, size: %d\r\n", BlockSize);
                    uint8_t buf[BlockSize];
                    memcpy(buf, &Firmware[CurrAddress], BlockSize);
                    com_iface_send(iface_a, buf, BlockSize);
                    CurrAddress += BlockSize;
                }
                // Read eeprom
                else if(rxBuf[3] == 'E')
                {
                    printf("Read eeprom data, size: %d\r\n", BlockSize);
                    uint8_t buf[BlockSize];
                    memset(buf, 0xFF, BlockSize);
                    com_iface_send(iface_a, buf, BlockSize);
                }
            }
            // Write firmware command
            else
            {
                // Write data to firmware rxBuf
                if(MemoryType == 'F')
                {
                    printf("Write flash data, size: %d\r\n", BlockSize);
                    memcpy(&Firmware[CurrAddress], &rxBuf[4], BlockSize);
                    FirmwareSize += BlockSize;
                    CurrAddress += BlockSize;
                }
                else
                {
                    printf("Write eeprom data, size: %d\r\n", BlockSize);
                }
                
                uint8_t buf = '\r';
                com_iface_send(iface_a, &buf, 1);
            }
            break;
        }
        default :
        {
            printf("Received unknown command\r\n");
            uint8_t buf = '?';
            com_iface_send(iface_a, &buf, 1);
            break;
        }
    }
    printf("------------------------------------- \r\n");
    printf("\r\n");
    
    if(recDone)
    {
        if(YES == status)
        {
            FirmwareData = [[NSData alloc] initWithBytes:Firmware length:FirmwareSize];
            [self.delegate didReceiveData:FirmwareData status:YES];
        }
        else
        {
            [self.delegate didReceiveData:nil status:NO];
        }
    }
}

- (void) closePseudoTerm:(bool) state
{
    NSLog(@"Closing pseudo terminal......");
    
    com_iface_t **iface_p;
	
	// Work out which interface and settings we need to use.
    iface_p = &iface_a;
	
	// Close port if open
	if (*iface_p)
	{
		com_iface_close(*iface_p);
	}
    
    recDone = NO;
    status = NO;
    
    iface_a = NULL;
    FirmwareSize = 0;
    CurrAddress = 0;
    
    if(YES == state)
    {
        [self.delegate didCloseComport: YES];
    }
    else
    {
        [self.delegate didCloseComport: NO];
    }
}

@end

static void receive_data(com_iface_t *iface, void *buf, size_t size)
{
	// Sanity check
	if (!iface || !buf || (size <= 0))
		return;
	
	// Forward from Arduino
	if ((iface == iface_a) || (iface->parent == iface_a))
    {
        NSLog(@"Received data from Arduino");
        
        // Read the data and parse it
        [PesudoTerm ParsePacket:buf :size];
    }
}

static void connected(com_iface_t *iface, int connect)
{
    if (!iface)
		return;
    NSLog(@"Connections: %d", connect);
}

