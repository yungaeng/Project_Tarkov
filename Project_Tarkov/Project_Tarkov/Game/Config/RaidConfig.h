#pragma once

namespace RaidConfig
{
    constexpr float MapWidth = 500.0f;
    constexpr float MapLength = 500.0f;
    constexpr float RaidDuration = 1500.0f;
    constexpr int MinPlayers = 6, MaxPlayers = 8;
    constexpr int InitialAICount = 10, MaxActiveAI = 12, MaxTotalAISpawns = 20;
    constexpr int SpawnCandidateCount = 16;
    constexpr float PreferredSpawnSeparation = 120.0f;
    constexpr int ExtractionCount = 4;
    constexpr float DefaultExtractTime = 8.0f;
    constexpr int LootPointCount = 100;
    constexpr float WalkSpeed = 3.0f, RunSpeed = 6.0f;
    // Map 01 MVP uses the reduced counts specified in section 11.
    constexpr int MvpLootCount = 20;
    constexpr float ExtractRadius = 6.0f;
}
