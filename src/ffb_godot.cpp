#include "SDL3/SDL_error.h"
#include <climits>
#include <cstdint>
#include <optional>
#include <unordered_map>
extern "C" {
#include <SDL3/SDL.h>
}

#include "SDL3/SDL_haptic.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_joystick.h"
#include "SDL3/SDL_stdinc.h"

#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/variant/utility_functions.hpp"
#include "uid_int.hpp"
#include <godot_cpp/classes/time.hpp>

using Effect_Id = int;
using namespace godot;
static constexpr uint64_t MIN_UPDATE_INTERVAL_US = 5000; // 5ms = 200Hz 上限
// 调整 tuple 增加时间戳和 haptic 句柄（如果需要）
using HapPrepare = std::tuple<SDL_HapticEffect, Effect_Id, uint64_t>;
// 或者单独存储每个 effect 的 last_update

static constexpr std::int64_t CANNOT_UNIQUEID_IT = -1;

struct Effect_Map : std::unordered_map<std::int64_t, HapPrepare> {
  // 每个 effect 独立限流，使用 map 存储时间戳
  std::unordered_map<std::int64_t, uint64_t> last_update_map;
  Int64UUIDGenerator ug{1};
  std::optional<std::int64_t> create_const(SDL_Haptic *haptic,
                                           SDL_Joystick *joystick, int16_t lv,
                                           int32_t dir, uint16_t attk_len,
                                           uint16_t fade_len) {
    if (!haptic)
      return std::nullopt; // 使用传入的 haptic，不再自己打开
    std::int64_t key = CANNOT_UNIQUEID_IT;
    do {
      key = ug.nextId();
    } while (key == CANNOT_UNIQUEID_IT);

    auto [it, inserted] =
        this->try_emplace(key, std::tuple{SDL_HapticEffect{}, -1, 0ULL});
    auto &[e, id, _] = it->second;
    SDL_zero(e);
    e.type = SDL_HAPTIC_CONSTANT;
    e.constant.level = lv;
    e.constant.direction.type = SDL_HAPTIC_CARTESIAN;
    e.constant.direction.dir[0] = dir;
    e.constant.length = SDL_HAPTIC_INFINITY;
    e.constant.attack_length = attk_len;
    e.constant.fade_length = fade_len;

    id = SDL_CreateHapticEffect(haptic, &e);
    if (id < 0) {
      this->erase(it);
      return std::nullopt;
    }
    // SDL_RunHapticEffect(haptic, id, SDL_HAPTIC_INFINITY);
    return key;
  }

  bool update_const(SDL_Haptic *haptic, std::int64_t key, int16_t lv,
                    int32_t dir) {
    auto it = this->find(key);
    if (it == this->end())
      return false;
    auto &[e, id, _] = it->second;
    if (id < 0 || !haptic)
      return false;

    uint64_t now = Time::get_singleton()->get_ticks_usec();
    uint64_t &last = last_update_map[key];
    if (now - last < MIN_UPDATE_INTERVAL_US)
      return true; // 跳过
    last = now;

    e.constant.level = lv;
    e.constant.direction.dir[0] = dir;
    if (!SDL_UpdateHapticEffect(haptic, id, &e)) {
      UtilityFunctions::printerr("Update Failed: ", SDL_GetError());
      return false;
    }
    return true;
  }

  // 必须添加销毁函数
  bool destroy_effect(SDL_Haptic *haptic, std::int64_t key) {
    auto it = find(key);
    if (it == end())
      return false;
    auto &[e, id, _] = it->second;
    if (id >= 0 && haptic)
      SDL_DestroyHapticEffect(haptic, id);
    erase(it);
    last_update_map.erase(key);
    return true;
  }
  bool stop_effect(SDL_Haptic *haptic, std::int64_t key) {
    auto it = find(key);
    if (it == end())
      return false;
    auto &[e, id, _] = it->second;
    if (id >= 0 && haptic) {
      SDL_StopHapticEffect(haptic, id);
      return true;
    }
    return false;
  }
  bool start_effect(SDL_Haptic *haptic, std::int64_t key) {
    auto it = find(key);
    if (it == end())
      return false;
    auto &[e, id, _] = it->second;
    if (id >= 0 && haptic &&
        SDL_RunHapticEffect(haptic, id, SDL_HAPTIC_INFINITY))
      return true;
    UtilityFunctions::print("Run Haptic Effect Failed");
    return false;
  }

