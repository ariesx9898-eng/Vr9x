# Orsted - Original Character Model Spec (production)

Status: **ORIGINAL design sheet for this project.** It keeps the canon identity cues (see `Docs/Lore_Research.md` §2):
silver hair, golden sanpaku eyes, a very tall frame, a pale fur-trimmed coat and no visible weapon. Every tailoring,
proportion, palette and animation decision below is our own. Do not trace anime key art or official illustrations.

Runtime hookup (from `Content/Data/Characters.json`):

| Field | Value |
|---|---|
| Skeletal mesh | `/Game/Characters/Orsted/SK_Orsted.SK_Orsted` |
| Anim BP | `/Game/Characters/Orsted/ABP_Orsted.ABP_Orsted_C` |
| Capsule | radius **38**, half-height **98** (196 cm capsule) |
| Mesh offset Z | **-98** (pivot at the soles; feet touch the capsule bottom) |
| Stance enum | `Orsted` (upright, still, economical) |
| Aura colour | pale gold/silver, `R 0.93 G 0.86 B 0.62` |

## 1. Silhouette

- **Read at 30 m: a tall vertical column with a wide collar.** The long coat hangs straight to mid-calf, and a broad fur collar squares the shoulders. Arms stay close to the body. No weapon breaks the outline.
- Contrast with Rudeus (161.7 cm, short cloak, forward lean): Orsted is about 33 cm taller and never leans in. The pale coat makes him the brightest vertical shape on screen, and the dark legs and boots anchor him to the ground.
- The negative space between coat panels shows as a narrow dark "V" of the undercoat at the chest.

## 2. Proportions (~195 cm to the crown, bare feet; ~197 cm in boots)

| Measure | Value | Note |
|---|---|---|
| Head count | 8 heads | Heroic but not exaggerated. Head height ~24.5 cm |
| Shoulder width (bone) | 46 cm, 54 cm with collar | Collar adds bulk, not the body |
| Chest / waist | lean V-taper, waist ~80 cm circumference | "Average physical fit" per sources, athletic rather than bulky |
| Leg length (crotch to floor) | ~95 cm (48.5%) | Long legs sell the gliding Dragon Step |
| Hands | long fingers, large palms | Open-palm strikes are the key poses; hands must read clearly |
| Neck | long, straight | Supports the still, upright head carriage |

Skeleton: UE5 Mannequin-compatible hierarchy (retargetable), scaled for height. Sockets used by data:
`hand_r`, `hand_l`, `foot_r`, `head`, `spine_03`. Add a `DragonStepTarget` motion-warp target in `AM_Orsted_DragonStep`.

## 3. Face and hair

- Face: long, angular, severe. High cheekbones, strong straight brows, thin mouth set in a neutral line. Age reads as late 20s to 30s. The default expression is **neutral and unreadable**, not scowling. The curse is what makes people afraid, not the face, so avoid villain-coding.
- Eyes: **golden, sanpaku** (iris sits high with visible white below). This is the most important canon cue. Iris `#D4A62A` with a darker limbal ring `#8A6415`. Add an optional faint emissive rim (0-0.2) driven by `State.Aura` / awakening.
- Hair: **silver**, medium length, swept back from the forehead with a few loose strands at the temples, falling to the nape. Use hair cards with a cool silver base and soft white highlights. Anisotropic shading should stay subtle, never chrome.

## 4. Palette

| Role | Hex | Notes |
|---|---|---|
| Hair base / highlight | `#BFC4CA` / `#EEF1F3` | Cool silver, never yellow |
| Eyes | `#D4A62A` | Matches the aura gold |
| Coat shell | `#E9E4DA` | Warm off-white, low saturation, roughness 0.7 |
| Fur trim | `#F3EFE6` with `#CFC7B8` roots | Soft groom, short and dense |
| Undercoat / tunic | `#2B2E35` | Charcoal with a slight blue tint |
| Trousers | `#3A3F48` | Dark slate |
| Boots / gloves | `#26211D` | Dark brown-black leather |
| Metal accents | `#B9A36A` | Muted brass-gold clasps, very few |
| Aura FX | `#EDDC9E` to `#FFFFFF` | Pale gold into silver (`AuraColor`) |

