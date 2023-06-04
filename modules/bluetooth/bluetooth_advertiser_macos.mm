/**************************************************************************/
/*  bluetooth_advertiser_macos.mm                                         */
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
#include "bluetooth_advertiser_macos.h"
#include "servers/bluetooth/bluetooth_advertiser.h"
#include "core/config/engine.h"
#include "core/core_bind.h"
#include "core/variant/typed_array.h"

#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>

//////////////////////////////////////////////////////////////////////////
// MyPeripheralManagerDelegate - This is a little helper class so we can advertise our service

@interface MyPeripheralManagerDelegate : NSObject<CBPeripheralManagerDelegate> {
}

@property (nonatomic, assign) BluetoothAdvertiser* context;
@property (nonatomic, assign) bool active;
@property (nonatomic, assign) int readRequestCount;
@property (nonatomic, assign) int writeRequestCount;

@property (atomic, strong) CBPeripheralManager* peripheralManager;
@property (atomic, strong) CBUUID* serviceUuid;
@property (atomic, strong) NSString* serviceName;
@property (atomic, strong) NSData* serviceManufacturerInformation;
@property (atomic, strong) NSMutableArray* serviceCharacteristics;
@property (atomic, strong) CBMutableService* service;
@property (atomic, strong) NSDictionary* serviceDict;
@property (atomic, strong) NSMutableDictionary* readRequests;
@property (atomic, strong) NSMutableDictionary* writeRequests;

- (instancetype) initWithQueue:(dispatch_queue_t)btleQueue;
- (void) respondReadRequest:(NSString *)response forRequest:(int)request;
- (void) respondWriteRequest:(NSString *)response forRequest:(int)request;
- (bool) startAdvertising;

@end

@implementation MyPeripheralManagerDelegate

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
        self.readRequestCount = 0;
        self.readRequests = [[NSMutableDictionary alloc] initWithCapacity:10];
        self.writeRequestCount = 0;
        self.writeRequests = [[NSMutableDictionary alloc] initWithCapacity:10];
        self.active = false;
        if (btleQueue)
        {
            self.peripheralManager = [[CBPeripheralManager alloc] initWithDelegate:self queue:btleQueue];
        }
    }
    return self;
}