  // 创建周期性效果 (支持正弦、方波等)
  std::optional<std::int64_t> create_periodic(
      SDL_Haptic *haptic,
      uint16_t type, // SDL_HAPTIC_SINE, SDL_HAPTIC_SQUARE, SDL_HAPTIC_TRIANGLE,
                     // SDL_HAPTIC_SAWTOOTHUP, SDL_HAPTIC_SAWTOOTHDOWN
      int16_t magnitude, // 幅度 (0-32767)
      int16_t offset,    // 偏移
      uint16_t period,   // 周期 (ms)
      uint16_t attack_length, uint16_t fade_length,
      int32_t direction) // 方向角度 (笛卡尔坐标X)
  {
    if (!(type == SDL_HAPTIC_SINE || type == SDL_HAPTIC_SQUARE ||
          type == SDL_HAPTIC_TRIANGLE || type == SDL_HAPTIC_SAWTOOTHUP ||
          type == SDL_HAPTIC_SAWTOOTHDOWN))
      return std::nullopt;
    if (!haptic)
      return std::nullopt;
    std::int64_t key = CANNOT_UNIQUEID_IT;
    do {
      key = ug.nextId();
    } while (key == CANNOT_UNIQUEID_IT);

    auto [it, inserted] =
        try_emplace(key, std::tuple{SDL_HapticEffect{}, -1, 0ULL});
    auto &[e, id, _] = it->second;
    SDL_zero(e);

    e.type = type;
    e.periodic.magnitude = magnitude;
    e.periodic.offset = offset;
    e.periodic.period = period;
    e.periodic.length = SDL_HAPTIC_INFINITY;
    e.periodic.attack_length = attack_length;
    e.periodic.fade_length = fade_length;
    e.periodic.direction.type = SDL_HAPTIC_CARTESIAN;
    e.periodic.direction.dir[0] = direction;

    id = SDL_CreateHapticEffect(haptic, &e);
    if (id < 0) {
      erase(it);
      return std::nullopt;
    }
    return key;
  }

  // 更新周期性效果
  bool update_periodic(SDL_Haptic *haptic, std::int64_t key, int16_t magnitude,
                       int16_t offset, int32_t direction) {
    auto it = find(key);
    if (it == end())
      return false;
    auto &[e, id, _] = it->second;
    if (id < 0 || !haptic)
      return false;

    // 检查是否是周期效果 (type 必须在 periodic 范围内)
    uint16_t type = e.type;
    if (!(type == SDL_HAPTIC_SINE || type == SDL_HAPTIC_SQUARE ||
          type == SDL_HAPTIC_TRIANGLE || type == SDL_HAPTIC_SAWTOOTHUP ||
          type == SDL_HAPTIC_SAWTOOTHDOWN))
      return false;

    uint64_t now = Time::get_singleton()->get_ticks_usec();
    uint64_t &last = last_update_map[key];
    if (now - last < MIN_UPDATE_INTERVAL_US)
      return true;
    last = now;

    e.periodic.magnitude = magnitude;
    e.periodic.offset = offset;
    e.periodic.direction.dir[0] = direction;

    if (!SDL_UpdateHapticEffect(haptic, id, &e)) {
      UtilityFunctions::printerr("Update periodic failed: ", SDL_GetError());
      return false;
    }
    return true;
  }

