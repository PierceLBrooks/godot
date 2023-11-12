extends Node2D

var enumerator: BluetoothEnumerator = null

func _on_bluetooth_enumerator_added(id):
	print("add: "+str(id))

func _on_bluetooth_service_enumeration_started(id):
	print("start: "+str(id))

func _on_bluetooth_service_enumeration_stopped(id):
	print("stop: "+str(id))

func _on_bluetooth_peer_discovered(id, peer, name, data):
	print("discover: "+str(id)+" @ "+str(peer)+" & "+str(name)+" & "+str(JSON.stringify(str(data))))
	if ("Mac" in name):
		enumerator.connect_peer(peer)

func _on_bluetooth_peer_connected(id, peer):
	print("connect: "+str(id)+" @ "+str(peer))

func _on_bluetooth_peer_disconnected(id, peer):
	print("disconnect: "+str(id)+" @ "+str(peer))

func _on_bluetooth_peer_characteristic_discovered(id, peer, service, characteristic, writable):
	print("characteristic: "+str(id)+" @ "+str(peer)+" & "+str(service)+" & "+str(characteristic))
	if (writable and service == "29D7544B-6870-45A4-BB7E-D981535F4525" and characteristic == "B81672D5-396B-4803-82C2-029D34319015"):
		BluetoothEnumerator.write_peer_service_characteristic(peer, service, characteristic, "Hello, world!")

func _on_bluetooth_peer_characteristic_read(id, peer, service, characteristic, value):
	print("read: "+str(id)+" @ "+str(peer)+" & "+str(service)+" & "+str(characteristic)+" = "+str(value))

func _on_bluetooth_peer_characteristic_wrote(id, peer, service, characteristic):
	print("write: "+str(id)+" @ "+str(peer)+" & "+str(service)+" & "+str(characteristic))
	enumerator.read_peer_service_characteristic(peer, service, characteristic)

func _on_bluetooth_peer_characteristic_error(id, peer, service, characteristic):
	print("error: "+str(id)+" @ "+str(peer)+" & "+str(service)+" & "+str(characteristic))

# Called when the node enters the scene tree for the first time.
func _ready():
	#noob.add_sought_service("ADE3D529-C784-4F63-A987-EB69F70EE816")
	enumerator = BluetoothEnumerator.new()
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

