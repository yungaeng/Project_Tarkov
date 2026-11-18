# 다운로드한 상업 이용 가능 애니메이션

확인일: 2026-11-19

아래 GLB/glTF 목록의 Quaternius 및 KayKit 세 팩은 동봉 License.txt에서 CC0-1.0을 확인했다. 이 세 팩은 게임 판매·수정·재배포가 가능하며 출처 표기는 필수가 아니다. 추가 Mixamo/ANIMMADE FBX는 별도 라이선스이므로 하단 링크로 구분한다. 이 목록은 취득 기록이며 실제 런타임 사용본은 별도 안내한다. 게임 실행 확인은 미완료다.

## 요청한 동작과 확보한 클립

- 걷기·달리기: Walk_Loop, Jog_Fwd_Loop, Sprint_Loop, Walking_A/B/C, Running_A/B, Walking_Backwards, Running_Strafe_Left/Right
- 웅크리기·포복: Crouch_Idle_Loop, Crouch_Fwd_Loop, Crouching, Crawling
- 무릎앉기 관련: Fixing_Kneeling (수리 동작이며 전투용 무릎 조준 대기는 아님)
- 권총: Pistol_Idle_Loop, Pistol_Aim_Down/Neutral/Up, Pistol_Shoot, Pistol_Reload, Ranged_1H_Aiming/Reload/Shoot/Shooting
- 소총 관련: Running_HoldingRifle, Ranged_2H_Aiming/Reload/Shoot/Shooting (범용 양손 원거리 동작; 실제 소총 그립 정렬 확인 필요)
- 피격: Hit_Chest, Hit_Head, Hit_Knockback, Hit_A, Hit_B
- 사망: Death01, Death_A, Death_B, Death_A_Pose, Death_B_Pose
- 기타: 점프·착지·회피·구르기·슬라이딩·눕기·일어나기·상호작용·아이템 사용·근접공격·도구 작업 등

이 폴더는 취득 원본과 미사용 자료를 보관한다. 실제 사용본과 연결 상태는 [게임용 모션 안내](../../../Animations/Characters/Imported/README.md)를 참고한다. 위 내용은 CC0 세 팩의 취득 목록이다. 추가 확보한 전투 모션은 [Mixamo_Combat/README.md](Mixamo_Combat/README.md)를 참고한다. FBX의 데이터 검사는 [combat-fbx-inventory.json](combat-fbx-inventory.json)에 기록한다. Lie_Down/Lie_Idle/Lie_StandUp은 이름만으로 엎드림 자세라고 단정하지 않는다.

## KayKit_CharacterAnimations

공식 출처: https://kaylousberg.itch.io/kaykit-character-animations

고유 클립 이름 144개 (기준 포즈 포함). FBX와 GLB/glTF의 같은 모션, 루트모션 버전은 별도 동작으로 중복 계산하지 않았다.