  // 创建条件效果 (damper, inertia, friction, spring)
  std::optional<std::int64_t>
  create_condition(SDL_Haptic *haptic,
                   uint16_t type, // SDL_HAPTIC_SPRING, SDL_HAPTIC_DAMPER,
                                  // SDL_HAPTIC_INERTIA, SDL_HAPTIC_FRICTION
                   int16_t right_sat, int16_t left_sat, int16_t right_coeff,
                   int16_t left_coeff, int16_t deadband, int16_t center,
                   uint16_t attack_length, uint16_t fade_length) {
    if (!(type == SDL_HAPTIC_SPRING || type == SDL_HAPTIC_DAMPER ||
          type == SDL_HAPTIC_INERTIA || type == SDL_HAPTIC_FRICTION))
      return std::nullopt;
    if (!haptic)
      return std::nullopt;
    std::int64_t key = CANNOT_UNIQUEID_IT;
    do {
      key = ug.nextId();
    } while (key == CANNOT_UNIQUEID_IT);

    auto [it, inserted] =
        try_emplace(key, std::tuple{SDL_HapticEffect{}, -1, 0ULL});
    auto &[e, id, _] = it->second;
    SDL_zero(e);

    e.type = type;
    // condition 效果有 3 个轴，我们只使用第一个轴 (索引0)
    e.condition.right_sat[0] = right_sat;
    e.condition.left_sat[0] = left_sat;
    e.condition.right_coeff[0] = right_coeff;
    e.condition.left_coeff[0] = left_coeff;
    e.condition.deadband[0] = deadband;
    e.condition.center[0] = center;
    e.condition.length = SDL_HAPTIC_INFINITY;

    id = SDL_CreateHapticEffect(haptic, &e);
    if (id < 0) {
      erase(it);
      return std::nullopt;
    }
    return key;
  }

  // 更新条件效果（只更新常用参数，其他参数保持不变）
  bool update_condition(SDL_Haptic *haptic, std::int64_t key, int16_t right_sat,
                        int16_t left_sat, int16_t right_coeff,
                        int16_t left_coeff, int16_t deadband, int16_t center) {
    auto it = find(key);
    if (it == end())
      return false;
    auto &[e, id, _] = it->second;
    if (id < 0 || !haptic)
      return false;

    // 检查是否是条件效果
    uint16_t type = e.type;
    if (!(type == SDL_HAPTIC_SPRING || type == SDL_HAPTIC_DAMPER ||
          type == SDL_HAPTIC_INERTIA || type == SDL_HAPTIC_FRICTION))
      return false;

    uint64_t now = Time::get_singleton()->get_ticks_usec();
    uint64_t &last = last_update_map[key];
    if (now - last < MIN_UPDATE_INTERVAL_US)
      return true;
    last = now;

    e.condition.right_sat[0] = right_sat;
    e.condition.left_sat[0] = left_sat;
    e.condition.right_coeff[0] = right_coeff;
    e.condition.left_coeff[0] = left_coeff;
    e.condition.deadband[0] = deadband;
    e.condition.center[0] = center;

    if (!SDL_UpdateHapticEffect(haptic, id, &e)) {
      UtilityFunctions::printerr("Update condition failed: ", SDL_GetError());
      return false;
    }
    return true;
  }
};

class FFBManager : public Node {
  GDCLASS(FFBManager, Node)

