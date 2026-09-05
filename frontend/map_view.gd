extends Node3D

signal connection_established
signal connection_closed
signal data_received(message: String)

@export var HOST: String = "127.0.0.1"
@export var PORT: int = 6253

@export var camera: Camera3D

@export var statusDisplay: RichTextLabel
@export var topObjectPopup: RichTextLabel
@export var topObjectPopupWidth: float
@export var interfaceBackground: ColorRect

@export var interfaceAnimWeight: float

#@export var templatePoint: Node3D
@export var pointMesh: Mesh # Assign a small SphereMesh, BoxMesh, or QuadMesh in the Inspector
@export var pointMaterial: StandardMaterial3D

@export var refetchInterval: float

# --- Outline style controls ---
#@export var outline_color: Color = Color.WHITE
@export var line_width: float = 0.05

var tcp_client: StreamPeerTCP = StreamPeerTCP.new()
var last_status: int = StreamPeerTCP.STATUS_NONE

var reconstruction_thread: Thread

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	interfaceBackground.visible = true
	
	# Initialize status tracking
	#statusDisplay.text = "Initializing client..."
	last_status = tcp_client.get_status()
	
	received_data_accumulated = ""
	
	connection_established.connect(_proc_connection_established)
	data_received.connect(_proc_data_received)
	
	reconstruction_thread = Thread.new()
	
	_setup_multimesh()
	connect_to_server()

var refetchTimePassed: float
var currentShipMetadata: Dictionary

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	# Add current delta time to refetch interval
	refetchTimePassed += delta
	
	statusDisplay.text = "%.2f" % (1 / delta) + " FPS (" + ("%.2f" % (delta * 1000)) + " ms)"
	
	# Continuously poll the socket status changes
	tcp_client.poll()
	var current_status = tcp_client.get_status()
	
	if current_status != last_status:
		_on_status_changed(current_status)
		last_status = current_status

	# Read incoming data if the client remains connected
	if current_status == StreamPeerTCP.STATUS_CONNECTED:
		var available_bytes: int = tcp_client.get_available_bytes()
		if available_bytes > 0:
			var data = tcp_client.get_data(available_bytes)
			var error_code = data[0]
			var byte_array = data[1]
			
			if error_code == OK:
				var message: String = byte_array.get_string_from_utf8()
				emit_signal("data_received", message)
		elif refetchTimePassed > refetchInterval:
			# If there is no data to be received and refetch time passed comes
			send_message("query:AIS:updates")
			refetchTimePassed = 0
			
	
	if camera.currentlyFocused:
		topObjectPopup.visible = true
		if topObjectPopup.size.x < topObjectPopupWidth - 2.1:
			topObjectPopup.size.x = lerp(topObjectPopup.size.x, topObjectPopupWidth, interfaceAnimWeight * delta)
			topObjectPopup.offset_transform_position.x = (topObjectPopupWidth - topObjectPopup.size.x) / 2 + (-topObjectPopupWidth / 2)
			topObjectPopup.text = "[ ... ]"
		else:
			topObjectPopup.size.x = topObjectPopupWidth
			topObjectPopup.offset_transform_position.x = -topObjectPopupWidth / 2
			if currentShipMetadata:
				if currentShipMetadata["Name"]:
					topObjectPopup.text = currentShipMetadata["Name"].strip_edges()
				elif currentShipMetadata["IMO"]:
					topObjectPopup.text = "IMO: " + str(currentShipMetadata["IMO"])
				else:
					topObjectPopup.text = "MMSI: " + str(currentShipMetadata["MMSI"])
	else:
		#print(topObjectPopup.size.x)
		if topObjectPopup.size.x > 2.1:
			topObjectPopup.text = "[ CLOSED ]"
			topObjectPopup.size.x = lerp(topObjectPopup.size.x, 0.0, interfaceAnimWeight * delta)
			
			# FIX: Changed 200 to topObjectPopupWidth to match the opening animation
			topObjectPopup.offset_transform_position.x = (topObjectPopupWidth - topObjectPopup.size.x) / 2 + (-topObjectPopupWidth / 2)
		else: 
			topObjectPopup.visible = false

# Returned data processing part
# This part of the code handles when the data gets Received, parsed, and displayed.
# Also is responsible for running query data when first connected to server
func _proc_connection_established():
	print("Connection established. Querying data first thing yeah")
	#statusDisplay.text += "\nQuerying world reconstruction data..."
	send_message("query:GEODATA")

var received_data_accumulated: String = ""

