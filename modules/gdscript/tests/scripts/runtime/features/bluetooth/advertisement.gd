extends Node2D


var register: String = "ABCD"
var advertiser: BluetoothAdvertiser = null

func _on_bluetooth_service_advertisement_started(service_uuid):
	print("start: "+service_uuid)

func _on_bluetooth_service_characteristic_read(service_uuid, characteristic_uuid, request, peer):
	print("read: "+service_uuid+" @ "+str(characteristic_uuid)+" = "+peer)
	advertiser.respond_characteristic_read_request(characteristic_uuid, register, request)

func _on_bluetooth_service_characteristic_write(service_uuid, characteristic_uuid, request, peer, value):
	register = value
	print("write: "+service_uuid+" @ "+str(characteristic_uuid)+" = "+peer+" & "+value)
	advertiser.respond_characteristic_write_request(characteristic_uuid, register, request)

# Called when the node enters the scene tree for the first time.
func _ready():
	advertiser = BluetoothAdvertiser.new()
	advertiser.set_service_manufacturer_information(Marshalls.base64_to_raw("SGVsbG8sIHdvcmxkIQ=="))
	advertiser.set_service_name("btle_service")
	advertiser.set_service_uuid("29D7544B-6870-45A4-BB7E-D981535F4525")
	advertiser.set_characteristic_permission("B81672D5-396B-4803-82C2-029D34319015", true);
	advertiser.add_characteristic("B81672D5-396B-4803-82C2-029D34319015")
	advertiser.connect("bluetooth_service_advertisement_started", _on_bluetooth_service_advertisement_started)
	advertiser.connect("bluetooth_service_characteristic_read", _on_bluetooth_service_characteristic_read)
	advertiser.connect("bluetooth_service_characteristic_write", _on_bluetooth_service_characteristic_write)
	while not (advertiser.is_active()):
		await get_tree().create_timer(1.0).timeout
		advertiser.set_active(true)
	return


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	pass


func _exit_tree():
	advertiser = null