  SDL_Joystick *joystick = nullptr;
  SDL_Haptic *haptic = nullptr;
  SDL_JoystickID *joy_ids = nullptr;
  int num_joy = 0;
  uint32_t joystick_id = 0; // ← 不能是 -1，uint32_t 的 -1 是 4294967295

private:
  Effect_Map map;

protected:
  static void _bind_methods() {
    ClassDB::bind_method(D_METHOD("init_sdl"), &FFBManager::init_sdl);
    ClassDB::bind_method(D_METHOD("deinit_sdl"), &FFBManager::deinit_sdl);
    // ClassDB::bind_method(D_METHOD("set_joystick_id", "id"),
    //                      &FFBManager::set_joystick_id);
    // ClassDB::bind_method(D_METHOD("get_joystick_id"),
    //                      &FFBManager::get_joystick_id);
    ClassDB::bind_method(
        D_METHOD("new_effect_constforce", "lv", "dir", "attk_len", "fade_len"),
        &FFBManager::new_effect_constforce);
    ClassDB::bind_method(
        D_METHOD("update_effect_constforce", "key", "lv", "dir"),
        &FFBManager::update_effect_constforce);
    ClassDB::bind_method(D_METHOD("destroy_effect_constforce", "key"),
                         &FFBManager::destroy_effect_constforce);

    ClassDB::bind_method(D_METHOD("start_effect", "key"),
                         &FFBManager::start_effect);
    ClassDB::bind_method(D_METHOD("stop_effect", "key"),
                         &FFBManager::stop_effect);
    ClassDB::add_property("FFBManager",
                          PropertyInfo(Variant::INT, "joystick_id"),
                          "set_joystick_id", "get_joystick_id");

    // Periodic
    ClassDB::bind_method(D_METHOD("new_effect_periodic", "type", "magnitude",
                                  "offset", "period", "attack_length",
                                  "fade_length", "direction"),
                         &FFBManager::new_effect_periodic);
    ClassDB::bind_method(D_METHOD("update_effect_periodic", "key", "magnitude",
                                  "offset", "direction"),
                         &FFBManager::update_effect_periodic);

    // Condition
    ClassDB::bind_method(D_METHOD("new_effect_condition", "type", "right_sat",
                                  "left_sat", "right_coeff", "left_coeff",
                                  "deadband", "center", "attack_length",
                                  "fade_length"),
                         &FFBManager::new_effect_condition);
    ClassDB::bind_method(D_METHOD("update_effect_condition", "key", "right_sat",
                                  "left_sat", "right_coeff", "left_coeff",
                                  "deadband", "center"),
                         &FFBManager::update_effect_condition);
  }

public:
  void set_joystick_id(uint32_t p_id) { joystick_id = p_id; }
  uint32_t get_joystick_id() const { return joystick_id; }

  std::int64_t new_effect_constforce(int16_t lv, int32_t dir, uint16_t attk_len,
                                     uint16_t fade_len) {
    if (joystick == nullptr) {
      UtilityFunctions::printerr("Joystick Not Initialized");
      return CANNOT_UNIQUEID_IT;
    }
    if (haptic == nullptr) {
      UtilityFunctions::printerr("Haptic Not Initialized");
      return CANNOT_UNIQUEID_IT;
    }
    // haptic = SDL_OpenHapticFromJoystick(joystick);
    auto res = map.create_const(haptic, joystick, lv, dir, attk_len, fade_len);
    if (res.has_value()) {
      return res.value();
    } else {
      return CANNOT_UNIQUEID_IT;
    }
  }

  bool update_effect_constforce(std::int64_t key, int16_t lv, int32_t dir) {
    auto it = map.find(key);
    if (it == map.end()) {
      return false;
    }
    // auto [x,z,w] = it->second;
    auto res = map.update_const(haptic, key, lv, dir);
    return res;
  }
  bool destroy_effect_constforce(std::int64_t key) {
    auto it = map.find(key);
    if (it == map.end()) {
      return false;
    }
    auto res = map.destroy_effect(haptic, key);
    return res;
  }

  // 周期性效果
  std::int64_t new_effect_periodic(uint16_t type, int16_t magnitude,
                                   int16_t offset, uint16_t period,
                                   uint16_t attack_length, uint16_t fade_length,
                                   int32_t direction) {
    if (!joystick || !haptic) {
      UtilityFunctions::printerr("Joystick or Haptic not initialized");
      return CANNOT_UNIQUEID_IT;
    }
    auto res = map.create_periodic(haptic, type, magnitude, offset, period,
                                   attack_length, fade_length, direction);
    return res.value_or(CANNOT_UNIQUEID_IT);
  }

  bool update_effect_periodic(std::int64_t key, int16_t magnitude,
                              int16_t offset, int32_t direction) {
    return map.update_periodic(haptic, key, magnitude, offset, direction);
  }

