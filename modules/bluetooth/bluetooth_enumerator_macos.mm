/**************************************************************************/
/*  bluetooth_enumerator_macos.mm                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "bluetooth_server_macos.h"
#include "bluetooth_enumerator_macos.h"
#include "servers/bluetooth/bluetooth_enumerator.h"
#include "core/config/engine.h"
#include "core/core_bind.h"
#include "core/variant/typed_array.h"

#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>

//////////////////////////////////////////////////////////////////////////
// MyCentralManagerDelegate - This is a little helper class so we can discover our service

@interface MyCentralManagerDelegate : NSObject<CBCentralManagerDelegate, CBPeripheralDelegate> {
}

@property (nonatomic, assign) BluetoothEnumerator* context;
@property (nonatomic, assign) bool active;

@property (atomic, strong) CBCentralManager* centralManager;
@property (atomic, strong) NSMutableArray* peers;

- (instancetype) initWithQueue:(dispatch_queue_t)btleQueue;
- (bool) startScanning;
- (bool) stopScanning;
- (bool) connectToPeer: (CBUUID *)uuid;
- (void) discoverFromPeer: (CBPeripheral *)peer;
- (bool) readAtPeerServiceCharacteristic: (NSMutableArray *)parameters;
- (bool) writeAtPeerServiceCharacteristic: (NSMutableArray *)parameters;

@end

@implementation MyCentralManagerDelegate

+ (NSString*)stringFromCBManagerState:(CBManagerState)state
{
    switch (state)
    {
        case CBManagerStatePoweredOff:   return @"PoweredOff";
        case CBManagerStatePoweredOn:    return @"PoweredOn";
        case CBManagerStateResetting:    return @"Resetting";
        case CBManagerStateUnauthorized: return @"Unauthorized";
        case CBManagerStateUnknown:      return @"Unknown";
        case CBManagerStateUnsupported:  return @"Unsupported";
    }
}

#pragma mark init funcs

- (instancetype)init
{
    return [self initWithQueue:nil];
}

- (instancetype)initWithQueue:(dispatch_queue_t) btleQueue
{
    //NSLog(@"init %d", [NSThread isMultiThreaded]);
    self = [super init];
    if (self)
    {
        self.peers = [[NSMutableArray alloc] initWithCapacity:10];
        self.active = false;
        if (btleQueue)
        {
            self.centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:btleQueue];
        }
    }
    return self;
}

#pragma mark CENTRAL FUNCTIONS
- (void)centralManager:(CBCentralManager *)central
 didDiscoverPeripheral:(CBPeripheral *)peripheral
     advertisementData:(NSDictionary *)advertisementData
                  RSSI:(NSNumber *)RSSI
{
    String base64Data;
    NSError *error = nil;
    const char *name = [peripheral.name cStringUsingEncoding:NSASCIIStringEncoding];
    if (name == NULL)
    {
        name = "";
    }
    if (advertisementData != nil)
    {
        NSData *jsonData = nil;
        if ([NSJSONSerialization isValidJSONObject:advertisementData])
        {
            jsonData = [NSJSONSerialization dataWithJSONObject:advertisementData 
                                                       options:NSJSONWritingPrettyPrinted
                                                         error:&error];
        }
        if (jsonData != nil)
        {
            int length = jsonData.length;
            Vector<uint8_t> data;
            data.resize(length);
            memcpy(data.ptrw(), jsonData.bytes, length);
            base64Data += core_bind::Marshalls::get_singleton()->raw_to_base64(data);
        }
    }
    if (![self.peers containsObject:peripheral])
    {
        [self.peers addObject:peripheral];
    }
    NSLog(@"Found: %@", peripheral.name);
    self.context->on_discover(peripheral.identifier.UUIDString.UTF8String, name, base64Data);
}

- (void) centralManager: (CBCentralManager *)central
   didConnectPeripheral: (CBPeripheral *)peripheral
{
    if (![self.peers containsObject:peripheral]) 
    {
        [self.peers addObject:peripheral];
    }
    [peripheral setDelegate:self];
    if (!self.context->on_connect(peripheral.identifier.UUIDString.UTF8String))
    {
        [self.centralManager cancelPeripheralConnection:peripheral];
    }
    else
    {
        [self discoverFromPeer:peripheral];
    }
}

- (void) centralManager:(CBCentralManager *)central
 didRetrievePeripherals:(NSArray *)peripherals
{
    NSLog(@"Retrieved peripheral: %lu - %@", [peripherals count], peripherals);
    
    [self stopScanning];
    
    for (int i = 0; i < peripherals.count; i++)
    {
        CBPeripheral *peripheral = [peripherals objectAtIndex:i];
        if (!self.context->on_discover(peripheral.identifier.UUIDString.UTF8String, "", ""))
        {
            NSDictionary *connectOptions = @{
                CBConnectPeripheralOptionNotifyOnConnectionKey: @YES,
                CBConnectPeripheralOptionNotifyOnDisconnectionKey: @YES,
                CBConnectPeripheralOptionNotifyOnNotificationKey: @YES,
                //        CBConnectPeripheralOptionEnableTransportBridgingKey:,
                //        CBConnectPeripheralOptionRequiresANCS:,
                CBConnectPeripheralOptionStartDelayKey: @0
            };
            [self.centralManager connectPeripheral:peripheral
                                 options:connectOptions];
            [self.centralManager cancelPeripheralConnection:peripheral];
        }
        else
        {
            if (![self.peers containsObject:peripheral]) 
            {
                [self.peers addObject:peripheral];
            }
        }
    }
}

- (void) centralManager: (CBCentralManager *)central
didDisconnectPeripheral: (CBPeripheral *)peripheral
                  error: (NSError *)error
{
    NSLog(@"didDisconnectPeripheral %@ with error = %@", peripheral, [error localizedDescription]);
    if ([self.peers containsObject:peripheral]) 
    {
        [self.peers removeObject:peripheral];
    }
    self.context->on_disconnect(peripheral.identifier.UUIDString.UTF8String);
}

- (void)    centralManager: (CBCentralManager *)central
didFailToConnectPeripheral: (CBPeripheral *)peripheral
                     error: (NSError *)error
{
    NSLog(@"Fail to connect to peripheral: %@ with error = %@", peripheral, [error localizedDescription]);
    if ([self.peers containsObject:peripheral]) 
    {
        [self.peers removeObject:peripheral];
    }
    self.context->on_disconnect(peripheral.identifier.UUIDString.UTF8String);
}

- (void)peripheral:(CBPeripheral *)peripheral 
 didModifyServices:(NSArray<CBService *> *)invalidatedServices
{
    [self discoverFromPeer:peripheral];
}

- (void) peripheral: (CBPeripheral *)peripheral
didDiscoverServices: (NSError *)error
{
    TypedArray<String> services = self.context->get_sought_services();
    NSLog(@"Service discovery error: %@", error);
    for (CBService *service in peripheral.services)
    {
        if (services.size() > 0)
        {
            bool check = true;
            for (int i = 0; i < services.size(); i++)
            {
                if (strcmp(services[i].stringify().utf8().get_data(), service.UUID.UUIDString.UTF8String) == 0)
                {
                    check = false;
                    break;
                }
            }
            if (check)
            {
                continue;
            }
        }
        NSLog(@"Service found with UUID: %@", service.UUID);
        [peripheral discoverCharacteristics:nil
                    forService:service];
    }
}

- (void)                  peripheral:(CBPeripheral *)peripheral
didDiscoverCharacteristicsForService:(CBService *)service
                               error:(NSError *)error
{
    TypedArray<String> services = self.context->get_sought_services();
    //NSLog(@"Characteristic discovery error: %@", error);
    if (services.size() > 0)
    {
        bool check = true;
        for (int i = 0; i < services.size(); i++)
        {
            if (strcmp(services[i].stringify().utf8().get_data(), service.UUID.UUIDString.UTF8String) == 0)
            {
                check = false;
                break;
            }
        }
        if (check)
        {
            service = nil;
        }
    }
    if (service == nil)
    {
        return;
    }
    for (CBCharacteristic *characteristic in service.characteristics)
    {
        bool writable = false;
        if (characteristic.properties & CBCharacteristicPropertyWrite)
        {
            writable = true;
        }
        self.context->on_discover_service_characteristic(peripheral.identifier.UUIDString.UTF8String, service.UUID.UUIDString.UTF8String, characteristic.UUID.UUIDString.UTF8String, writable);
        NSLog(@"Service: %@ with Char: %@", service.UUID, characteristic.UUID);
        if (characteristic.properties & CBCharacteristicPropertyNotify)
        {
            [peripheral setNotifyValue:YES forCharacteristic:characteristic];
        }
    }
}

- (void)             peripheral:(CBPeripheral *)peripheral 
didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
                          error:(NSError *)error
{
    if (error != nil || characteristic.value == nil)
    {
        NSLog(@"Characteristic read error: %@", error);
        self.context->on_error(peripheral.identifier.UUIDString.UTF8String, characteristic.service.UUID.UUIDString.UTF8String, characteristic.UUID.UUIDString.UTF8String);
        return;
    }
    int length = characteristic.value.length;
    Vector<uint8_t> value;
    value.resize(length);
    memcpy(value.ptrw(), characteristic.value.bytes, length);
    self.context->on_read(peripheral.identifier.UUIDString.UTF8String, characteristic.service.UUID.UUIDString.UTF8String, characteristic.UUID.UUIDString.UTF8String, core_bind::Marshalls::get_singleton()->raw_to_base64(value));
}

- (void)             peripheral:(CBPeripheral *)peripheral 
 didWriteValueForCharacteristic:(CBCharacteristic *)characteristic
                          error:(NSError *)error
{
    if (error != nil)
    {
        NSLog(@"Characteristic write error: %@", error);
        self.context->on_error(peripheral.identifier.UUIDString.UTF8String, characteristic.service.UUID.UUIDString.UTF8String, characteristic.UUID.UUIDString.UTF8String);
        return;
    }
    self.context->on_write(peripheral.identifier.UUIDString.UTF8String, characteristic.service.UUID.UUIDString.UTF8String, characteristic.UUID.UUIDString.UTF8String);
}

- (void)peripheralDidUpdateName:(CBPeripheral *)peripheral
{
    const char *name = [peripheral.name cStringUsingEncoding:NSASCIIStringEncoding];
    if (name == NULL)
    {
        name = "";
    }
    self.context->on_discover(peripheral.identifier.UUIDString.UTF8String, name, "");
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)manager
{
    NSLog(@"CBCentralManager entered state %@", [MyCentralManagerDelegate stringFromCBManagerState:manager.state]);
    if (manager.state == CBManagerStatePoweredOn && self.active)
    {
		self.active = false;
        if ([self startScanning])
		{
			self.active = true;
		}
    }
}

- (bool) startScanning
{
	bool success = false;
    if (self.centralManager.state == CBManagerStatePoweredOn && !self.active)
    {
        int count = self.context->get_sought_service_count();
        NSDictionary *scanOptions = @{CBCentralManagerScanOptionAllowDuplicatesKey : @YES};
        if (count > 0)
        {
            NSMutableArray *services = [[NSMutableArray alloc] initWithCapacity:count];
            for (int i = 0; i < count; i++)
            {
                CBUUID *uuid = [CBUUID UUIDWithString:[[NSString alloc] initWithUTF8String:self.context->get_sought_service(i).utf8().get_data()]];
                [services addObject:uuid];
            }
            NSLog(@"services: %@", services);
            if (!self.centralManager.isScanning)
            {
                [self.centralManager scanForPeripheralsWithServices: services
                                     options: scanOptions];
            }
        }
        else
        {
            if (!self.centralManager.isScanning)
            {
                [self.centralManager scanForPeripheralsWithServices: nil
                                     options: scanOptions];
            }
        }
		self.active = true;
		success = true;
	}
    if (success)
    {
        if (self.context->on_start())
        {
            NSLog(@"on_start: %d", self.active);
        }
    }
	return success;
}

- (bool) stopScanning
{
    bool success = false;
    if (self.centralManager.isScanning)
    {
        [self.centralManager stopScan];
        self.context->on_stop();
        self.context->stop_scanning();
        self.active = false;
        success = true;
    }
    return success;
}

- (bool) connectToPeer: (CBUUID *)uuid
{
    bool success = false;
    int count = self.peers.count;
    for (int i = 0; i < count; i++)
    {
        CBPeripheral *peer = [self.peers objectAtIndex:i];
        if ([peer.identifier.UUIDString isEqualToString:uuid.UUIDString])
        {
            NSDictionary *connectOptions = @{
                CBConnectPeripheralOptionNotifyOnConnectionKey: @YES,
                CBConnectPeripheralOptionNotifyOnDisconnectionKey: @YES,
                CBConnectPeripheralOptionNotifyOnNotificationKey: @YES,
                //        CBConnectPeripheralOptionEnableTransportBridgingKey:,
                //        CBConnectPeripheralOptionRequiresANCS:,
                CBConnectPeripheralOptionStartDelayKey: @0
            };
            [self.centralManager connectPeripheral:peer options:connectOptions];
            success = true;
            break;
        }
    }
    if (success && self.centralManager.isScanning)
    {
        [self stopScanning];
    }
    return success;
}

- (void) discoverFromPeer: (CBPeripheral *)peer
{
    int count = self.context->get_sought_service_count();
    if (count > 0)
    {
        NSMutableArray *services = [[NSMutableArray alloc] initWithCapacity:count];
        for (int i = 0; i < count; i++)
        {
            CBUUID *uuid = [CBUUID UUIDWithString:[[NSString alloc] initWithUTF8String:self.context->get_sought_service(i).utf8().get_data()]];
            [services addObject:uuid];
        }
        [peer discoverServices:services];
    }
    else
    {
        [peer discoverServices:nil];
    }
}

- (bool) readAtPeerServiceCharacteristic: (NSMutableArray *)parameters
{
    bool success = false;
    int count = self.peers.count;
    CBUUID *peerUUID = [CBUUID UUIDWithString:[parameters objectAtIndex:0]];
    CBUUID *serviceUUID = [CBUUID UUIDWithString:[parameters objectAtIndex:1]];
    CBUUID *characteristicUUID = [CBUUID UUIDWithString:[parameters objectAtIndex:2]];
    for (int i = 0; i < count; i++)
    {
        CBPeripheral *peer = [self.peers objectAtIndex:i];
        if ([peerUUID.UUIDString isEqualToString:peer.identifier.UUIDString])
        {
            for (CBService *service in peer.services)
            {
                if ([serviceUUID.UUIDString isEqualToString:service.UUID.UUIDString])
                {
                    for (CBCharacteristic *characteristic in service.characteristics)
                    {
                        if ([characteristicUUID.UUIDString isEqualToString:characteristic.UUID.UUIDString])
                        {
                            if (characteristic.properties & CBCharacteristicPropertyRead)
                            {
                                [peer readValueForCharacteristic:characteristic];
                                success = true;
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
    }
    return success;
}

- (bool) writeAtPeerServiceCharacteristic: (NSMutableArray *)parameters
{
    bool success = false;
    int count = self.peers.count;
    CBUUID *peerUUID = [CBUUID UUIDWithString:[parameters objectAtIndex:0]];
    CBUUID *serviceUUID = [CBUUID UUIDWithString:[parameters objectAtIndex:1]];
    CBUUID *characteristicUUID = [CBUUID UUIDWithString:[parameters objectAtIndex:2]];
    NSString *value = [parameters objectAtIndex:3];
    for (int i = 0; i < count; i++)
    {
        CBPeripheral *peer = [self.peers objectAtIndex:i];
        if ([peerUUID.UUIDString isEqualToString:peer.identifier.UUIDString])
        {
            for (CBService *service in peer.services)
            {
                if ([serviceUUID.UUIDString isEqualToString:service.UUID.UUIDString])
                {
                    for (CBCharacteristic *characteristic in service.characteristics)
                    {
                        if ([characteristicUUID.UUIDString isEqualToString:characteristic.UUID.UUIDString])
                        {
                            if (characteristic.properties & CBCharacteristicPropertyWrite)
                            {
                                Vector<uint8_t> base64Data = core_bind::Marshalls::get_singleton()->base64_to_raw(value.UTF8String);
                                NSData *data = [NSData dataWithBytes:base64Data.ptr() length:base64Data.size()];
                                [peer writeValue:data
                                      forCharacteristic:characteristic
                                      type:CBCharacteristicWriteWithResponse];
                                success = true;
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
    }
    return success;
}

@end

BluetoothEnumeratorMacOS::BluetoothEnumeratorMacOS() {
    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            dispatch_queue_t btle_service = dispatch_queue_create("btle_service",  DISPATCH_QUEUE_SERIAL);
            MyCentralManagerDelegate *central_manager_delegate = [[MyCentralManagerDelegate alloc] initWithQueue:btle_service];
            central_manager_delegate.context = this;
            this->central_manager_delegate = (__bridge_retained void *)central_manager_delegate;
        }
    });
}

BluetoothEnumeratorMacOS::~BluetoothEnumeratorMacOS() {
	//MyCentralManagerDelegate *central_manager_delegate = (__bridge MyCentralManagerDelegate *)this->central_manager_delegate;
    CFRelease(this->central_manager_delegate);
}

bool BluetoothEnumeratorMacOS::start_scanning() const {
    MyCentralManagerDelegate *central_manager_delegate = (__bridge MyCentralManagerDelegate *)this->central_manager_delegate;
    return [central_manager_delegate startScanning];
}

bool BluetoothEnumeratorMacOS::stop_scanning() const {
    MyCentralManagerDelegate *central_manager_delegate = (__bridge MyCentralManagerDelegate *)this->central_manager_delegate;
    return [central_manager_delegate stopScanning];
}

void BluetoothEnumeratorMacOS::connect_peer(String p_peer_uuid) {
    MyCentralManagerDelegate *central_manager_delegate = (__bridge MyCentralManagerDelegate *)this->central_manager_delegate;
    CBUUID *peer = [CBUUID UUIDWithString:[[NSString alloc] initWithUTF8String:p_peer_uuid.utf8().get_data()]];
    if (![central_manager_delegate connectToPeer:peer])
    {
        NSLog(@"Connection failure: %@", peer);
        on_disconnect(p_peer_uuid);
    }
}

void BluetoothEnumeratorMacOS::read_peer_service_characteristic(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid) {
    MyCentralManagerDelegate *central_manager_delegate = (__bridge MyCentralManagerDelegate *)this->central_manager_delegate;
    NSMutableArray *parameters = [[NSMutableArray alloc] initWithCapacity:4];
    [parameters addObject:[[NSString alloc] initWithUTF8String:p_peer_uuid.utf8().get_data()]];
    [parameters addObject:[[NSString alloc] initWithUTF8String:p_service_uuid.utf8().get_data()]];
    [parameters addObject:[[NSString alloc] initWithUTF8String:p_characteristic_uuid.utf8().get_data()]];
    if (![central_manager_delegate readAtPeerServiceCharacteristic:parameters])
    {
        NSLog(@"Read failure: %@", [parameters objectAtIndex:0]);
        on_error(p_peer_uuid, p_service_uuid, p_characteristic_uuid);
    }
}

void BluetoothEnumeratorMacOS::write_peer_service_characteristic(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid, String p_value) {
    MyCentralManagerDelegate *central_manager_delegate = (__bridge MyCentralManagerDelegate *)this->central_manager_delegate;
    NSMutableArray *parameters = [[NSMutableArray alloc] initWithCapacity:4];
    [parameters addObject:[[NSString alloc] initWithUTF8String:p_peer_uuid.utf8().get_data()]];
    [parameters addObject:[[NSString alloc] initWithUTF8String:p_service_uuid.utf8().get_data()]];
    [parameters addObject:[[NSString alloc] initWithUTF8String:p_characteristic_uuid.utf8().get_data()]];
    [parameters addObject:[[NSString alloc] initWithUTF8String:core_bind::Marshalls::get_singleton()->utf8_to_base64(p_value).utf8().get_data()]];
    if (![central_manager_delegate writeAtPeerServiceCharacteristic:parameters])
    {
        NSLog(@"Write failure: %@", [parameters objectAtIndex:0]);
        on_error(p_peer_uuid, p_service_uuid, p_characteristic_uuid);
    }
}

void BluetoothEnumeratorMacOS::on_register() const {
	// nothing to do here
}

void BluetoothEnumeratorMacOS::on_unregister() const {
	// nothing to do here
}
