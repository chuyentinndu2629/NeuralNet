extends Camera3D

@export var enabled: bool
@export var lerpWeight: float
@export var defaultPos: Vector3
@export var defaultRotation: Vector3  # Euler angles
@export var focusedOffset: Vector3
@export var focusedRotation: Vector3

var focused: Vector3
var focusedId: String

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	position = defaultPos
	make_current();

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	if Input.is_action_just_pressed("escape"):
		#if focused: focused.focused = false
		focused = Vector3(0, 0, 0)
		focusedId = ""
		
	if enabled:
		#print("Cam: ", position);
		#print("Focus: ", focused)
		# Lerp to offset
		if focused:
			position = position.lerp(focused + focusedOffset, lerpWeight * delta)
			rotation = rotation.lerp(focusedRotation, lerpWeight * delta)
		else:
			position = position.lerp(defaultPos, lerpWeight * delta)
			rotation = rotation.lerp(defaultRotation, lerpWeight * delta)

func _focus(object: Node3D):
	#if focused: focused.focused = false
	focused = object.position
	focusedId = object.name
	#focused.focused = true
	
	get_node("/root/MapView").send_message(object.name)
	#print(get_node("/root/MapView").name)