#pragma mark PERIPHERAL FUNCTIONS
- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral
{
    NSLog(@"CBPeripheralManager entered state %@", [MyPeripheralManagerDelegate stringFromCBManagerState:peripheral.state]);
    [self startAdvertising];
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral
                                       error:(NSError *)error
{
    if (error)
    {
        NSLog(@"Error advertising: %@", [error localizedDescription]);
    }
    NSLog(@"peripheralManagerDidStartAdvertising %d", self.peripheralManager.isAdvertising);
    if (self.peripheralManager.isAdvertising)
    {
        if (self.context->on_start())
        {
            NSLog(@"on_start: %d", self.active);
        }
    }
}

- (void) peripheralManager: (CBPeripheralManager *)peripheral
     didReceiveReadRequest: (CBATTRequest *)request
{
    NSString *uuid = request.characteristic.UUID.UUIDString;
    int readRequest = self.readRequestCount;
    NSNumber *read = [NSNumber numberWithInt:readRequest];
    [self.readRequests setObject:[NSValue valueWithPointer:(void *)request] forKey:read];
    self.readRequestCount += 1;
    if (!self.context->on_read(uuid.UTF8String, readRequest, request.central.identifier.UUIDString.UTF8String))
    {
        [self.peripheralManager respondToRequest:request withResult:CBATTErrorRequestNotSupported];
    }
    else
    {
        if ([self.readRequests objectForKey:read])
        {
            [self.peripheralManager respondToRequest:request withResult:CBATTErrorRequestNotSupported];
        }
    }
    [self.readRequests removeObjectForKey:read];
}

- (void) peripheralManager: (CBPeripheralManager *)peripheral
     didReceiveWriteRequests: (NSArray<CBATTRequest *> *)requests
{
    int count = requests.count;
    NSMutableArray *writes = [[NSMutableArray alloc] initWithCapacity:count];
    for (int i = 0; i < count; i++)
    {
        CBATTRequest *request = requests[i];
        int length = request.value.length;
        NSString *uuid = request.characteristic.UUID.UUIDString;
        int writeRequest = self.writeRequestCount;
        NSNumber *write = [NSNumber numberWithInt:writeRequest];
        Vector<uint8_t> value;
        value.resize(length);
        memcpy(value.ptrw(), request.value.bytes, length);
        [self.writeRequests setObject:[NSValue valueWithPointer:(void *)request] forKey:write];
        self.writeRequestCount += 1;
        self.context->on_write(uuid.UTF8String, writeRequest, request.central.identifier.UUIDString.UTF8String, core_bind::Marshalls::get_singleton()->raw_to_base64(value));
        [writes addObject:write];
    }
    count = writes.count;
    for (int i = 0; i < writes.count; i++)
    {
        NSNumber *key = writes[i];
        if ([self.writeRequests objectForKey:key])
        {
            count -= 1;
            [self.writeRequests removeObjectForKey:key];
        }
    }
    if (count < writes.count)
    {
        [self.peripheralManager respondToRequest:requests[0] withResult:CBATTErrorRequestNotSupported];
    }
    else
    {
        [self.peripheralManager respondToRequest:requests[0] withResult:CBATTErrorSuccess];
    }
}

- (void) respondReadRequest: (NSString *)response forRequest: (int)request
{
    NSNumber *key = [NSNumber numberWithInt:request];
    if ([self.readRequests objectForKey:key])
    {
        CBATTRequest *value = (CBATTRequest *)[(NSValue *)[self.readRequests objectForKey:key] pointerValue];
        if (value)
        {
            //[self.peripheralManager setDesiredConnectionLatency:CBPeripheralManagerConnectionLatencyLow forCentral:value.central];
            value.value = [response dataUsingEncoding:NSUTF8StringEncoding];
            [self.peripheralManager respondToRequest:value withResult:CBATTErrorSuccess];
            [self.readRequests removeObjectForKey:key];
        }
    }
}

- (void) respondWriteRequest: (NSString *)response forRequest: (int)request
{
    NSNumber *key = [NSNumber numberWithInt:request];
    if ([self.writeRequests objectForKey:key])
    {
        CBATTRequest *value = (CBATTRequest *)[(NSValue *)[self.writeRequests objectForKey:key] pointerValue];
        if (value)
        {
            //[self.peripheralManager setDesiredConnectionLatency:CBPeripheralManagerConnectionLatencyLow forCentral:value.central];
            //value.value = [response dataUsingEncoding:NSUTF8StringEncoding];
            [self.writeRequests removeObjectForKey:key];
        }
    }
}

- (bool) startAdvertising
{
    bool success = false;
    if (self.peripheralManager.state == CBManagerStatePoweredOn && self.active)
    {
        int count = self.context->get_characteristic_count();
        if (self.serviceUuid && self.serviceName && self.serviceManufacturerInformation)
        {
            self.serviceDict = @{
                CBAdvertisementDataLocalNameKey: self.serviceName,
                CBAdvertisementDataManufacturerDataKey: self.serviceManufacturerInformation,
                //            CBAdvertisementDataSolicitedServiceUUIDsKey: @[self.serviceUuid],
                CBAdvertisementDataServiceUUIDsKey: @[self.serviceUuid]
            };
            self.service = [[CBMutableService alloc] initWithType:self.serviceUuid primary:YES];
        }
        else
        {
            self.serviceDict = nil;
            self.service = nil;
        }
        if (count > 0)
        {
            self.serviceCharacteristics = [[NSMutableArray alloc] initWithCapacity:count];
            for (int i = 0; i < count; i++)
            {
                CBUUID *uuid = [CBUUID UUIDWithString:[[NSString alloc] initWithUTF8String:self.context->get_characteristic(i).utf8().get_data()]];
                CBMutableCharacteristic *characteristic = nil;
                if (self.context->get_characteristic_permission(self.context->get_characteristic(i))) {
                    characteristic = [[CBMutableCharacteristic alloc]
                                initWithType:uuid
                                properties:(CBCharacteristicPropertyRead|CBCharacteristicPropertyWrite)
                                value: nil
                                permissions:(CBAttributePermissionsReadable|CBAttributePermissionsWriteable)];
                } else {
                    characteristic = [[CBMutableCharacteristic alloc]
                                initWithType:uuid
                                properties:CBCharacteristicPropertyRead
                                value: nil
                                permissions:CBAttributePermissionsReadable];
                }
                [self.serviceCharacteristics addObject:characteristic];
            }
        }
        else
        {
            self.serviceCharacteristics = nil;
        }
        if (self.service && self.serviceCharacteristics)
        {
            self.service.characteristics = self.serviceCharacteristics;
            if (!self.peripheralManager.isAdvertising)
            {
                [self.peripheralManager addService:self.service];
                [self.peripheralManager startAdvertising:self.serviceDict];
                NSLog(@"startAdvertising. isAdvertising: %d", self.peripheralManager.isAdvertising);
                success = true;
            }
        }
        if (!success)
        {
            NSLog(@"Failure.");
        }
    }
    return success;
}

@end

BluetoothAdvertiserMacOS::BluetoothAdvertiserMacOS() {
    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            dispatch_queue_t btle_service = dispatch_queue_create("btle_service",  DISPATCH_QUEUE_SERIAL);
            MyPeripheralManagerDelegate *peripheral_manager_delegate = [[MyPeripheralManagerDelegate alloc] initWithQueue:btle_service];
            peripheral_manager_delegate.context = this;
            this->peripheral_manager_delegate = (__bridge_retained void *)peripheral_manager_delegate;
        }
    });
}

