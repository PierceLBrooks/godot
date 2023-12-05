extends Node2D

var enumerator: BluetoothEnumerator = null

func _on_bluetooth_service_enumeration_started(sought_services):
	print("start: "+str(sought_services))

func _on_bluetooth_service_enumeration_stopped(sought_services):
	print("stop: "+str(sought_services))

func _on_bluetooth_peer_discovered(sought_services, peer, name, data):
	print("discover: "+str(peer)+" & "+str(name)+" & "+str(JSON.stringify(data)))
	if ("Samsung" in name):
		enumerator.connect_peer(peer)

func _on_bluetooth_peer_connected(sought_services, peer):
	print("connect: "+str(peer))

func _on_bluetooth_peer_disconnected(sought_services, peer):
	print("disconnect: "+str(peer))

func _on_bluetooth_peer_characteristic_discovered(sought_services, peer, service, characteristic, writable):
	print("characteristic: "+str(peer)+" & "+str(service)+" & "+str(characteristic))
	if (writable and service == "29D7544B-6870-45A4-BB7E-D981535F4525" and characteristic == "B81672D5-396B-4803-82C2-029D34319015"):
		enumerator.write_peer_service_characteristic(peer, service, characteristic, "Hello, world!")

func _on_bluetooth_peer_characteristic_read(sought_services, peer, service, characteristic, value):
	print("read: "+str(peer)+" & "+str(service)+" & "+str(characteristic)+" = "+str(value))

func _on_bluetooth_peer_characteristic_wrote(sought_services, peer, service, characteristic):
	print("write: "+str(peer)+" & "+str(service)+" & "+str(characteristic))
	enumerator.read_peer_service_characteristic(peer, service, characteristic)

func _on_bluetooth_peer_characteristic_error(sought_services, peer, service, characteristic):
	print("error: "+str(peer)+" & "+str(service)+" & "+str(characteristic))

# Called when the node enters the scene tree for the first time.
func _ready():
	#noob.add_sought_service("ADE3D529-C784-4F63-A987-EB69F70EE816")
	enumerator = BluetoothEnumerator.new()
	print(enumerator.get_device_name())
	print(enumerator.get_device_address())
	enumerator.connect("bluetooth_service_enumeration_started", _on_bluetooth_service_enumeration_started)
	enumerator.connect("bluetooth_service_enumeration_stopped", _on_bluetooth_service_enumeration_stopped)
	enumerator.connect("bluetooth_peer_discovered", _on_bluetooth_peer_discovered)
	enumerator.connect("bluetooth_peer_connected", _on_bluetooth_peer_connected)
	enumerator.connect("bluetooth_peer_disconnected", _on_bluetooth_peer_disconnected)
	enumerator.connect("bluetooth_peer_characteristic_discovered", _on_bluetooth_peer_characteristic_discovered)
	enumerator.connect("bluetooth_peer_characteristic_read", _on_bluetooth_peer_characteristic_read)
	enumerator.connect("bluetooth_peer_characteristic_wrote", _on_bluetooth_peer_characteristic_wrote)
	enumerator.connect("bluetooth_peer_characteristic_error", _on_bluetooth_peer_characteristic_error)
	while not (enumerator.is_active()):
		await get_tree().create_timer(1.0).timeout
		enumerator.set_active(true)
	return


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	pass


func _exit_tree():
	enumerator = null

