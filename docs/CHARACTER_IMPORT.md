# Character appearance and rifle posing

## Current appearance (2026-10-10)

The active KimGawon assets were regenerated from
`F:/Koikatsu/Export_PMX/20261010085443_김가원/model.pmx` and its
`Outfit 00/model.pmx`. This export contains the body and hair only, with no
clothing meshes. Both PMX files are combined; no garments from the previous
export are retained. The missing clothing alpha mask is optional during import.
The existing runtime rig, clothing inventory, and shader base-layer behavior
are unchanged. Clothing inventory controls do not add visible garments to this
body-only asset. The asset contains 31,550 vertices, 48,842 triangles and 52 joints.
Validation was performed without building or running the game.

The dressed-outfit details below describe the previous export.

Player and enemy `CharacterVisual` instances share the imported KimGawon appearance in
`Assets/Models/Player/KimGawon`. The supplied body PMX and `Outfit 00/model.pmx` are
combined into a fully dressed character, normalized to 1.8 meters. The source export
is not modified. Alternate clothing states, duplicate shadow geometry and temporary
expression overlays are excluded; the exported body mask hides skin under clothing.

The supplied face, left/right eye and body color textures are preserved in the atlas.
Other exported color textures are included where available. Hair uses the export's
material color because its referenced main color textures are absent.

`Ch22_nonPBR.fbx` now supplies only the animation rig. `LoadAppearance` replaces its
geometry and inverse bind matrices, fits 52 joints to the new body, and offsets clip
translation keys to preserve the new proportions. All actors share the immutable
geometry, textures and clips while retaining separate poses. Clothing weights are
transferred from four nearby body-surface vertices, then normalized to four gameplay
joint influences. Sleeves follow the arms, the skirt follows the hips/thighs, and
socks/shoes follow the legs/feet. Body and clothing use the same bone palette in the
same draw, including crouch, rifle IK and death poses. `import.json` records each
clothing vertex range and its bound joints; the validator exercises both sides with
30-degree limb rotations. PMX facial morphs and hair/skirt secondary physics are not
imported; hair follows its mapped bones and the skirt uses the body skinning above.

To regenerate the assets (Python with NumPy/Pillow and the existing Assimp DLL):

```powershell
python Tools/import_pmx_character.py 'F:/Koikatsu/Export_PMX/20261010000752_김가원/model.pmx' --rig Project_Tarkov/Project_Tarkov/Assets/Models/Player/Ch22_nonPBR.fbx --assimp vcpkg_installed/x64-windows/bin/assimp-vc143-mt.dll --output Project_Tarkov/Project_Tarkov/Assets/Models/Player/KimGawon
python Tools/validate_character.py Project_Tarkov/Project_Tarkov/Assets/Models/Player/KimGawon --preview .build/qa/character-preview.png
```

The `.mesh` format is little endian: `TKCHAR02`, three uint32 counts (joints,
vertices, indices), joints (uint32 ASCII-name length, name, float3 bind position),
vertices (float3 position, float3 normal, float2 UV, uint32x4 joints, float4 weights,
uint32 garment),
and uint32 triangle indices. Positions use model centimeters. The importer reflects
PMX Z and reverses triangle winding. Atlas UVs use the PNG's top-left row origin.

Garment IDs are 0 (permanent geometry), 1 (shirt), 2 (skirt), 3 (socks), 4 (shoes),
and 5 (body). The renderer takes each character's four-bit clothing mask and filters
only the corresponding garments. Permanent underwear remains, and removing the
shirt reveals a dark base layer instead of holes in the masked body. The loader
continues to accept `TKCHAR01` assets without garment IDs.

Clothing is represented by inventory item types 3..6, each with a maximum stack of
one and its own weight. The hideout's clothing buttons equip from the carried bag
first, then from the stash; removing an item returns it to the stash. In a raid,
select an item and press Equip. Removing worn clothing requires a free bag slot;
if the bag is full, the item stays equipped. Bag items use the existing drop/pickup
flow, and clothing is included in world loot. No armor protection is granted.

`HIDEOUT 2` stores seven stash counts and the clothing mask before the carried-item
count. Version 1 saves still load with the original three item IDs unchanged and a
starter outfit plus one spare of each garment. Equipped clothing is recovered on
extraction and lost on death/missing/abandoned raids, like other deployed gear.
Save failure rolls back hideout clothing transfers along with the other state.

The rifle's butt stock proportions are shortened, with overall rendered length set
to 0.943 m as an AK-74-sized reference; this remains the project's stylized rifle,
not a detailed reproduction. Dimension reference:
https://sureshot-armament.com/wp-content/uploads/2023/12/ak_ebook.pdf
All four weapon mesh parts share the same scale. Rifle animation pivots around the
butt pad ahead of the shoulder. Two-bone arm IK and wrist alignment follow the same
final weapon transform used for drawing; the support hand follows the magazine
during reload. Unreachable targets are clamped to arm length rather than stretching.

No project build or game run was performed for this change. Asset validation and
software-rendered appearance previews do not replace an in-game check of walking,
crouching, aiming, reload transitions and extreme aim angles.
