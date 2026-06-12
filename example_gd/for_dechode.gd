extends FFBManager
@export var node:BaseCar;

var global_timer := 0;
@export var mod_engine_factor_hz = 40;
@export var ethta_car_body:float = 0.01;
@export var SmoothingFactor := 120.0;

@export var max_autoforce = 1800;

@export var lowspeed_ramp_speed := 3.0 

var prev_fz_fr := 0.0;
var prev_fz_fl := 0.0;
var prev_align_torque:=0.0;
var LowSpeed_Force := 0.0;
var max_lowspeed := 10.0;
var low_speed_dir :=0.0;
var prev_steering := 0.0;

var key_effect_const := 0;
func _ready():
	init_sdl()
	key_effect_const = new_effect_constforce(0,1,0,0);
	start_effect(key_effect_const);
	#print_info()
	#stop_effect()
	#constant_force(3000, 1, 0, 0)  # 启动一次，永久播放

func _physics_process(delta):
	var safe_speed :float= abs(node.speedo)
	
	# === AutoCenterForce（物理回正力）===
	var align_torque_fl :float= node.wheel_fl.slip_vec.x * node.wheel_fl.y_force * ethta_car_body
	var align_torque_fr :float= node.wheel_fr.slip_vec.x * node.wheel_fr.y_force * ethta_car_body
	var rear_fy :float= node.wheel_bl.force_vec.x + node.wheel_br.force_vec.x
	var TqAlign := align_torque_fl + align_torque_fr
	
	var SoftenFactor := 450.0
	var AutoCenterForce: float = TqAlign * SoftenFactor / (abs(TqAlign) + SoftenFactor)
	AutoCenterForce += rear_fy * 0.5
	AutoCenterForce = (AutoCenterForce + prev_align_torque * (SmoothingFactor / 100.0)) / (SmoothingFactor / 100.0 + 1.0)
	prev_align_torque = AutoCenterForce
	
	# === LowSpeed Parking Force（只在低速生效）===
	var low_force := 0.0
	
	if safe_speed < max_lowspeed:
		var steer_center := node.steering_amount * node.car_params.max_steer
		var steer_delta := steer_center - prev_steering
		
		if abs(steer_delta) > 0.002:  # 死区
			var target :float= sign(steer_delta) * 7.0
			low_speed_dir = move_toward(low_speed_dir, target, lowspeed_ramp_speed)
		else:
			low_speed_dir = move_toward(low_speed_dir, 0.0, lowspeed_ramp_speed * 0.5)
		
		low_speed_dir = clamp(low_speed_dir, -7.0, 7.0)
		
		# 速度越低，parking force 越强；speed→10 时自然衰减到接近 0
		var speed_attenuation := 1.0 / (pow(safe_speed, 0.5) + 0.5)  # 比 +1 更柔和
		low_force = (10000.0 / 8.0) * low_speed_dir * speed_attenuation
		
		prev_steering = steer_center
	else:
		low_speed_dir = 0.0
		prev_steering = node.steering_amount * node.car_params.max_steer
	
	# === 分别处理两种力的速度响应 ===
	# AutoCenterForce：速度越高越明显（模拟气动/轮胎回正）
	var autocenter_speed_factor := remap(clamp(safe_speed, 0.0, 30.0), 0.0, 30.0, 0.1, 1.0)
	
	# LowSpeed Force：速度越低越强，高速直接为 0
	var lowspeed_speed_factor := 1.0 if safe_speed < max_lowspeed else 0.0
	# 或者更平滑的过渡：
	# var lowspeed_speed_factor := clamp(1.0 - safe_speed / max_lowspeed, 0.0, 1.0)
	
	var final_autocenter := AutoCenterForce * autocenter_speed_factor
	var final_lowspeed := low_force * lowspeed_speed_factor
	
	var total_force := final_autocenter + final_lowspeed
	
	# === 映射到设备：直接用 tanh，不要额外除法 ===
	var normalized := tanh(total_force / max_autoforce)  # ∈ [-0.76, 0.76] @ ±1800
	var device_force := normalized * 32765.0             # 直接映射，不用 remap
	update_effect_constforce(key_effect_const,int(device_force),1);
	
func _exit_tree():
	stop_effect(key_effect_const);
	deinit_sdl();
