#pragma once
#include "TradingTypes.h"
#include <array>
#include <cstdint>
#include <iosfwd>

class Hideout;

namespace Trading {
class TradingSystem {
public:
    TradingSystem();
    void InitializeDefaults();
    void ResetAllTradersRandom(std::uint32_t seed);
    TraderState& GetTrader(TraderId id) { return traders_[static_cast<int>(id)]; }
    const TraderState& GetTrader(TraderId id) const { return traders_[static_cast<int>(id)]; }
    const std::vector<QuestDef>& QuestDefs() const { return questDefs_; }
    const std::vector<QuestProgress>& QuestProgresses() const { return questProgress_; }
    QuestProgress* FindProgress(int id);
    const QuestProgress* FindProgress(int id) const;
    int Price(const ItemStack& item) const;
    int BuyTotal(const TradeSession& session) const;
    int SellTotal(const TradeSession& session) const;
    bool StageFromTrader(TradeSession& session, int itemIndex, int x, int y);
    bool StageFromPlayer(TradeSession& session, Hideout& hideout, int itemIndex, int x, int y);
    bool UnstageBuy(TradeSession& session, int index);
    bool UnstageSell(TradeSession& session, Inventory& stash, int index);
    void Rollback(TradeSession& session, Inventory& stash);
    bool Commit(TradeSession& session, Hideout& hideout);
    bool AcceptQuest(int id, Hideout& hideout);
    bool DeliverQuest(int id, Hideout& hideout);
    bool CompleteQuest(int id, Hideout& hideout);
    void OnRaidEnemyKilled();
    void OnRaidPosition(float x, float z);
    void OnRaidExtracted();
    void OnRaidFailed();
    bool Write(std::ostream& out) const;
    bool Read(std::istream& in);
private:
    static bool CanPlace(const std::vector<StagedItem>& stage, int x, int y, int w, int h);
    void Advance(Objective objective, int amount = 1);
    std::array<TraderState, TraderCount> traders_;
    std::vector<QuestDef> questDefs_;
    std::vector<QuestProgress> questProgress_;
};
}
