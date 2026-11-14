#include "TradingSystem.h"
#include "../Hideout/Hideout.h"
#include <algorithm>
#include <istream>
#include <limits>
#include <ostream>
#include <random>

namespace Trading {
namespace {
constexpr int ItemId(ItemType item) { return static_cast<int>(item); }
}

TradingSystem::TradingSystem() { InitializeDefaults(); }

void TradingSystem::InitializeDefaults() {
    traders_[0] = {TraderId::WeaponAmmo, "무기·탄약 상인", "trader_weapon", 1, {}};
    traders_[1] = {TraderId::MedicalFood, "의료·식량 상인", "trader_medical", 1, {}};
    traders_[2] = {TraderId::ClothingGear, "의류·장구 상인", "trader_gear", 1, {}};
    for (auto& trader : traders_) trader.stash.SetSlotCapacity(TraderStashCapacity);

    questDefs_.clear();
    questDefs_.push_back({101, TraderId::WeaponAmmo,-1,"초기 탄약 확보","공용 탄약 30발을 납품하십시오.","탄약이 필요해.","탄약을 가져왔나?","좋아, 이 정도면 버틸 수 있겠어.",Objective::DeliverItem,ItemType::Ammo,30,15000,1,ItemType::Magazine});
    questDefs_.push_back({102, TraderId::WeaponAmmo,-1,"산업단지 소탕","PMC로 적대 인원 3명을 처치하십시오.","산업단지의 위협을 줄여 줘.","계속 소탕해.","깔끔하게 처리했군.",Objective::KillEnemies,ItemType::Ammo,3,25000,1,ItemType::Rifle});
    questDefs_.push_back({201, TraderId::MedicalFood,-1,"비상 의료 물자","붕대 3개를 납품하십시오.","붕대를 구해 줘.","붕대를 기다리고 있어.","고마워. 치료에 도움이 되겠어.",Objective::DeliverItem,ItemType::Bandage,3,12000,2,ItemType::Water});
    questDefs_.push_back({301, TraderId::ClothingGear,-1,"중앙 공장 정찰","PMC로 중앙 공장 주변을 정찰하고 생환하십시오.","공장 주변을 확인하고 돌아와.","정찰은 끝났나?","살아 돌아왔군. 수고했어.",Objective::VisitLocation,ItemType::Ammo,1,20000,1,ItemType::Backpack,0.f,0.f,25.f});

    questDefs_.push_back({401,TraderId::MedicalFood,0,"응급 처치 물자","붕대 2개를 가져다주십시오.","다친 사람을 치료할 붕대가 필요해요.","붕대를 찾았나요?","덕분에 동료를 살릴 수 있겠어요.",Objective::DeliverItem,ItemType::Bandage,2,8000,2,ItemType::Food});
    questDefs_.push_back({402,TraderId::WeaponAmmo,1,"위협 제거","PMC로 적대 인원 2명을 처치하십시오.","복도 밖의 적을 정리해 주세요.","아직 안심할 수 없어요.","이제 조금은 안전하겠네요.",Objective::KillEnemies,ItemType::Ammo,2,10000,1,ItemType::Magazine});
    questDefs_.push_back({403,TraderId::ClothingGear,2,"공장 외곽 정찰","PMC로 중앙 공장 주변을 확인하고 생환하십시오.","공장 외곽의 접근로를 확인해 주세요.","정찰 결과를 기다리고 있어요.","정보가 큰 도움이 되겠어요.",Objective::VisitLocation,ItemType::Ammo,1,12000,1,ItemType::Helmet,0.f,0.f,30.f});
    questProgress_.clear();
    for (const auto& def : questDefs_) questProgress_.push_back({def.id,QuestStatus::Available,0});
    ResetAllTradersRandom(20261113u);
}

void TradingSystem::ResetAllTradersRandom(std::uint32_t seed) {
    std::mt19937 rng(seed);
    struct Stock { ItemType item; int low, high, stacksLow, stacksHigh; };
    const std::array<std::vector<Stock>,TraderCount> pools{{
        {{ItemType::Rifle,1,1,1,2},{ItemType::Magazine,1,1,2,4},{ItemType::Pistol,1,1,1,2},{ItemType::PistolMagazine,1,1,2,4},{ItemType::Ammo,15,30,2,5}},
        {{ItemType::Bandage,1,5,3,6},{ItemType::Water,1,2,3,6},{ItemType::Food,1,2,2,4}},
        {{ItemType::Shirt,1,1,1,2},{ItemType::Trousers,1,1,1,2},{ItemType::Vest,1,1,1,2},{ItemType::Helmet,1,1,1,2},{ItemType::Backpack,1,1,1,2},{ItemType::Boots,1,1,1,2}}
    }};
    for (int t=0;t<TraderCount;++t) {
        auto& stash=traders_[t].stash;
        stash=Inventory{}; stash.SetSlotCapacity(TraderStashCapacity);
        for (const auto& entry:pools[t]) {
            const int stackCount=std::uniform_int_distribution<int>(entry.stacksLow,entry.stacksHigh)(rng);
            for (int i=0;i<stackCount;++i) {
                const int count=std::uniform_int_distribution<int>(entry.low,entry.high)(rng);
                if (!stash.TryAdd({entry.item,count})) break;
            }
        }
    }
}

QuestProgress* TradingSystem::FindProgress(int id) { for (auto& p:questProgress_) if (p.questId==id) return &p; return nullptr; }
const QuestProgress* TradingSystem::FindProgress(int id) const { for (const auto& p:questProgress_) if (p.questId==id) return &p; return nullptr; }
int TradingSystem::Price(const ItemStack& item) const { return GetItemDefinition(item.type).price * item.quantity; }
int TradingSystem::BuyTotal(const TradeSession& s) const { int total=0;for(const auto& i:s.buy) total+=Price(i.stack);return total; }
int TradingSystem::SellTotal(const TradeSession& s) const { int total=0;for(const auto& i:s.sell) total+=Price(i.stack);return total; }

bool TradingSystem::CanPlace(const std::vector<StagedItem>& stage,int x,int y,int w,int h) {
    if(x<0||y<0||w<1||h<1||x+w>StageColumns||y+h>StageRows)return false;
    for(const auto& item:stage) {
        const auto size=ItemFootprint(item.stack.type);
        if(x<item.stageX+size.width&&x+w>item.stageX&&y<item.stageY+size.height&&y+h>item.stageY)return false;
    }
    return true;
}

bool TradingSystem::StageFromTrader(TradeSession& s,int index,int x,int y) {
    auto& items=GetTrader(s.trader).stash;
    if(index<0||index>=static_cast<int>(items.Items().size()))return false;
    const ItemStack source=items.Items()[index];const auto size=ItemFootprint(source.type);
    if(!CanPlace(s.buy,x,y,size.width,size.height))return false;
    StagedItem staged{source,x,y,source.slot};
    if(!items.Remove(source.id,source.quantity))return false;
    s.buy.push_back(staged);return true;
}
bool TradingSystem::StageFromPlayer(TradeSession& s,Hideout& hideout,int index,int x,int y) {
    auto& stash=hideout.storage;
    if(index<0||index>=static_cast<int>(stash.Items().size()))return false;
    const ItemStack source=stash.Items()[index];const auto size=ItemFootprint(source.type);
    if(source.type==ItemType::Vest||source.type==ItemType::Backpack) {
        const int first=source.type==ItemType::Vest?12:18;
        const int last=source.type==ItemType::Vest?18:30;
        for(int slot=first;slot<last;++slot)if(hideout.loadout.At(slot)) {
            s.message="아이템이 들어 있는 조끼·배낭은 판매할 수 없습니다.";
            return false;
        }
    }
    if(!CanPlace(s.sell,x,y,size.width,size.height))return false;
    StagedItem staged{source,x,y,source.slot};
    if(!stash.Remove(source.id,source.quantity))return false;
    s.sell.push_back(staged);return true;
}
bool TradingSystem::UnstageBuy(TradeSession& s,int i) {
    if(i<0||i>=static_cast<int>(s.buy.size()))return false;
    auto& stash=GetTrader(s.trader).stash;auto item=s.buy[i];item.stack.slot=item.originSlot;
    if(!stash.TryAdd(item.stack)){item.stack.slot=-100;if(!stash.TryAdd(item.stack))return false;}
    s.buy.erase(s.buy.begin()+i);return true;
}
bool TradingSystem::UnstageSell(TradeSession& s,Inventory& stash,int i) {
    if(i<0||i>=static_cast<int>(s.sell.size()))return false;
    auto item=s.sell[i];item.stack.slot=item.originSlot;
    if(!stash.TryAdd(item.stack)){item.stack.slot=-100;if(!stash.TryAdd(item.stack))return false;}
    s.sell.erase(s.sell.begin()+i);return true;
}
void TradingSystem::Rollback(TradeSession& s,Inventory& stash) {
    while(!s.buy.empty()&&!UnstageBuy(s,static_cast<int>(s.buy.size())-1))break;
    while(!s.sell.empty()&&!UnstageSell(s,stash,static_cast<int>(s.sell.size())-1))break;
}

bool TradingSystem::Commit(TradeSession& s,Hideout& hideout) {
    if(s.buy.empty()&&s.sell.empty())return false;
    const auto net=static_cast<std::int64_t>(BuyTotal(s))-SellTotal(s);
    if(net>hideout.rubles){s.message="루블이 부족합니다.";return false;}
    if(net<0&&hideout.rubles>(std::numeric_limits<std::int64_t>::max)()+net){s.message="정산 금액 범위를 초과했습니다.";return false;}
    Hideout updated=hideout;
    for(auto item:s.buy){item.stack.slot=-100;if(!updated.storage.TryAdd(item.stack)){s.message="창고에 구매 물품을 넣을 공간이 없습니다.";return false;}}
    auto& trader=updated.trading.GetTrader(s.trader).stash;
    for(auto item:s.sell){item.stack.slot=-100;if(!trader.TryAdd(item.stack)){s.message="상인 창고에 판매 물품을 넣을 공간이 없습니다.";return false;}}
    updated.rubles-=net;
    if(!updated.Save()){s.message=updated.message;return false;}
    hideout=std::move(updated);s.buy.clear();s.sell.clear();s.message="거래를 완료했습니다.";return true;
}

bool TradingSystem::AcceptQuest(int id,Hideout& h) {
    auto* progress=FindProgress(id);if(!progress||progress->status!=QuestStatus::Available)return false;
    Hideout updated=h;auto* p=updated.trading.FindProgress(id);p->status=QuestStatus::Active;p->count=0;
    if(!updated.Save())return false;h=std::move(updated);return true;
}
bool TradingSystem::DeliverQuest(int id,Hideout& h) {
    const auto it=std::find_if(questDefs_.begin(),questDefs_.end(),[&](const auto& q){return q.id==id;});
    const auto* p=FindProgress(id);if(it==questDefs_.end()||!p||p->status!=QuestStatus::Active||it->objective!=Objective::DeliverItem)return false;
    Hideout updated=h;if(updated.storage.Count(it->targetItem)<it->required)return false;
    if(updated.storage.Take(it->targetItem,it->required)!=it->required)return false;
    auto* progress=updated.trading.FindProgress(id);progress->count=it->required;progress->status=QuestStatus::ReadyToComplete;
    if(!updated.Save())return false;h=std::move(updated);return true;
}
bool TradingSystem::CompleteQuest(int id,Hideout& h) {
    const auto it=std::find_if(questDefs_.begin(),questDefs_.end(),[&](const auto& q){return q.id==id;});
    const auto* p=FindProgress(id);if(it==questDefs_.end()||!p||p->status!=QuestStatus::ReadyToComplete)return false;
    Hideout updated=h;if(updated.rubles>(std::numeric_limits<std::int64_t>::max)()-it->rewardRubles)return false;
    updated.rubles+=it->rewardRubles;
    if(it->rewardItemCount>0&&!updated.storage.TryAdd({it->rewardItem,it->rewardItemCount})){h.message="보상 아이템을 넣을 창고 공간이 없습니다.";return false;}
    auto* progress=updated.trading.FindProgress(id);progress->status=QuestStatus::Completed;
    auto& trader=updated.trading.GetTrader(it->trader);trader.loyalty=(std::min)(1000,trader.loyalty+1);
    if(!updated.Save())return false;h=std::move(updated);return true;
}

void TradingSystem::Advance(Objective objective,int amount) {
    for(auto& p:questProgress_) {
        if(p.status!=QuestStatus::Active)continue;
        const auto it=std::find_if(questDefs_.begin(),questDefs_.end(),[&](const auto& q){return q.id==p.questId;});
        if(it==questDefs_.end()||it->objective!=objective)continue;
        p.count=(std::min)(it->required,p.count+amount);
        if(p.count>=it->required)p.status=QuestStatus::ReadyToComplete;
    }
}
void TradingSystem::OnRaidEnemyKilled(){Advance(Objective::KillEnemies);}
void TradingSystem::OnRaidPosition(float x,float z) {
    for(auto& p:questProgress_) {
        if(p.status!=QuestStatus::Active)continue;
        const auto it=std::find_if(questDefs_.begin(),questDefs_.end(),[&](const auto& q){return q.id==p.questId;});
        if(it==questDefs_.end()||it->objective!=Objective::VisitLocation||p.count)continue;
        const float dx=x-it->targetX,dz=z-it->targetZ;if(dx*dx+dz*dz<=it->targetRadius*it->targetRadius)p.count=1;
    }
}
void TradingSystem::OnRaidExtracted(){
    Advance(Objective::ExtractSuccess);
    for(auto& p:questProgress_) {
        if(p.status!=QuestStatus::Active||p.count==0)continue;
        const auto it=std::find_if(questDefs_.begin(),questDefs_.end(),[&](const auto& q){return q.id==p.questId;});
        if(it!=questDefs_.end()&&it->objective==Objective::VisitLocation)p.status=QuestStatus::ReadyToComplete;
    }
}
void TradingSystem::OnRaidFailed(){
    for(auto& p:questProgress_) {
        if(p.status!=QuestStatus::Active||p.count==0)continue;
        const auto it=std::find_if(questDefs_.begin(),questDefs_.end(),[&](const auto& q){return q.id==p.questId;});
        if(it!=questDefs_.end()&&it->objective==Objective::VisitLocation)p.count=0;
    }
}

bool TradingSystem::Write(std::ostream& out) const {
    auto writeInventory=[&](const Inventory& inv){out<<inv.SlotCapacity()<<' '<<inv.Items().size()<<'\n';for(const auto& i:inv.Items())out<<ItemId(i.type)<<' '<<i.quantity<<' '<<i.slot<<' '<<i.rounds<<' '<<i.magazineInserted<<' '<<static_cast<int>(i.fireMode)<<'\n';};
    out<<"TRADING 1\n";
    for(const auto& t:traders_){out<<t.loyalty<<'\n';writeInventory(t.stash);}
    out<<questProgress_.size()<<'\n';for(const auto& p:questProgress_)out<<p.questId<<' '<<static_cast<int>(p.status)<<' '<<p.count<<'\n';
    return static_cast<bool>(out);
}
bool TradingSystem::Read(std::istream& in) {
    std::string marker;int version=0;if(!(in>>marker>>version)||marker!="TRADING"||version!=1)return false;
    auto readInventory=[&](Inventory& inv){int capacity=0,count=0;if(!(in>>capacity>>count)||capacity<TraderStashCapacity||count<0||count>10000)return false;inv=Inventory{};if(!inv.SetSlotCapacity(capacity))return false;for(int n=0;n<count;++n){int type,slot,inserted,mode;ItemStack item{ItemType::Bandage,0};if(!(in>>type>>item.quantity>>slot>>item.rounds>>inserted>>mode)||type<0||type>=ItemTypeCount||item.quantity<1||item.quantity>GetItemDefinition(static_cast<ItemType>(type)).maxStack||inserted<0||inserted>1||mode<0||mode>2)return false;item.type=static_cast<ItemType>(type);item.slot=slot;item.magazineInserted=inserted!=0;item.fireMode=static_cast<FireMode>(mode);if(inv.Covering(slot)||!inv.TryAdd(item))return false;}return true;};
    for(auto& t:traders_){if(!(in>>t.loyalty)||t.loyalty<1||t.loyalty>1000||!readInventory(t.stash))return false;}
    int count=0;if(!(in>>count)||count!=static_cast<int>(questDefs_.size()))return false;
    std::vector<QuestProgress> progress;progress.reserve(count);
    for(int n=0;n<count;++n){int id,status,value;if(!(in>>id>>status>>value)||status<0||status>static_cast<int>(QuestStatus::Completed)||value<0)return false;const auto def=std::find_if(questDefs_.begin(),questDefs_.end(),[&](const auto& q){return q.id==id;});if(def==questDefs_.end()||value>def->required)return false;progress.push_back({id,static_cast<QuestStatus>(status),value});}
    questProgress_=std::move(progress);return true;
}
}
