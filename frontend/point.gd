extends StaticBody3D

@export var camera: Camera3D
@export var defaultScale: Vector3
@export var focusedScale: Vector3
@export var lerpWeight: float
#var focused: bool

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	if camera.focused == position: scale = scale.lerp(focusedScale, lerpWeight * delta)
	else: scale = scale.lerp(defaultScale, lerpWeight * delta)
	
	#pass

# Function to detect mouse input.
func _on_click_input_event(camera: Node, event: InputEvent, event_position: Vector3, normal: Vector3, shape_idx: int) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
		camera._focus(self)
