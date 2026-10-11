#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

enum class BodyPart : int { Head, Torso, Pelvis, LeftArm, RightArm, LeftLeg, RightLeg, Count };
inline constexpr int BodyPartCount = static_cast<int>(BodyPart::Count);
inline constexpr const char* BodyPartNames[BodyPartCount]{
    "머리", "몸통", "골반", "왼팔", "오른팔", "왼다리", "오른다리"
};

class PlayerVitals
{
public:
    float Health() const
    {
        float total = 0;
        for (float value : bodyHealth) total += value;
        return total / BodyPartCount;
    }
    float BodyHealth(BodyPart part) const { return bodyHealth[static_cast<std::size_t>(part)]; }
    const std::array<float, BodyPartCount>& BodyHealth() const { return bodyHealth; }
    float Hydration() const { return hydration; }
    float Stamina() const { return stamina; }
    bool Alive() const
    {
        if (bodyHealth[0] + bodyHealth[1] <= 50) return false;
        return !std::all_of(bodyHealth.begin(), bodyHealth.end(), [](float value) { return value <= 50; });
    }
    bool CanSprint() const { return !exhausted && stamina > 0 && Alive(); }
    void Damage(float amount)
    {
        amount = (std::max)(0.0f, amount);
        for (float& value : bodyHealth) value = (std::max)(0.0f, value - amount);
    }
    void Damage(BodyPart part, float amount)
    {
        auto& value = bodyHealth[static_cast<std::size_t>(part)];
        value = (std::max)(0.0f, value - (std::max)(0.0f, amount));
    }
    bool Heal(BodyPart part)
    {
        auto& value = bodyHealth[static_cast<std::size_t>(part)];
        if (value >= 100) return false;
        value = (std::min)(100.0f, value + 20);
        return true;
    }
    bool Drink() { if (!Alive() || hydration >= 100) return false; hydration = (std::min)(100.0f, hydration + 30); return true; }
    bool Restore(const std::array<float, BodyPartCount>& body, float water, float energy)
    {
        if (!std::isfinite(water) || !std::isfinite(energy) || water < 0 || water > 100 || energy < 0 || energy > 100)
            return false;
        for (float value : body)
            if (!std::isfinite(value) || value < 0 || value > 100) return false;
        bodyHealth = body;
        hydration = water;
        stamina = energy;
        exhausted = stamina <= 0;
        return true;
    }
    void Update(float dt, bool sprinting)
    {
        if (!Alive()) return;
        hydration = (std::max)(0.0f, hydration - dt * 0.1f);
        stamina = std::clamp(stamina + dt * (sprinting ? -22.0f : 14.0f), 0.0f, 100.0f);
        if (stamina <= 0) exhausted = true;
        if (stamina >= 20) exhausted = false;
        if (hydration <= 0) Damage(dt * 0.5f);
    }
private:
    std::array<float, BodyPartCount> bodyHealth{100, 100, 100, 100, 100, 100, 100};
    float hydration = 100, stamina = 100;
    bool exhausted = false;
};
