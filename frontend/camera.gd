extends Camera3D

@export var enabled: bool
@export var lerpWeight: float
@export var defaultPos: Vector3
@export var defaultRotation: Vector3  # Euler angles
@export var focusedOffset: Vector3
@export var focusedRotation: Vector3
@export var zoomStep: float
@export var drag: float = 5.0  # how fast drag momentum decays after release (higher = stops sooner)
@export var zoomCurve: Curve

@export var mapView: Node3D

var currentlyFocused: bool
var focused: Vector3
var focusedId: String
var currentZoom: float # in %

var is_holding: bool = false
var last_ground_point = null      # Vector3 or null - world point currently "grabbed" by the cursor
var drag_velocity: Vector3 = Vector3.ZERO

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	position = defaultPos
	make_current()
	
	currentlyFocused = false

	currentZoom = 1.0

func mapZoomValues(currentZoom: float):
	if zoomCurve:
		# Curve.sample() takes an X position (e.g. 0.0 to 1.0) and returns the Y value
		return zoomCurve.sample(currentZoom)
	return 0.0

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	if Input.is_action_just_pressed("escape"):
		#if focused: focused.focused = false
		focused = Vector3(0, 0, 0)
		focusedId = ""
		currentlyFocused = false

	if enabled:
		# Lerp to offset
		if currentlyFocused:
			position = position.lerp(focused + focusedOffset, lerpWeight * delta)
			#rotation = rotation.lerp(focusedRotation, lerpWeight * delta)
			rotation.x = lerp_angle(rotation.x, focusedRotation.x / 180 * PI, lerpWeight * delta)
			rotation.y = lerp_angle(rotation.y, focusedRotation.y / 180 * PI, lerpWeight * delta)
			rotation.z = lerp_angle(rotation.z, focusedRotation.z / 180 * PI, lerpWeight * delta)
		else:
			if is_holding:
				_drag_camera(delta)
			elif drag_velocity.length_squared() > 0.0001:
				# Coast to a stop after the mouse is released
				position += drag_velocity * delta
				defaultPos.x = position.x
				defaultPos.z = position.z
				drag_velocity = drag_velocity.lerp(Vector3.ZERO, clamp(drag * delta, 0.0, 1.0))
			else:
				drag_velocity = Vector3.ZERO

			position = position.lerp(
				Vector3(defaultPos.x, defaultPos.y * mapZoomValues(currentZoom), defaultPos.z),
			lerpWeight * delta)
			rotation.x = lerp_angle(rotation.x, defaultRotation.x / 180 * PI, lerpWeight * delta)
			rotation.y = lerp_angle(rotation.y, defaultRotation.y / 180 * PI, lerpWeight * delta)
			rotation.z = lerp_angle(rotation.z, defaultRotation.z / 180 * PI, lerpWeight * delta)

func _focus(key: String):
	print(key)
	#if focused: focused.focused = false
	currentlyFocused = true
	focused = mapView.pointPositions[mapView.keyToIndex[key]];
	focusedId = key
	#focused.focused = true
	
	#mapView.multimeshNode.scale = Vector3.ONE * 0.5

	get_node("/root/MapView").send_message(key)
	#print(get_node("/root/MapView").name)

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT:
			is_holding = event.pressed
			if event.pressed:
				# Grab the ground point under the cursor and kill any leftover momentum
				drag_velocity = Vector3.ZERO
				last_ground_point = get_click_position_on_ground(event.position)
				
				var point = get_clicked_vessel_key(event.position)
				if point:
					#print("FOCUSING")
					_focus(point)
			else:
				last_ground_point = null

		if event.pressed:
			# Pressed
			if event.button_index == MOUSE_BUTTON_WHEEL_UP:
				#print("Zoom+")
				if currentZoom >= 0.1: currentZoom -= zoomStep;
			elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
				#print("Zoom-")
				if currentZoom <= 2.0: currentZoom += zoomStep;

func _drag_camera(delta: float) -> void:
	var mouse_pos: Vector2 = get_viewport().get_mouse_position()
	var ground_now = get_click_position_on_ground(mouse_pos)
	if ground_now == null:
		return

	if last_ground_point != null:
		var move_delta: Vector3 = last_ground_point - ground_now
		position += move_delta
		defaultPos.x = position.x
		defaultPos.z = position.z

		if delta > 0.0:
			drag_velocity = move_delta / delta

	# Recompute against the (now-moved) camera so the grabbed point stays glued to the cursor
	last_ground_point = get_click_position_on_ground(mouse_pos)

func get_click_position_on_ground(screen_pos: Vector2) -> Variant:
	var ray_origin: Vector3 = project_ray_origin(screen_pos)
	var ray_direction: Vector3 = project_ray_normal(screen_pos)

	var ground_plane := Plane(Vector3.UP, 0.0) # y = 0 plane, normal pointing up
	var intersection = ground_plane.intersects_ray(ray_origin, ray_direction)

	return intersection # Vector3 if it hit, null if the ray is parallel to the plane
	
func get_clicked_vessel_key(screen_pos: Vector2, max_pixel_dist: float = 15.0) -> String:
	var camera := get_viewport().get_camera_3d()
	if not camera or mapView.pointPositions.is_empty():
		return ""

	var max_dist_sq: float = max_pixel_dist * max_pixel_dist
	var closest_idx: int = -1
	var min_dist_sq: float = INF
	var total_points: int = mapView.multimesh.visible_instance_count

	for i in range(total_points):
		var world_pos: Vector3 = mapView.pointPositions[i]
		
		if camera.is_position_behind(world_pos):
			continue
			
		var screen_p: Vector2 = camera.unproject_position(world_pos)
		var dist_sq: float = screen_pos.distance_squared_to(screen_p)

		if dist_sq < max_dist_sq and dist_sq < min_dist_sq:
			min_dist_sq = dist_sq
			closest_idx = i

	if closest_idx != -1:
		return mapView.indexToKey[closest_idx]
		
	return ""
	