func _proc_data_received(data: String):
	if data.right(5) != "\nEND\n":
		received_data_accumulated += data
	else:
		received_data_accumulated += data.left(-5)
		
		var dataDict = JSON.parse_string(received_data_accumulated)
		if not dataDict: return
		
		if (dataDict["type"] == "FeatureCollection"):
			#statusDisplay.text += "\nParsed data. [b][i]Reconstructing world[/i][/b]..."
			reconstruction_thread.start(_reconstruct_world.bind(dataDict))
		elif dataDict["type"] == "AISUpdates":
			#statusDisplay.text += "\nAIS update.";
			#print("AIS UPDATE: ", dataDict)
			_update_ais(dataDict)
		elif dataDict["type"] == "AISInfo":
			currentShipMetadata = dataDict["data"]
			
		received_data_accumulated = ""

var multimesh_node: MultiMeshInstance3D
var multimesh: MultiMesh

# Lookup structures for 100k+ points
var keyToIndex: Dictionary = {}        # vessel_id -> index
var indexToKey: Array[String] = []     # index -> vessel_id
var pointPositions: PackedVector3Array = PackedVector3Array() # For instant CPU click checks

var bufferCapacity: int = 10000 # Initial capacity, grows dynamically

func _setup_multimesh():
	multimesh_node = MultiMeshInstance3D.new()
	multimesh_node.position.y = 0.1
	multimesh_node.material_override = pointMaterial;
	multimesh = MultiMesh.new()
	
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.mesh = pointMesh if pointMesh else SphereMesh.new()
	multimesh.instance_count = bufferCapacity
	multimesh.visible_instance_count = 0
	
	multimesh_node.multimesh = multimesh
	add_child(multimesh_node)

func _update_ais(data: Dictionary):
	if not data.has("data") or not data["data"]: 
		return
		
	var ais_data: Dictionary = data["data"]
	
	for key in ais_data:
		var item: Dictionary = ais_data[key]
		var pos := Vector3(-item["Lon"], 0, item["Lat"]) # Note: Ensure sign consistency for Lon
		
		if key not in keyToIndex:
			_add_vessel(key, pos)
		else:
			_update_vessel(keyToIndex[key], pos)

func _add_vessel(key: String, pos: Vector3):
	var idx: int = indexToKey.size()
	
	# Grow internal buffer in chunks if capacity limit is reached
	if idx >= multimesh.instance_count:
		bufferCapacity *= 2
		multimesh.instance_count = bufferCapacity

	keyToIndex[key] = idx
	indexToKey.append(key)
	pointPositions.append(pos)
	
	var basis_up := Basis().rotated(Vector3.RIGHT, deg_to_rad(-90.0))
	
	# Set transform on GPU
	var t := Transform3D(basis_up, pos)
	multimesh.set_instance_transform(idx, t)
	
	# Reveal newly added instance
	multimesh.visible_instance_count = indexToKey.size()

func _update_vessel(idx: int, pos: Vector3):
	pointPositions[idx] = pos
	
	var basis_up := Basis().rotated(Vector3.RIGHT, deg_to_rad(-90.0))
	var t := Transform3D(basis_up, pos)
	multimesh.set_instance_transform(idx, t)
	
func focus_vessel(mmsi: String):
	currentShipMetadata = {}
	print(mmsi)
	send_message("query:AIS:MMSI:" + mmsi)

func _reconstruct_world(data: Dictionary):
	#var templatePointDeferred = templatePoint.call_deferred("duplicate", true)
	
	for country in data["features"]:
		#print(country["properties"]["ADMIN"])
		var outline_color: Color = Color.from_hsv(randf(), 0.4, 1.0)
		
		if country["geometry"]["type"] == "Polygon":
			for coords_set in country["geometry"]["coordinates"]:
				var vertices: PackedVector2Array
				
				for coords in coords_set:
					vertices.append(Vector2(coords[0], coords[1]))
					
				draw_outline(vertices, outline_color, line_width)

		elif country["geometry"]["type"] == "MultiPolygon":
			for polygon in country["geometry"]["coordinates"]:
				for coords_set in polygon:
					var vertices: PackedVector2Array
					
					for coords in coords_set:
						vertices.append(Vector2(coords[0], coords[1]))
					
					draw_outline(vertices, outline_color, line_width)
					
	# World reconstructed. It's time to let the user interact and do stuff?
	interfaceBackground.set_deferred("visible", false)

func generate_random_string(length: int) -> String:
	var chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
	var result = ""
	
	for i in range(length):
		var random_index = randi() % chars.length()
		result += chars[random_index]
		
	return result