  // 条件效果
  std::int64_t new_effect_condition(uint16_t type, int16_t right_sat,
                                    int16_t left_sat, int16_t right_coeff,
                                    int16_t left_coeff, int16_t deadband,
                                    int16_t center, uint16_t attack_length,
                                    uint16_t fade_length) {
    if (!joystick || !haptic) {
      UtilityFunctions::printerr("Joystick or Haptic not initialized");
      return CANNOT_UNIQUEID_IT;
    }
    auto res = map.create_condition(haptic, type, right_sat, left_sat,
                                    right_coeff, left_coeff, deadband, center,
                                    attack_length, fade_length);
    return res.value_or(CANNOT_UNIQUEID_IT);
  }

  bool update_effect_condition(std::int64_t key, int16_t right_sat,
                               int16_t left_sat, int16_t right_coeff,
                               int16_t left_coeff, int16_t deadband,
                               int16_t center) {
    return map.update_condition(haptic, key, right_sat, left_sat, right_coeff,
                                left_coeff, deadband, center);
  }

  bool init_sdl() {
    bool res = SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC);
    if (!res) {
      const char *err = SDL_GetError();
      UtilityFunctions::printerr("SDL Init Failed: ", err ? err : "(null)");
      return false;
    }

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    joy_ids = SDL_GetJoysticks(&num_joy);
    if (joy_ids == nullptr || num_joy == 0) {
      UtilityFunctions::printerr("Cannot Find Joys");
      SDL_Quit();
      return false;
    }

    UtilityFunctions::print("Find ", num_joy, " Joy(s)");

    if (joystick_id >= (uint32_t)num_joy) {
      UtilityFunctions::printerr("'joystick_id' ", joystick_id,
                                 " out of range, default to 0");
      joystick_id = 0;
    }

    SDL_JoystickID id_ofjoy = joy_ids[joystick_id];
    joystick = SDL_OpenJoystick(id_ofjoy);
    if (joystick == nullptr) {
      UtilityFunctions::printerr("Cannot Bind to Joy: ", SDL_GetError());
      SDL_free(joy_ids);
      joy_ids = nullptr;
      SDL_Quit();
      return false;
    }
    haptic = SDL_OpenHapticFromJoystick(joystick);
    if (!haptic) {
      UtilityFunctions::printerr("Failed to open haptic from joystick: ",
                                 SDL_GetError());
      return false;
    }
    UtilityFunctions::print("Init Successfully");
    return true;
  }

  void print_joy_info() {
    if (joystick == nullptr) {
      UtilityFunctions::printerr("Joystick Not Open");
      return;
    }
    const char *name = SDL_GetJoystickName(joystick);
    int res = 0;
    if (haptic != nullptr) {
      res = SDL_GetMaxHapticEffectsPlaying(haptic);
    }
    UtilityFunctions::print("Simutanious Effect: ", res);
    UtilityFunctions::print("Opened Joy: ", name ? name : "Unknown",
                            " Instance: ", joystick_id);
  }

  bool start_effect(std::int64_t key) {
    if (haptic == nullptr) {
      return false;
    }
    return map.start_effect(haptic, key);
  }
  bool stop_effect(std::int64_t key) {
    if (haptic == nullptr) {
      return false;
    }
    return map.stop_effect(haptic, key);
  }
  // 启动恒力（只调用一次！）

  bool deinit_sdl() {
    for (auto &[key, tuple] : map) {
      auto &[e, id, _] = tuple;
      if (id >= 0 && haptic) {
        SDL_DestroyHapticEffect(haptic, id);
      }
    }
    map.clear();
    map.last_update_map.clear();
    if (haptic) {
      SDL_CloseHaptic(haptic);
      haptic = nullptr;
    }
    if (joy_ids != nullptr) {
      SDL_free(joy_ids);
      joy_ids = nullptr;
    }
    if (joystick != nullptr) {
      SDL_CloseJoystick(joystick);
      joystick = nullptr;
    }
    SDL_Quit();
    return true;
  }

  ~FFBManager() override { deinit_sdl(); }
};