BluetoothAdvertiserMacOS::~BluetoothAdvertiserMacOS() {
    //MyPeripheralManagerDelegate *peripheral_manager_delegate = (__bridge MyPeripheralManagerDelegate *)this->peripheral_manager_delegate;
    CFRelease(this->peripheral_manager_delegate);
}

void BluetoothAdvertiserMacOS::respond_characteristic_read_request(String p_characteristic_uuid, String p_response, int p_request) const {
    [(__bridge MyPeripheralManagerDelegate *)peripheral_manager_delegate respondReadRequest:[[NSString alloc] initWithUTF8String:p_response.utf8().get_data()] forRequest:p_request];
}

void BluetoothAdvertiserMacOS::respond_characteristic_write_request(String p_characteristic_uuid, String p_response, int p_request) const {
    [(__bridge MyPeripheralManagerDelegate *)peripheral_manager_delegate respondWriteRequest:[[NSString alloc] initWithUTF8String:p_response.utf8().get_data()] forRequest:p_request];
}

bool BluetoothAdvertiserMacOS::start_advertising() const {
    bool success = false;
    MyPeripheralManagerDelegate *peripheral_manager_delegate = (__bridge MyPeripheralManagerDelegate *)this->peripheral_manager_delegate;
    peripheral_manager_delegate.active = true;
    if (get_characteristic_count() <= 0) {
        print_line("Advertise failure");
        return success;
    }
    peripheral_manager_delegate.serviceName = [[NSString alloc] initWithUTF8String:get_service_name().utf8().get_data()];
    peripheral_manager_delegate.serviceManufacturerInformation = [NSData dataWithBytes:get_service_manufacturer_information().ptr() length:get_service_manufacturer_information().size()];
    peripheral_manager_delegate.serviceUuid = [CBUUID UUIDWithString:[[NSString alloc] initWithUTF8String:get_service_uuid().utf8().get_data()]];
    if (peripheral_manager_delegate.peripheralManager.state != CBManagerStatePoweredOn) {
        return success;
    }
    if (!peripheral_manager_delegate.peripheralManager.isAdvertising) {
        success = [peripheral_manager_delegate startAdvertising];
    } else {
        on_start();
        success = true;
    }
    return success;
}

bool BluetoothAdvertiserMacOS::stop_advertising() const {
    MyPeripheralManagerDelegate *peripheral_manager_delegate = (__bridge MyPeripheralManagerDelegate *)this->peripheral_manager_delegate;
    peripheral_manager_delegate.active = false;
    if (peripheral_manager_delegate.peripheralManager.isAdvertising) {
        [peripheral_manager_delegate.peripheralManager removeAllServices];
        [peripheral_manager_delegate.peripheralManager stopAdvertising];
        on_stop();
        return true;
    }
    return false;
}

void BluetoothAdvertiserMacOS::on_register() const {
	// nothing to do here
}

void BluetoothAdvertiserMacOS::on_unregister() const {
	// nothing to do here
}