- `Melee_1H_Slash`
- `Melee_1H_Stab`
- `Melee_2H_Attack`
- `Melee_2H_Idle`
- `Melee_2H_Slam`
- `Melee_Block`
- `Melee_Block_Attack`
- `Melee_Block_Hit`
- `Melee_Blocking`
- `Melee_Dualwield_Slash`
- `Melee_Dualwield_SlashCombo`
- `Melee_Unarmed_Idle`
- `Melee_Unarmed_Kick`
- `Melee_Unarmed_Punch`
- `Melee_Unarmed_Smash`
- `T-Pose`
- `Death_A`
- `Death_A_Pose`
- `Hit_A`
- `Idle_A`
- `Idle_B`
- `Dodge_Backwards`
- `Dodge_Forward`
- `Dodge_Left`
- `Dodge_Right`
- `Running_A`
- `Walking_A`
- `Flexing`
- `EXPERIMENTAL_Large_Transform`
- `Melee_1H_Attack_Chop`
- `Melee_1H_Attack_Jump_Chop`
- `Melee_1H_Attack_Slice_Diagonal`
- `Melee_1H_Attack_Slice_Horizontal`
- `Melee_1H_Attack_Stab`
- `Melee_2H_Attack_Chop`
- `Melee_2H_Attack_Slice`
- `Melee_2H_Attack_Spin`
- `Melee_2H_Attack_Spinning`
- `Melee_2H_Attack_Stab`
- `Melee_Dualwield_Attack_Chop`
- `Melee_Dualwield_Attack_Slice`
- `Melee_Dualwield_Attack_Stab`
- `Melee_Unarmed_Attack_Kick`
- `Melee_Unarmed_Attack_Punch_A`
- `Ranged_1H_Aiming`
- `Ranged_1H_Reload`
- `Ranged_1H_Shoot`
- `Ranged_1H_Shooting`
- `Ranged_2H_Aiming`
- `Ranged_2H_Reload`
- `Ranged_2H_Shoot`
- `Ranged_2H_Shooting`
- `Ranged_Bow_Aiming_Idle`
- `Ranged_Bow_Draw`
- `Ranged_Bow_Draw_Up`
- `Ranged_Bow_Idle`
- `Ranged_Bow_Release`
- `Ranged_Bow_Release_Up`
- `Ranged_Magic_Raise`
- `Ranged_Magic_Shoot`
- `Ranged_Magic_Spellcasting`
- `Ranged_Magic_Spellcasting_Long`
- `Ranged_Magic_Summon`
- `Death_B`
- `Death_B_Pose`
- `Hit_B`
- `Interact`
- `PickUp`
- `Spawn_Air`
- `Spawn_Ground`
- `Throw`
- `Use_Item`
- `Crawling`
- `Crouching`
- `Dodge_Backward`
- `Running_HoldingBow`
- `Running_HoldingRifle`
- `Running_Strafe_Left`
- `Running_Strafe_Right`
- `Sneaking`
- `Walking_Backwards`
- `Jump_Full_Long`
- `Jump_Full_Short`
- `Jump_Idle`
- `Jump_Land`
- `Jump_Start`
- `Running_B`
- `Walking_B`
- `Walking_C`
- `Cheering`
- `Lie_Down`
- `Lie_Idle`
- `Lie_StandUp`
- `Push_Ups`
- `Sit_Chair_Down`
- `Sit_Chair_Idle`
- `Sit_Chair_StandUp`
- `Sit_Floor_Down`
- `Sit_Floor_Idle`
- `Sit_Floor_StandUp`
- `Sit_Ups`
- `Waving`
- `EXPERIMENTAL_Medium_Transform`
- `Skeletons_Awaken_Floor`
- `Skeletons_Awaken_Floor_Long`
- `Skeletons_Awaken_Standing`
- `Skeletons_Death`
- `Skeletons_Death_Pose`
- `Skeletons_Death_Resurrect`
- `Skeletons_Idle`
- `Skeletons_Inactive_Floor_Pose`
- `Skeletons_Inactive_Standing_Pose`
- `Skeletons_Spawn_Ground`
- `Skeletons_Taunt`
- `Skeletons_Taunt_Longer`
- `Skeletons_Walking`
- `Chop`
- `Chopping`
- `Dig`
- `Digging`
- `Fishing_Bite`
- `Fishing_Cast`
- `Fishing_Catch`
- `Fishing_Idle`
- `Fishing_Reeling`
- `Fishing_Struggling`
- `Fishing_Tug`
- `Hammer`
- `Hammering`
- `Holding_A`
- `Holding_B`
- `Holding_C`
- `Lockpick`
- `Lockpicking`
- `Pickaxe`
- `Pickaxing`
- `Saw`
- `Sawing`
- `Work_A`
- `Work_B`
- `Work_C`
- `Working_A`
- `Working_B`
- `Working_C`

## Quaternius_UniversalAnimationLibrary

공식 출처: https://quaternius.itch.io/universal-animation-library

고유 클립 이름 43개 (기준 포즈 포함). FBX와 GLB/glTF의 같은 모션, 루트모션 버전은 별도 동작으로 중복 계산하지 않았다.

