# Inventory item photographs

Asset: `items.png`. Created 2026-11-03 with the built-in `image_gen` tool, then edited with the user's supplied black string-thong product reference. The source generation remains outside the repository; this final PNG is the runtime asset. No API/CLI fallback was used.

The atlas is an equal 4×4 grid. Cells follow the numeric ItemType order: bandage, water, ammo, shirt; trousers, sports bra, underpants, vest; helmet, backpack, boots, rifle; rifle magazine, pistol, pistol magazine, unused. UI uses normalized quarter-cell UV coordinates. The background is intentionally opaque charcoal; do not enable alpha-cutout processing.

Generation prompt:

> Use case: product-mockup. Game inventory texture atlas, square 4 by 4 EXACT equal cell grid, no borders, no text. Each object centered fully inside its own cell with generous 15% margins. Realistic detailed isolated studio product photographs, dark charcoal background uniform. Row 1: rolled medical gauze bandage; clear water bottle blue cap; cluster of brass rifle cartridges; multicam combat long sleeve shirt. Row 2: multicam combat trousers; black sports bra garment; black underwear garment; multicam short tactical plate carrier vest. Row 3: multicam ballistic helmet; olive tactical backpack; pair of black combat boots; modern black AK style rifle horizontal. Row 4: curved black rifle magazine; black semiautomatic pistol horizontal; straight black pistol magazine; empty cell. No people, no symbols or icons. Consistent soft studio lighting, realistic fabric metal plastic textures.

Edit prompt (image 1 = generated atlas; image 2 = user reference):

> Edit image 1 inventory atlas only at row 2 column 3: replace black underwear with the garment in image 2 reference, black thin string thong with triangular front, product only no person. Keep EXACT 4x4 grid positions and all other 14 item photographs unchanged. Same dark charcoal background and detailed realistic product rendering. No text.

The photographs are inventory representations, not exact renders of the existing game meshes. The character underwear mesh is unchanged by this atlas edit.

## Trader portraits

`trader_weapon.png`, `trader_medical.png`, and `trader_gear.png` are square pixel-art portraits displayed in the hideout stairwell trading panel. All three share the weapon trader's coarse 16-bit PC-98-style dots, limited palette, dithered stipple shading, chunky outlines, and stepped pixel edges. Each trader has a serious, unsmiling expression. The weapon trader is an older male arms dealer; the gear trader is an adult female clothing/kit trader. The medical trader is an adult woman in her early twenties, with a Bronya-inspired cool silver palette, realistic loose shoulder-length hair, and a Korean digital-camouflage field medic uniform with a red-cross armband. Her open jacket frames a low-neck black lace-trimmed top. Each portrait keeps a supply-room background suited to its trader's inventory.

The final portraits were generated and refined with the built-in `image_gen` tool on 2026-11-13. They are runtime assets; source generation files remain outside the repository.