# Draws a closed border/outline for a ring of points instead of a filled polygon.
# Built as a ribbon of small quads (rather than a raw line primitive) because
# most Godot 4 renderers ignore GL line-width, so a real line would always be 1px
# regardless of `line_width`. This way the thickness is controllable and consistent.
func draw_outline(points: PackedVector2Array, color: Color = Color.WHITE, width: float = 0.05) -> void:
	if points.size() < 2:
		push_error("Not enough points to draw an outline.")
		return
	
	var st = SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	st.set_color(color)
	
	var point_count = points.size()
	var half_width = width * 0.5
	
	for i in range(point_count):
		var current = points[i]
		var next = points[(i + 1) % point_count] # wraps around to close the loop
		
		if current.is_equal_approx(next):
			continue
		
		var direction: Vector2 = (next - current).normalized()
		var normal: Vector2 = Vector2(-direction.y, direction.x) * half_width
		
		var v1 = current + normal
		var v2 = current - normal
		var v3 = next + normal
		var v4 = next - normal
		
		var v1_3d = Vector3(v1.x, 0, v1.y)
		var v2_3d = Vector3(v2.x, 0, v2.y)
		var v3_3d = Vector3(v3.x, 0, v3.y)
		var v4_3d = Vector3(v4.x, 0, v4.y)
		
		# Two triangles forming this segment's quad
		st.set_normal(Vector3.UP)
		st.add_vertex(v1_3d)
		st.add_vertex(v2_3d)
		st.add_vertex(v3_3d)
		
		st.set_normal(Vector3.UP)
		st.add_vertex(v2_3d)
		st.add_vertex(v4_3d)
		st.add_vertex(v3_3d)
		
	var new_mesh = st.commit()
	var mesh_instance: MeshInstance3D = MeshInstance3D.new()
	mesh_instance.mesh = new_mesh
	mesh_instance.rotation = Vector3(0, 0, PI)
	mesh_instance.name = "CountryMesh." + generate_random_string(6)
	
	var material: ORMMaterial3D = ORMMaterial3D.new()
	material.albedo_color = color
	material.shading_mode = ORMMaterial3D.SHADING_MODE_UNSHADED
	material.cull_mode = ORMMaterial3D.CULL_DISABLED
	mesh_instance.material_override = material
	
	mesh_instance.position = Vector3(0, 0, 0)
	
	self.call_deferred("add_child", mesh_instance)
		
# Initiate connection part
func connect_to_server() -> void:
	print("Connecting to %s:%d..." % [HOST, PORT])
	#statusDisplay.text = "Connecting to %s:%d" % [HOST, PORT]
	var error = tcp_client.connect_to_host(HOST, PORT)
	if error != OK:
		print("Failed to initialize connection attempt: ", error)
		#statusDisplay.text = "Failed to initialize connection attempt: " + error

# Send string data encoded in UTF-8
func send_message(message: String) -> void:
	if tcp_client.get_status() == StreamPeerTCP.STATUS_CONNECTED:
		var byte_array: PackedByteArray = message.to_utf8_buffer()
		var error = tcp_client.put_data(byte_array)
		if error != OK:
			print("Failed to send data: ", error)
	else:
		print("Cannot send data. Client is not connected.")

# Handle internal connection states
func _on_status_changed(new_status: int) -> void:
	match new_status:
		StreamPeerTCP.STATUS_NONE:
			interfaceBackground.visible = true
			
			print("Disconnected from server.")
			#statusDisplay.text = "Disconnected."
			emit_signal("connection_closed")
		StreamPeerTCP.STATUS_CONNECTING:
			interfaceBackground.visible = true
			
			print("Connecting...")
			#statusDisplay.text = "Connecting..."
		StreamPeerTCP.STATUS_CONNECTED:
			print("Successfully connected to server!")
			#statusDisplay.text = "[color=green]Connected[/color] to server."
			tcp_client.set_no_delay(true) # Disables Nagle's algorithm for reduced latency
			emit_signal("connection_established")
		StreamPeerTCP.STATUS_ERROR:
			interfaceBackground.visible = true
			
			print("Connection error encountered.")
			#statusDisplay.text = "Connection error. Closed."
			emit_signal("connection_closed")

func _exit_tree() -> void:
	# Gracefully terminate the socket when the node leaves the tree
	tcp_client.disconnect_from_host()
	if reconstruction_thread and reconstruction_thread.is_started():
		reconstruction_thread.wait_to_finish()