Rule: 70% pale (coat), 25% dark (undercoat, legs), 5% gold (eyes, clasps, FX).

## 5. Clothing breakdown (original tailoring)

1. **Greatcoat**: long, single-breasted, closed from sternum to waist with three concealed clasps. Splits from the waist into four panels (front left/right, back left/right) for cloth simulation. Hem at mid-calf. Sleeves are straight with deep cuffs, and the forearms are visible when the palms strike.
2. **Collar**: wide stand-up fur collar that wraps to the shoulder points. It frames the face at the jaw but never covers the mouth (the face must read in dialogue).
3. **Undercoat**: fitted charcoal tunic with a high band collar, visible in the chest "V" and under the cuffs.
4. **Belt**: plain dark leather at the waist under the coat, with a small brass buckle. **No sheath, no weapon.**
5. **Trousers**: dark slate, straight cut, tucked into boots.
6. **Boots**: knee-high, dark leather, low heel, reinforced toe (he kicks as well as strikes).
7. **Gloves**: none by default (bare hands for the palm style). Thin dark gloves are an optional variant.
8. **Optional late-game cosmetic**: a **black curse-suppressing helm**, a nod to the canon helmet made by Cliff (spoiler; unlock after story). The design is an original smooth black full helm with a narrow gold-lit visor slit.

Materials: the coat shell uses heavy wool/canvas normal detail, fur uses a groom or shell cards, leather is worn at
the creases. Keep the coat clean. He is immaculate, not battle-worn.

## 6. Posture and animation personality

Core idea: **nothing wasted.** He is the calmest thing in any fight.

| State | Direction |
|---|---|
| Idle | Upright, weight centred, hands relaxed at his sides or loosely clasped behind his back. Minimal sway (amplitude about 1/3 of Rudeus). Rare slow blink. The head tracks threats by turning the eyes first, then the head. |
| Walk (160 cm/s) | Long, unhurried stride; almost no vertical bob; arms swing very little. |
| Run / sprint (470 / 720) | Stays upright (<8 degrees forward lean); the coat tails trail. High acceleration and braking (4000), so starts and stops are near-instant with no skid. |
| Turning | Slow, deliberate yaw (`RotationRateYaw 420`). The body follows the eyes. |
| Palm Strike (`AM_Orsted_Basic`) | 0.12 s startup from a half-step. The strike comes from the hips with an open palm, then returns to neutral straight away. No flourish. |
| Disturb Magic (`AM_Orsted_DisturbMagic`) | A small raised-hand gesture toward the incoming spell, fingers spread, in a 0.35 s window. On a whiff the recovery shows a readable 0.45 s reset of the hand. |
| Dragon Step (`AM_Orsted_DragonStep`) | Low glide with the feet barely leaving the ground and the coat snapping flat behind. Motion-warped to `DragonStepTarget`; arrives in a compact palm. **No i-frame visuals**: he is hittable. |
| Saint Dragon Battle Aura | Slow exhale, chin lifted slightly, the coat lifts in a gentle updraft. Pale gold/silver pressure ripples at a 500 cm radius. |
| Awakening: Dragon God (1.5 s) | A single still moment: the eyes' gold rim intensifies, the aura condenses to the body, then control returns. No screaming, no flexing. |
| Hit reactions | Minimal flinch on light hits; with 180 poise, staggers are rare and short. When staggered, it should read as surprise rather than pain. |
| Dodge (380 cm) | A short sidestep, not a roll. He never rolls on the ground. |

## 7. Technical budget

- LOD0 about 60k triangles (body 25k, coat 15k, hair cards 12k, fur 8k). 4 LODs.
- Chaos Cloth on the four coat panels and the collar edge. Hem colliders on the thighs and calves; keep the coat from clipping during Dragon Step.
- Textures: 4K body/coat, 2K head/hair. Use a Substrate-compatible master material.
- Facial blendshapes for neutral, speaking and slight frown. There is no big-emotion set; he doesn't need one.
