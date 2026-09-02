extends Camera3D

@export var enabled: bool
@export var lerpWeight: float
@export var defaultPos: Vector3
@export var defaultRotation: Vector3  # Euler angles
@export var focusedOffset: Vector3
@export var focusedRotation: Vector3
@export var zoomStep: float
@export var drag: float = 5.0  # how fast drag momentum decays after release (higher = stops sooner)

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
				Vector3(defaultPos.x, defaultPos.y * currentZoom, defaultPos.z),
			lerpWeight * delta)
			rotation.x = lerp_angle(rotation.x, defaultRotation.x / 180 * PI, lerpWeight * delta)
			rotation.y = lerp_angle(rotation.y, defaultRotation.y / 180 * PI, lerpWeight * delta)
			rotation.z = lerp_angle(rotation.z, defaultRotation.z / 180 * PI, lerpWeight * delta)

func _focus(object: Node3D):
	#print(object)
	#if focused: focused.focused = false
	currentlyFocused = true
	focused = object.position
	focusedId = object.name
	#focused.focused = true

	get_node("/root/MapView").send_message(object.name)
	#print(get_node("/root/MapView").name)

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT:
			is_holding = event.pressed
			if event.pressed:
				# Grab the ground point under the cursor and kill any leftover momentum
				drag_velocity = Vector3.ZERO
				last_ground_point = get_click_position_on_ground(event.position)
				
				var point = check_click_on_points(event.position)
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
	
func check_click_on_points(screen_pos: Vector2) -> Node3D:
	# When I click on data points like vessels or aircrafts
	var ray_origin: Vector3 = project_ray_origin(screen_pos)
	var ray_direction: Vector3 = project_ray_normal(screen_pos)
	var to = ray_origin + ray_direction * 1000.0 # Ray length
	
	var space_state = get_world_3d().direct_space_state
	var query = PhysicsRayQueryParameters3D.create(ray_origin, to)
	var result = space_state.intersect_ray(query)
	
	if result:
		var clicked_object = result.collider
		return clicked_object
	return null
	
