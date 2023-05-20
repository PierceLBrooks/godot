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

@property (nonatomic, assign) CBUUID* serviceUuid;
@property (nonatomic, assign) CBUUID* characteristicUuid;
@property (nonatomic, assign) bool active;

@property (atomic, strong) CBPeripheralManager* peripheralManager;
@property (atomic, strong) CBCentral *currentCentral;
@property (atomic, strong) CBMutableCharacteristic *mainCharacteristic;
@property (atomic, strong) CBMutableService *service;
@property (atomic, strong) NSDictionary *serviceDict;

- (instancetype)initWithQueue:(dispatch_queue_t) bleQueue;
- (void)sendValue:(NSString *)data;

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
    self = [super init];
    if (self)
    {
        self.active = false;
        self.serviceUuid = [CBUUID UUIDWithString:@"29D7544B-6870-45A4-BB7E-D981535F4525"];
        self.characteristicUuid = [CBUUID UUIDWithString:@"B81672D5-396B-4803-82C2-029D34319015"];
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
    if (peripheral.state == CBManagerStatePoweredOn)
    {
        self.serviceDict = @{
            CBAdvertisementDataLocalNameKey: self.description,
            //            CBAdvertisementDataSolicitedServiceUUIDsKey: @[self.serviceUuid],
            CBAdvertisementDataServiceUUIDsKey: @[self.serviceUuid]
        };
        
        self.service = [[CBMutableService alloc] initWithType:self.serviceUuid primary:YES];
        
        self.mainCharacteristic = [[CBMutableCharacteristic alloc]
                               initWithType:self.characteristicUuid
                               properties:(
                                           CBCharacteristicPropertyRead |
                                           CBCharacteristicPropertyNotify // needed for didSubscribeToCharacteristic
                                           )
                               value: nil
                               permissions:CBAttributePermissionsReadable];
        
        self.service.characteristics = @[self.mainCharacteristic];
        if (self.active && !self.peripheralManager.isAdvertising)
        {
            [self.peripheralManager addService:self.service];
            [self.peripheralManager startAdvertising:self.serviceDict];
            NSLog(@"startAdvertising. isAdvertising: %d", self.peripheralManager.isAdvertising);
        }
    }
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
}

//------------------------------------------------------------------------------
- (void) peripheralManager: (CBPeripheralManager *)peripheral
     didReceiveReadRequest: (CBATTRequest *)request
{
    if ([request.characteristic.UUID isEqualTo:self.characteristicUuid])
    {
        [self.peripheralManager setDesiredConnectionLatency:CBPeripheralManagerConnectionLatencyLow
                                                 forCentral:request.central];
        NSString* description = @"ABCD";
        request.value = [description dataUsingEncoding:NSUTF8StringEncoding];
        [self.peripheralManager respondToRequest:request withResult:CBATTErrorSuccess];
        NSLog(@"didReceiveReadRequest:latencyCharacteristic. Responding with %@", description);
    }
    else
    {
        NSLog(@"didReceiveReadRequest: (unknown) %@", request);
    }
}

- (void)sendValue: (NSString *)data
{
    [self.peripheralManager updateValue:[data dataUsingEncoding:NSUTF8StringEncoding]
                      forCharacteristic:_mainCharacteristic
                   onSubscribedCentrals:@[_currentCentral]];
}
@end

//////////////////////////////////////////////////////////////////////////
// BluetoothAdvertiserMacOS - Subclass for bluetooth advertisers in macOS

class BluetoothAdvertiserMacOS : public BluetoothAdvertiser {
private:
	MyPeripheralManagerDelegate *peripheral_manager_delegate;
public:
	BluetoothAdvertiserMacOS();

	bool start_advertising() const override;
	bool stop_advertising() const override;

    void on_register() const override;
    void on_unregister() const override;
};

BluetoothAdvertiserMacOS::BluetoothAdvertiserMacOS() {
    active = true;
    @autoreleasepool
    {
        dispatch_queue_t ble_service = dispatch_queue_create("ble_test_service",  DISPATCH_QUEUE_SERIAL);
        peripheral_manager_delegate = [[MyPeripheralManagerDelegate alloc] initWithQueue:ble_service];
    }
};

bool BluetoothAdvertiserMacOS::start_advertising() const {
    if (!active || peripheral_manager_delegate.peripheralManager.state != CBManagerStatePoweredOn || get_characteristic_count() <= 0)
    {
        print_line("Advertise failure");
        return false;
    }
    @autoreleasepool
    {
        peripheral_manager_delegate.serviceUuid = [CBUUID UUIDWithString:[[NSString alloc] initWithUTF8String:get_service_uuid().utf8().get_data()]];
        peripheral_manager_delegate.characteristicUuid = [CBUUID UUIDWithString:[[NSString alloc] initWithUTF8String:get_characteristic(get_characteristic_count()-1).utf8().get_data()]];

        if (!peripheral_manager_delegate.peripheralManager.isAdvertising)
        {
            [peripheral_manager_delegate.peripheralManager addService:peripheral_manager_delegate.service];
            [peripheral_manager_delegate.peripheralManager startAdvertising:peripheral_manager_delegate.serviceDict];
        }

        print_line("Sending");
        while (![peripheral_manager_delegate currentCentral]);

        [peripheral_manager_delegate sendValue:@""];
        print_line("Sent");
    }
	return true;
};

bool BluetoothAdvertiserMacOS::stop_advertising() const {
    // nothing to do here
    return true;
};

void BluetoothAdvertiserMacOS::on_register() const {
    start_advertising();
}

void BluetoothAdvertiserMacOS::on_unregister() const {
	// nothing to do here
}

BluetoothMacOS::BluetoothMacOS() {
    if (Engine::get_singleton()->is_editor_hint() || Engine::get_singleton()->is_project_manager_hint()) {
        return;
    }
    Ref<BluetoothAdvertiserMacOS> advertiser;
    advertiser.instantiate();
    advertiser->set_service_uuid("29D7544B-6870-45A4-BB7E-D981535F4525");
    advertiser->add_characteristic("B81672D5-396B-4803-82C2-029D34319015");
    add_advertiser(advertiser);
};
