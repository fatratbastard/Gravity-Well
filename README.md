# Gravity-Well
Graviton's cousin. WebAssembly based application. 

## Goal ##
Destroy every red bunker on the planet to clear it. The bunker count in the top bar tells you how many are left. Clearing a planet lifts you off to the next one, where gravity is stronger and the bunkers are tougher.

The ground is fatal on contact, at any speed, and so is flying into a bunker. There is no landing. The planet wraps around, so flying far enough in one direction brings you back to the start.

## Controls ##

| Action | Keyboard | Touch |
| --- | --- | --- |
| Rotate left / right | Left / Right arrows, or A / D | Arrow pads, bottom left |
| Thrust | Up arrow or W | Thrust pad |
| Fire | Space or J | Fire pad |
| Shield and tractor beam | Down arrow, S, Shift or K | Beam pad |
| Start or restart | Space or Up | Tap the screen |

Hold Fire to shoot continuously. Up to 8 of your shots can be in the air at once. Sound plays after your first key press or tap; the button in the top right corner mutes it.

## Fuel and shield ##
You start with 10,000 fuel. Thrust burns 55 fuel per second and the shield burns 40 per second, so a full tank gives about three minutes of thrust. Rotating and firing cost nothing. At zero fuel you can no longer thrust or shield, and gravity takes you down.

The shield blocks bunker shots but not the ground. While it is up, a tractor beam reaches down below your ship. Hover over a yellow fuel tank, no more than about seven ship-lengths above it and roughly centered, and hold the shield to pull it in for 2,200 fuel. Shooting a tank destroys it and the fuel is lost.

## Scoring and lives ##

| Event | Points | Fuel |
| --- | --- | --- |
| Destroy a bunker | 250 + 25 per planet number |   |
| Beam up a fuel tank | 150 | +2,200 |
| Shoot a fuel tank | 50 | |
| Clear a planet | 1,000 × planet number | +1,500 |

You start with 3 ships and earn an extra ship every 10,000 points. After a crash you respawn at the start of the same planet with about 2 seconds of invulnerability, shown by a blinking ship. Invulnerability protects you from shots only, not from the ground. Bunkers you destroyed stay destroyed.

## Tips ##
* Stay low beside a bunker. Bunkers only fire when you are above them and within range. Dropping to their level or below, in a valley next to them, keeps you out of their line of fire.
* Change speed once bunkers start leading. From planet 3 on, bunkers aim where you are going, not where you are. Flying in a straight line at a steady speed makes you easy to hit.
* Use short bursts of thrust. Your thrust starts at almost four times the pull of gravity but falls to under one and a half times by planet 10, where gravity stops rising. Big climbs get expensive, so keep altitude rather than recovering it.
• Shield through incoming fire. A quick tap of the shield costs little fuel and blocks every shot that reaches you.
• Collect fuel before you need it. Beam up tanks as you pass them, and save your shots for bunkers.
• Mind the ceiling. You cannot fly off the top of the screen, so climbing out of trouble only goes so far.
