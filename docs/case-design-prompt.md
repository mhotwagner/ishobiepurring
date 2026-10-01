Design a 3D-printable enclosure for a small sensor device, starting from your 3D model template. I need two printable parts, a base and a lid, that I can export as STL or 3MF.

## What it is and where it lives

The device detects when my cat is purring. It sits inside his bed, under the cushion he lies on, and senses purr vibration (20–50 Hz) with an accelerometer. A 5 kg cat lies on top of it and I sometimes rest my head on him, so the enclosure gets pressed from above and has to be comfortable to lie on through a cushion. It is powered by a USB-C cable that stays plugged in.

Three things follow from that, and they matter more than anything else below:

- **Thin and smooth.** Aim for 14–16 mm total height and a footprint around 90 × 50 mm (a squarer footprint is fine if it packs better). Round every outer edge and corner generously. Nothing protrudes.
- **Nothing can rattle.** A loose part buzzes at the same frequencies as a purr and ruins the measurement. The lid is screwed down, every board is screwed or clamped, and wires are held in channels.
- **The lid carries load.** Posts or ribs take the weight from lid to floor so no force lands on a circuit board.

## Parts inside

These dimensions are nominal. I will measure each board with calipers before printing, so keep every one as a named parameter in one place.

| Part | PCB size (mm) | Mounting holes | Notes |
|---|---|---|---|
| ESP32 D1 mini, USB-C, no pin headers | 39.0 × 31.4 × 1.6, components up to 3.5 above the PCB | none | details below |
| MPU-6050 accelerometer (GY-521 board) | 21 × 16 × 1.6 | two Ø3, about 15 apart, along one long edge | wire pads along the other long edge |
| BMP388 temperature/pressure (Adafruit breakout) | 25.4 × 17.8 × 1.6 | four Ø2.5 on a 20.3 × 12.7 pattern | |
| SPH0645 I2S microphone (Adafruit breakout) | 16.7 × 12.7 × 1.8 | two Ø2.5 at one end | sound hole is on the underside, the face with no components |
| Green LED | 5 mm round | press fit | |

Wires are thin silicone hookup wire soldered straight to the boards, with no connectors. Leave about 2.5 mm under each board for the solder joints.

ESP32 board details, viewed from above with the USB port toward you:

- The USB-C socket is centred on the near short edge. Its metal shell is 9.0 × 3.2 mm and overhangs the board edge by about 2 mm.
- The Wi-Fi antenna is at the far short edge.
- There is one reset button: a side-push switch sitting in a notch at the near-left corner. Its plunger points out to the left. The tip is about 1.5 mm inboard of the board's left edge and about 2.5 mm from the USB edge (estimated from a photo).

## Requirements for each part

**ESP32**
- USB end sits against an end wall. A USB-C plug has to seat fully: either bring the socket mouth flush with the outer surface, or relieve the wall around it for a plug body up to 12.5 × 7 mm.
- The board has no mounting holes. Hold it in a cradle that supports its edges, give it a hard stop behind so plugging in the cable cannot push it inward, and clamp it from above with posts on the lid.
- The reset button is reachable from outside without opening the case, through a printed flexing tab in the side wall or a 2 mm pin hole lined up with the plunger. Keep it flush or recessed so bedding pressing on the case cannot trigger it.
- Keep screws and wire bundles at least 10 mm away from the antenna end.

**MPU-6050** (the primary sensor)
- Screwed flat to bosses on the floor, near the centre of the footprint, board parallel to the floor.
- Rigid coupling to the case is the whole point. No foam, no flexible mounts.

**BMP388**
- It measures the warmth of the cat above, to tell whether he is in the bed. The ESP32 runs warm and would swamp that reading.
- Put it at the far end from the ESP32, behind a partition wall with a slot for four wires.
- Raise it on posts so the sensor sits about 1 mm below a patch of lid thinned to roughly 0.8 mm.
- Add small vent slits in the side wall of that compartment so it sees room air pressure. No vents in the lid; the cushion covers it.

**Microphone**
- The sound hole needs a sealed path to outside air through a side wall, because the lid and floor are both covered by bedding. Put it at the far end from the USB port.
- To stay thin, I would lay the board flat with the sound hole down, on a plinth with a small groove (about 2 × 1.5 mm) running to a 2 mm opening in the side wall, so the board itself closes the top of the groove. Standing the board upright against the wall also works but costs height. Use whichever keeps the case thinner.

**LED**
- A 5.0 mm hole in the USB end wall, beside the port and clear of the reset button. That end faces out of the bedding because the cable leaves there.

**Wiring**
- A channel along one long side with clips or comb slots to hold the wires down.
- Runs: four wires from the ESP32 to the MPU-6050 and on to the BMP388, five wires to the microphone, two to the LED.

**Cable strain relief** (only if it adds no height and no protrusion)
- A pair of slots beside the USB opening for a small zip tie, so a tug on the cable is taken by the case and not the connector.

## Printing and assembly

- FDM, 0.4 mm nozzle, 0.2 mm layers, PETG.
- No supports. The base prints floor-down and the lid prints outer-face-down.
- Walls, floor and lid 2 mm thick, apart from the thinned patch over the BMP388.
- The lid has a lip that locates it on the base, and fastens with four M3 countersunk screws threading directly into printed bosses. Heads sit flush.
- Boards fasten with M2.5 screws into printed bosses.
- 0.3 mm clearance around each board.

## What I need back

- Base and lid as separate bodies.
- A simple placeholder block for each board, in position, so I can check the fit. A cutaway or exploded view if you can.
- The parameter list in one place.
- A short list of the assumptions you made and any requirement you could not meet.
