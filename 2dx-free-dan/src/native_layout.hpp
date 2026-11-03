#pragma once
#include <cstdint>
#include "addresses.hpp"
#include <cstddef>
namespace self_dan::native {
// Verified only for LDJ 2026090900-010. These are not stable public game APIs.
constexpr uint32_t timestamp = 0x6a8bafd1;
constexpr uint32_t entry_rva = 0xb1ba0c;
constexpr uint32_t image_size = 0xbb93000;
constexpr uintptr_t settings = 0xacd59a0;
constexpr uintptr_t scene_id = 0x107ecf0;
constexpr uintptr_t dan_records = 0xaaafe30;
constexpr uintptr_t dan_state = 0xaaafe20;
constexpr uintptr_t gauges = 0xb285350;
constexpr uintptr_t category_manager = 0xaab1b08;
constexpr uintptr_t music_id_offset = 0x67c;
constexpr uintptr_t music_levels_offset = 0x3ec;
enum Scene : int { music_select=23, stage_result=24, dan_select=25, dan_result=26,
    life_result=27, life_music_select=28 };
inline int& dan_stage=addresses::dan_stage_id;
inline int& life_stage=addresses::life_stage_id;
struct Settings {
    int32_t mode, style, difficulty[2], joined[2];
    uint8_t reserved[24];
    uintptr_t active_music;
};
static_assert(offsetof(Settings,active_music)==0x30);
struct DanRow {
    int32_t judgment[5];
    int32_t displayed_health;
    int32_t notes;
    uint8_t cleared;
    uint8_t padding[3];
};
static_assert(sizeof(DanRow)==0x20);
struct Gauge { int32_t raw, judgment_values[4]; };
static_assert(sizeof(Gauge)==0x14);
constexpr int chart_slot(int style,int difficulty) {
    return style>=0 && style<=1 && difficulty>=0 && difficulty<=4 ? style*5+difficulty : -1;
}
// 96fea0 dispatches (scene, resumed, parameter) to vtable slot 15.
// 967550 dispatches next(scene ID) via slot 22 and prepare(next ID) via slot 20.
using Init = uint64_t (*)(void*,void*);
using Update = void (*)(void*,bool,void*);
using Prepare = void (*)(void*,int);
using Next = int (*)(void*,int);
using Populate = uintptr_t (*)(void*);
} // namespace self_dan::native
