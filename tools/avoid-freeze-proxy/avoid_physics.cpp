#include "avoid_physics.h"

namespace avoidfreeze::proxy
{

bool IsPositionHazard(const CollisionView &col, Vec2 pos) noexcept
{
    if(!col.IsValid())
        return false;

    // Check key contact points on character collision box (28x28 bounding box)
    static constexpr Vec2 offsets[] = {
        {-14.0f, -14.0f}, // top-left
        { 14.0f, -14.0f}, // top-right
        {-14.0f,  14.0f}, // bottom-left
        { 14.0f,  14.0f}, // bottom-right
        {  0.0f,   0.0f}, // center
        {  0.0f,  14.0f}, // bottom-center (feet)
    };

    for(const auto &offset : offsets)
    {
        const Vec2 pt = pos + offset;
        const int tx = static_cast<int>(std::floor(pt.x / 32.0f));
        const int ty = static_cast<int>(std::floor(pt.y / 32.0f));

        if(IsHazardTile(col.GetTileIndex(tx, ty)) || IsHazardTile(col.GetFrontTileIndex(tx, ty)))
            return true;
    }
    return false;
}

int PredictHazard(const CollisionView &col, Vec2 start_pos, Vec2 start_vel, int dir, int max_ticks) noexcept
{
    if(!col.IsValid())
        return 0;

    Vec2 pos = start_pos;
    Vec2 vel = start_vel;

    // Standard DDNet tuning defaults
    constexpr float ground_control_speed = 10.0f;
    constexpr float ground_control_accel = 2.0f;
    constexpr float ground_friction = 0.85f;
    constexpr float gravity = 0.5f;

    for(int tick = 1; tick <= max_ticks; ++tick)
    {
        if(dir != 0)
        {
            const float target_vx = static_cast<float>(dir) * ground_control_speed;
            vel.x += static_cast<float>(dir) * ground_control_accel;
            if(dir > 0 && vel.x > target_vx)
                vel.x = target_vx;
            else if(dir < 0 && vel.x < target_vx)
                vel.x = target_vx;
        }
        else
        {
            vel.x *= ground_friction;
        }

        vel.y += gravity;
        pos += vel;

        if(IsPositionHazard(col, pos))
            return tick;
    }

    return 0;
}

int ResolveSafeDirection(const CollisionView &col, Vec2 pos, Vec2 vel, int current_dir) noexcept
{
    if(!col.IsValid())
        return current_dir;

    const int hazard_tick = PredictHazard(col, pos, vel, current_dir, 15);
    if(hazard_tick == 0)
        return current_dir;

    if(current_dir != 0)
    {
        const int opposite = -current_dir;
        if(PredictHazard(col, pos, vel, opposite, 15) == 0)
            return opposite;

        if(PredictHazard(col, pos, vel, 0, 15) == 0)
            return 0;

        return opposite;
    }

    // Current dir is 0 (sliding / falling). Test counter-steering.
    const int risk_left = PredictHazard(col, pos, vel, -1, 15);
    const int risk_right = PredictHazard(col, pos, vel, 1, 15);

    if(risk_left == 0 && risk_right != 0)
        return -1;
    if(risk_right == 0 && risk_left != 0)
        return 1;

    return 0;
}

} // namespace avoidfreeze::proxy
