extends Node3D

signal connection_established
signal connection_closed
signal data_received(message: String)

@export var HOST: String = "127.0.0.1"
@export var PORT: int = 6253

@export var statusDisplay: RichTextLabel
@export var interfaceBackground: ColorRect

@export var templatePoint: Node3D

var tcp_client: StreamPeerTCP = StreamPeerTCP.new()
var last_status: int = StreamPeerTCP.STATUS_NONE

var reconstruction_thread: Thread

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	interfaceBackground.visible = true
	
	# Initialize status tracking
	statusDisplay.text = "Initializing client..."
	last_status = tcp_client.get_status()
	
	received_data_accumulated = ""
	
	connection_established.connect(_proc_connection_established)
	data_received.connect(_proc_data_received)
	
	reconstruction_thread = Thread.new()
	
	connect_to_server()

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
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

# Returned data processing part
# This part of the code handles when the data gets recieved, parsed, and displayed.
# Also is responsible for running query data when first connected to server
func _proc_connection_established():
	print("Connection established. Querying data first thing yeah")
	statusDisplay.text += "\nQuerying world reconstruction data..."
	send_message("query:GEODATA")

var received_data_accumulated: String = ""

func _proc_data_received(data: String):
	if data.right(5) != "\nEND\n":
		received_data_accumulated += data
	else:
		received_data_accumulated += data.left(-5)
		
		var dataDict = JSON.parse_string(received_data_accumulated)
		if (dataDict["type"] == "FeatureCollection"):
			statusDisplay.text += "\nParsed data. [b][i]Reconstructing world[/i][/b]..."
			reconstruction_thread.start(_reconstruct_world.bind(dataDict))
			
		received_data_accumulated = ""

func _reconstruct_world(data: Dictionary):
	#var templatePointDeferred = templatePoint.call_deferred("duplicate", true)
	
	for country in data["features"]:
		#print(country["properties"]["ADMIN"])
		var color: Color = Color.from_hsv(randf(), 0.5, 1.0)
		
		if country["geometry"]["type"] == "Polygon":
			#var vertices: PackedVector2Array
			for coords_set in country["geometry"]["coordinates"]:
				var vertices: PackedVector2Array
				
				for coords in coords_set:
					#print(coords)
					vertices.append(Vector2(coords[0], coords[1]))
					
					#var newPoint = templatePoint.call_deferred("duplicate", true)
					#newPoint.position = Vector3(coords[0], 0, coords[1])
#
					##self.add_child(newPoint)
					#call_deferred("add_child", newPoint)
					
				draw_polygon(vertices, color)

		elif country["geometry"]["type"] == "MultiPolygon":
			for polygon in country["geometry"]["coordinates"]:
				for coords_set in polygon:
					var vertices: PackedVector2Array
					
					for coords in coords_set:
						#print(coords)
						vertices.append(Vector2(coords[0], coords[1]))
						
						#var newPoint = templatePoint.call_deferred("duplicate", true)
						#newPoint.position = Vector3(coords[0], 0, coords[1])
#
						##self.add_child(newPoint)
						#call_deferred("add_child", newPoint)
					
					draw_polygon(vertices, color)
					
	# World reconstructed. It's time to let the user interact and do stuff?
	interfaceBackground.set_deferred("visible", false)
	
func draw_polygon(points: PackedVector2Array, color: Color):
	var indices = Geometry2D.triangulate_polygon(points)
	
	if indices.is_empty():
		push_error("Failed to triangulate polygon. Make sure the points do not cross over each other!")
		return
		
	var st = SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	
	st.set_color(color)
	
	for i in range(0, indices.size(), 3):
		# Get idx layout of one triangle
		var idx1 = indices[i]
		var idx2 = indices[i + 1]
		var idx3 = indices[i + 2]
		
		# Pull down the 2D coords
		var point1_2d = points[idx1]
		var point2_2d = points[idx2]
		var point3_2d = points[idx3]
		
		# Converting Y axis to Z axis
		var point1_3d = Vector3(point1_2d.x, 0, point1_2d.y)
		var point2_3d = Vector3(point2_2d.x, 0, point2_2d.y)
		var point3_3d = Vector3(point3_2d.x, 0, point3_2d.y)
		
		# Calculate basic flat normals so lighting works...
		# I guess...? I don't know man, this is weird as hell.
		# Gemini said so so...
		var normal = (point2_3d - point1_3d).cross(point3_3d - point1_3d).normalized()
		
		# Add vertex 1
		st.set_normal(normal)
		st.add_vertex(point1_3d)
		
		# Add vertex 2
		st.set_normal(normal)
		st.add_vertex(point2_3d)
		
		# Add vertex 3
		st.set_normal(normal)
		st.add_vertex(point3_3d)
		
	var new_mesh = st.commit()
	var mesh_instance: MeshInstance3D = MeshInstance3D.new()
	mesh_instance.mesh = new_mesh
	
	var material: ORMMaterial3D = ORMMaterial3D.new()
	material.albedo_color = color
	material.shading_mode = ORMMaterial3D.SHADING_MODE_UNSHADED
	material.cull_mode = ORMMaterial3D.CULL_DISABLED
	mesh_instance.material_override = material
	
	mesh_instance.position = Vector3(0, 0, 0)
	
	#add_child(mesh_instance)
	#print(points)
	self.call_deferred("add_child", mesh_instance)
		
# Initiate connection part
func connect_to_server() -> void:
	print("Connecting to %s:%d..." % [HOST, PORT])
	statusDisplay.text = "Connecting to %s:%d" % [HOST, PORT]
	var error = tcp_client.connect_to_host(HOST, PORT)
	if error != OK:
		print("Failed to initialize connection attempt: ", error)
		statusDisplay.text = "Failed to initialize connection attempt: " + error

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
			statusDisplay.text = "Disconnected."
			emit_signal("connection_closed")
		StreamPeerTCP.STATUS_CONNECTING:
			interfaceBackground.visible = true
			
			print("Connecting...")
			statusDisplay.text = "Connecting..."
		StreamPeerTCP.STATUS_CONNECTED:
			print("Successfully connected to server!")
			statusDisplay.text = "[color=green]Connected[/color] to server."
			tcp_client.set_no_delay(true) # Disables Nagle's algorithm for reduced latency
			emit_signal("connection_established")
		StreamPeerTCP.STATUS_ERROR:
			interfaceBackground.visible = true
			
			print("Connection error encountered.")
			statusDisplay.text = "Connection error. Closed."
			emit_signal("connection_closed")

func _exit_tree() -> void:
	# Gracefully terminate the socket when the node leaves the tree
	tcp_client.disconnect_from_host()
	if reconstruction_thread and reconstruction_thread.is_started():
		reconstruction_thread.wait_to_finish()
