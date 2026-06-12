#include "SDL3/SDL_error.h"
extern "C" {
    #include <SDL3/SDL.h>
    #include <cmath>
    #include <cstddef>
    #include <cstdint>
    #include <cstdio>
}


// #include <format>
//  /*
#include "SDL3/SDL_haptic.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_joystick.h"
#include "SDL3/SDL_oldnames.h"
#include "SDL3/SDL_stdinc.h"
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/variant/variant.hpp"
#include <godot_cpp/variant/utility_functions.hpp>

#pragma once
using namespace godot;

class FFBManager : public Node {
  GDCLASS(FFBManager, Node)
  SDL_Joystick *joystick = nullptr;
  SDL_Haptic *haptic = nullptr;
  SDL_JoystickID* joy_ids=nullptr;
  uint32_t joystick_id = -1;
  bool initialized = false;
  SDL_HapticEffect effect;
  bool is_playing = false;
  int eff_id= -1;
protected:
  static void _bind_methods() {
      ClassDB::bind_method(D_METHOD("init_sdl"), &FFBManager::init_sdl);
      ClassDB::bind_method(D_METHOD("deinit_sdl"), &FFBManager::deinit_sdl);
      ClassDB::bind_method(D_METHOD("constant_force","lv","dir","attk_len","fade_len"), &FFBManager::constant_force);
      ClassDB::bind_method(D_METHOD("stop_effect"),&FFBManager::stop_effect);
      ClassDB::bind_method(D_METHOD("global_gain","gain"),&FFBManager::global_gain);
      ClassDB::bind_method(D_METHOD("rumble","strength","len"),&FFBManager::rumble);
      ClassDB::bind_method(D_METHOD("condition","maxf_r","maxf_l","coeff_r","coeff_l","deadband","which"),&FFBManager::condition);
      ClassDB::bind_method(D_METHOD("print_info"),&FFBManager::print_joy_info);
  }

public:
  FFBManager() = default;
  ~FFBManager() override = default;
  bool init_sdl() {
      auto res = SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC);
      if (res==0) {
          String s = SDL_GetError();
          UtilityFunctions::printerr("SDL Init Failed ",s);
          return false;
      }
      SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
      int num_joy = 0;
      joy_ids = SDL_GetJoysticks(&num_joy);
      if (joy_ids==NULL||num_joy==0) {
          UtilityFunctions::printerr("Cannot Find Joys");
          SDL_Quit();
          return false;
      }
      UtilityFunctions::print("Find ",num_joy," Joy");
      if (joystick_id==-1) {
          UtilityFunctions::printerr("Not a valid joyID , Default to Zero");
          joystick_id = 0;
      }
      SDL_JoystickID id_ofjoy = joy_ids[joystick_id];
      joystick = SDL_OpenJoystick(id_ofjoy);
      if (joystick==NULL) {
          UtilityFunctions::printerr("Cannot Bind to Joy");
          SDL_free(joy_ids);
          SDL_Quit();
          return false;
      }
      UtilityFunctions::print("Init Successfully");
      return true;
  }
  bool deinit_sdl(){
      SDL_free(joy_ids);
      SDL_CloseJoystick(joystick);
      SDL_Quit();
      return true;
  }
  void print_joy_info(){
      if (joystick==NULL) {
          UtilityFunctions::printerr("Cannot Bind to Joy");
          return;
      }
      const char* name = SDL_GetJoystickName(joystick);
      UtilityFunctions::print("Opened Joy: ", name?name:"Unknown" ," Instance: ",joystick_id);
  }
  bool constant_force(int16_t lv,int32_t dir,uint16_t attk_len,uint16_t fade_len){
      SDL_zero(effect);
      if (!is_playing) {
          haptic = SDL_OpenHapticFromJoystick(joystick);
          UtilityFunctions::print("Not Playing Yet,Open Haptic: ",haptic);
      }
      if (haptic==NULL) {
          UtilityFunctions::printerr("Haptic OpenFailed , Dont have it");
          return false;
      }
      effect.type = SDL_HAPTIC_CONSTANT;
      effect.constant.level = lv;
      effect.constant.direction.type = SDL_HAPTIC_CARTESIAN;
      effect.constant.direction.dir[0] = dir;
      effect.constant.length = SDL_HAPTIC_INFINITY;
      effect.constant.attack_length=attk_len;
      effect.constant.fade_length = fade_len;
      if (!is_playing) {
          eff_id = SDL_CreateHapticEffect(haptic, &effect);
          UtilityFunctions::print("Not Playing Yet,Open Effect ID: ",eff_id);
          SDL_RunHapticEffect(haptic, eff_id, SDL_HAPTIC_INFINITY);
      }
      if (is_playing) {
        SDL_UpdateHapticEffect(haptic, eff_id, &effect);
      }
      is_playing=true;
      return true;
  }
  bool rumble(float strength,uint len){
      if (haptic==nullptr) {
          UtilityFunctions::printerr("Haptic OpenFailed , Dont have it , In playRumble");
          return false;
      }
      auto res = SDL_PlayHapticRumble(haptic, strength, len);
      return res;
  }
  bool condition(int16_t maxf_r,int16_t maxf_l,int16_t coeff_r,int16_t coeff_l,int16_t deadband,uint which){
      SDL_zero(effect);
      if (!is_playing) {
          SDL_OpenHapticFromJoystick(joystick);
      }
      if (haptic==NULL) {
          UtilityFunctions::printerr("Haptic OpenFailed , Dont have it");
          return false;
      }
      if (1) {
          effect.type = SDL_HAPTIC_DAMPER;
      }
      if (2) {
          effect.type = SDL_HAPTIC_INERTIA;
      }
      if (3) {
          effect.type = SDL_HAPTIC_FRICTION;
      }
      effect.condition.right_sat[0]=maxf_r;
      effect.condition.left_sat[0]=maxf_l;
      effect.condition.right_coeff[0]=coeff_r;
      effect.condition.left_coeff[0]=coeff_l;
      effect.condition.deadband[0] = deadband;
      effect.condition.center[0] = 0;
      effect.condition.length = SDL_HAPTIC_INFINITY;
      if (!is_playing) {
          eff_id = SDL_CreateHapticEffect(haptic, &effect);
          SDL_RunHapticEffect(haptic, eff_id, 1);
      }
      if (is_playing) {
        SDL_UpdateHapticEffect(haptic, eff_id, &effect);
      }
      is_playing=true;
      return true;
  }
  bool global_gain(uint gain){
      if (SDL_GetHapticFeatures(haptic) & SDL_HAPTIC_GAIN) {
          // 将全局增益设置为 50%
          if (!SDL_SetHapticGain(haptic, gain)) {
              UtilityFunctions::printerr("Set Gain Failed: %s", SDL_GetError());
              return false;
          }
          UtilityFunctions::printerr("Cannot Set Gain , No Feature: %s", SDL_GetError());
          return false;
      }
      return false;
  }
  bool stop_effect(){
      if (eff_id==-1) {
          UtilityFunctions::printerr("No Effid");
          return false;
      }
      if (haptic==nullptr) {
          UtilityFunctions::printerr("Haptic OpenFailed , Dont have it , In stopEffect");
          return false;
      }
      is_playing=false;
      SDL_DestroyHapticEffect(haptic, eff_id);
      SDL_CloseHaptic(haptic);
      return true;
  }
  void print_type(const Variant &p_variant) const;
};