- `A_TPose`
- `Crouch_Fwd_Loop`
- `Crouch_Idle_Loop`
- `Dance_Loop`
- `Death01`
- `Driving_Loop`
- `Fixing_Kneeling`
- `Hit_Chest`
- `Hit_Head`
- `Idle_Loop`
- `Idle_Talking_Loop`
- `Idle_Torch_Loop`
- `Interact`
- `Jog_Fwd_Loop`
- `Jump_Land`
- `Jump_Loop`
- `Jump_Start`
- `PickUp_Table`
- `Pistol_Aim_Down`
- `Pistol_Aim_Neutral`
- `Pistol_Aim_Up`
- `Pistol_Idle_Loop`
- `Pistol_Reload`
- `Pistol_Shoot`
- `Punch_Cross`
- `Punch_Jab`
- `Push_Loop`
- `Roll`
- `Sitting_Enter`
- `Sitting_Exit`
- `Sitting_Idle_Loop`
- `Sitting_Talking_Loop`
- `Spell_Simple_Enter`
- `Spell_Simple_Exit`
- `Spell_Simple_Idle_Loop`
- `Spell_Simple_Shoot`
- `Sprint_Loop`
- `Swim_Fwd_Loop`
- `Swim_Idle_Loop`
- `Sword_Attack`
- `Sword_Idle`
- `Walk_Formal_Loop`
- `Walk_Loop`

## Quaternius_UniversalAnimationLibrary2

공식 출처: https://quaternius.itch.io/universal-animation-library-2

고유 클립 이름 43개 (기준 포즈 포함). FBX와 GLB/glTF의 같은 모션, 루트모션 버전은 별도 동작으로 중복 계산하지 않았다.

- `A_TPose`
- `Chest_Open`
- `ClimbUp_1m`
- `Consume`
- `Farm_Harvest`
- `Farm_PlantSeed`
- `Farm_Watering`
- `Hit_Knockback`
- `Idle_FoldArms_Loop`
- `Idle_Lantern_Loop`
- `Idle_No_Loop`
- `Idle_Rail_Call`
- `Idle_Rail_Loop`
- `Idle_Shield_Break`
- `Idle_Shield_Loop`
- `Idle_TalkingPhone_Loop`
- `LayToIdle`
- `Melee_Hook`
- `Melee_Hook_Rec`
- `NinjaJump_Idle_Loop`
- `NinjaJump_Land`
- `NinjaJump_Start`
- `OverhandThrow`
- `Shield_Dash`
- `Shield_OneShot`
- `Slide_Exit`
- `Slide_Loop`
- `Slide_Start`
- `Sword_Block`
- `Sword_Dash`
- `Sword_Heavy_Combo`
- `Sword_Regular_A`
- `Sword_Regular_A_Rec`
- `Sword_Regular_B`
- `Sword_Regular_B_Rec`
- `Sword_Regular_C`
- `Sword_Regular_Combo`
- `TreeChopping_Loop`
- `Walk_Carry_Loop`
- `Yes`
- `Zombie_Idle_Loop`
- `Zombie_Scratch`
- `Zombie_Walk_Fwd_Loop`

## 추가 FBX 자료와 별도 라이선스

- [Mixamo 전투 모션](Mixamo_Combat/README.md): 사용자가 직접 로그인한 뒤 공식 사이트에서 취득. 상업용 게임 이용 가능, CC0 아님. 원본 에셋 독립 재배포 제한.
- [ANIMMADE 낮은 자세 소총 사격](Animmade_RifleCrouchShooting/README.md): 상품 페이지에서 상업 이용 허용을 확인한 보조 자료. CC0 아님.
- Cransh Sketchfab 권총·소총 1인칭 팩은 다운로드하지 않았다.
- 유료 Pro/Source 에디션은 구매하지 않았다.

GLB/glTF 모션 이름은 파일 메타데이터에서 추출했다. 총기와 손의 정렬, 사망 연출의 방향·품질은 재생 검증 전이다.
