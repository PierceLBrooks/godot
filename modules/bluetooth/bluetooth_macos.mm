/**************************************************************************/
/*  bluetooth_macos.mm                                                    */
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

#include "bluetooth_macos.h"
#include "servers/bluetooth/bluetooth_advertiser.h"
#include "core/config/engine.h"

#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>

//////////////////////////////////////////////////////////////////////////
// MyPeripheralManagerDelegate - This is a little helper class so we can advertise our service

@interface MyPeripheralManagerDelegate : NSObject<CBPeripheralManagerDelegate> {
}

@property (nonatomic, assign) BluetoothAdvertiser* context;
@property (nonatomic, assign) bool active;
@property (nonatomic, assign) int readRequestCount;

@property (atomic, strong) CBUUID* serviceUuid;
@property (atomic, strong) CBUUID* characteristicUuid;
@property (atomic, strong) CBPeripheralManager* peripheralManager;
@property (atomic, strong) CBCentral* currentCentral;
@property (atomic, strong) CBMutableCharacteristic* mainCharacteristic;
@property (atomic, strong) CBMutableService* service;
@property (atomic, strong) NSDictionary* serviceDict;
@property (atomic, strong) NSMutableDictionary* readRequests;

- (instancetype) initWithQueue:(dispatch_queue_t)bleQueue;
- (void) sendValue:(NSString *)data;
- (void) respondReadRequest:(NSString *)response forRequest:(int)request;
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

//------------------------------------------------------------------------------
#pragma mark init funcs

- (instancetype)init
{
    return [self initWithQueue:nil];
}

- (instancetype)initWithQueue:(dispatch_queue_t) bleQueue
{
    //NSLog(@"init %d", [NSThread isMultiThreaded]);
    self = [super init];
    if (self)
    {
        self.readRequestCount = 0;
        self.readRequests = [[NSMutableDictionary alloc] initWithCapacity:10];
        self.active = false;
        if (bleQueue)
        {
            self.peripheralManager = [[CBPeripheralManager alloc] initWithDelegate:self queue:bleQueue];
        }
    }
    return self;
}

#pragma mark CENTRAL FUNCTIONS
- (void) peripheralManager:(CBPeripheralManager *)peripheral
                   central:(CBCentral *)central didSubscribeToCharacteristic:(CBCharacteristic *)characteristic
{
    _currentCentral = central;
}

- (void) peripheralManager: (CBPeripheralManager *)peripheral
                   central: (CBCentral *)central didUnsubscribeFromCharacteristic:(CBCharacteristic *)characteristic
{
    _currentCentral = nil;
}

- (void)peripheralManagerIsReadyToUpdateSubscribers:(CBPeripheralManager *)peripheral
{
    NSLog(@"------------------Ready To Update------------------");
}

//------------------------------------------------------------------------------
- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral
{
    NSLog(@"CBPeripheralManager entered state %@", [MyPeripheralManagerDelegate stringFromCBManagerState:peripheral.state]);
    [self startAdvertising];
}

//------------------------------------------------------------------------------
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
            NSLog(@"on_start %d", self.active);
        }
    }
}

//------------------------------------------------------------------------------
- (void) peripheralManager: (CBPeripheralManager *)peripheral
     didReceiveReadRequest: (CBATTRequest *)request
{
    NSString *uuid = request.characteristic.UUID.UUIDString;
    int readRequest = self.readRequestCount;
    [self.readRequests setObject:[NSValue valueWithPointer:(void *)request] forKey:[NSNumber numberWithInt:readRequest]];
    self.readRequestCount += 1;
    if (!self.context->on_read(uuid.UTF8String, readRequest))
    {
        NSLog(@"didReceiveReadRequest: (unknown) %@", request);
        [self.peripheralManager respondToRequest:request withResult:CBATTErrorRequestNotSupported];
    }
}

- (void) respondReadRequest: (NSString *)response forRequest: (int)request
{
    NSNumber *key = [NSNumber numberWithInt:request];
    CBATTRequest *value = (CBATTRequest *)[(NSValue *)[self.readRequests objectForKey:key] pointerValue];
    [self.peripheralManager setDesiredConnectionLatency:CBPeripheralManagerConnectionLatencyLow
                                                 forCentral:value.central];
    value.value = [response dataUsingEncoding:NSUTF8StringEncoding];
    NSLog(@"didReceiveReadRequest:latencyCharacteristic. Responding with \"%@\"", response);
    [self.peripheralManager respondToRequest:value withResult:CBATTErrorSuccess];
    [self.readRequests removeObjectForKey:key];
}

- (void) sendValue: (NSString *)data
{
    [self.peripheralManager updateValue:[data dataUsingEncoding:NSUTF8StringEncoding]
                      forCharacteristic:_mainCharacteristic
                   onSubscribedCentrals:@[_currentCentral]];
}

- (bool) startAdvertising
{
    bool success = false;
    if (self.peripheralManager.state == CBManagerStatePoweredOn && self.active)
    {
        if (self.serviceUuid)
        {
            self.serviceDict = @{
                CBAdvertisementDataLocalNameKey: self.description,
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
        if (self.characteristicUuid)
        {
            self.mainCharacteristic = [[CBMutableCharacteristic alloc]
                                initWithType:self.characteristicUuid
                                properties:(
                                            CBCharacteristicPropertyRead |
                                            CBCharacteristicPropertyNotify // needed for didSubscribeToCharacteristic
                                            )
                                value: nil
                                permissions:CBAttributePermissionsReadable];
        }
        else
        {
            self.mainCharacteristic = nil;
        }
        if (self.service && self.mainCharacteristic)
        {
            self.service.characteristics = @[self.mainCharacteristic];
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

//////////////////////////////////////////////////////////////////////////
// BluetoothAdvertiserMacOS - Subclass for bluetooth advertisers in macOS

class BluetoothAdvertiserMacOS : public BluetoothAdvertiser {
private:
	MyPeripheralManagerDelegate *peripheral_manager_delegate;
public:
	BluetoothAdvertiserMacOS();

    void respond_characteristic_read_request(String p_characteristic_uuid, String p_response, int p_request) const override;

	bool start_advertising() const override;
	bool stop_advertising() const override;

    void on_register() const override;
    void on_unregister() const override;
};

BluetoothAdvertiserMacOS::BluetoothAdvertiserMacOS() {
    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            dispatch_queue_t ble_service = dispatch_queue_create("ble_test_service",  DISPATCH_QUEUE_SERIAL);
            peripheral_manager_delegate = [[MyPeripheralManagerDelegate alloc] initWithQueue:ble_service];
            peripheral_manager_delegate.context = this;
        }
    });
}

void BluetoothAdvertiserMacOS::respond_characteristic_read_request(String p_characteristic_uuid, String p_response, int p_request) const {
    [peripheral_manager_delegate respondReadRequest:[[NSString alloc] initWithUTF8String:p_response.utf8().get_data()] forRequest:p_request];
}

bool BluetoothAdvertiserMacOS::start_advertising() const {
    bool success = false;
    peripheral_manager_delegate.active = true;
    if (get_characteristic_count() <= 0) {
        print_line("Advertise failure");
        return success;
    }
    peripheral_manager_delegate.serviceUuid = [CBUUID UUIDWithString:[[NSString alloc] initWithUTF8String:get_service_uuid().utf8().get_data()]];
    peripheral_manager_delegate.characteristicUuid = [CBUUID UUIDWithString:[[NSString alloc] initWithUTF8String:get_characteristic(get_characteristic_count()-1).utf8().get_data()]];
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

BluetoothMacOS::BluetoothMacOS() {
}

Ref<BluetoothAdvertiser> BluetoothMacOS::new_advertiser() {
    if (Engine::get_singleton()->is_editor_hint() || Engine::get_singleton()->is_project_manager_hint()) {
        return nullptr;
    }
    int count = get_advertiser_count();
    Ref<BluetoothAdvertiserMacOS> advertiser;
    advertiser.instantiate();
    add_advertiser(advertiser);
    return get_advertiser(count);
}
