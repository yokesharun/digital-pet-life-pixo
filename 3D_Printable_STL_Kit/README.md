PIXO V1 | USB-ONLY 3D PRINTABLE ENCLOSURE
Prototype design v1.0 - November 2026
All dimensions in millimetres. This is a FIT-TESTABLE PROTOTYPE, not a dimensionally certified production enclosure.

1. PROJECT SCOPE
----------------
INCLUDED:
- ESP32 CP2102 / ESP32-WROOM-style 30-pin USB-C development board
- 0.96-inch 4-pin I2C OLED board
- TTP223 touch sensor (under forehead, behind solid printed plastic)
- 2 loose-wired 6x6-style momentary tactile pushbuttons
- Bare 3.3V small 12mm piezo buzzer
- 2 captive separate button caps + removable button retaining bridge
- Front tray / shell and screw-secured removable rear cover

EXCLUDED: battery, battery charger, boost converter, constant-voltage module, power switch.
Power: ESP32 USB-C ONLY. USB port exits at the TOP so Pixo can stand upright on a flat desk while plugged in.

2. DELIVERABLES
---------------
pixo_v1_front_shell.stl            - primary shell with face, walls, display window, touch membrane
pixo_v1_rear_cover.stl             - matching rear cover, ESP32 mounts and buzzer cradle
pixo_v1_button_A_cap.stl           - captive push cap; one top dot; print blue if desired
pixo_v1_button_B_cap.stl           - captive push cap; two top dots; print green if desired
pixo_v1_button_retainer.stl        - REQUIRED extra part to sandwich and locate loose tactile switches
pixo_v1_assembly.step             - multi-part editable B-Rep STEP solids (not a parametric feature history)
pixo_v1_clearance_reference.step   - reference-only geometry for electronic component envelopes
pixo_v1_fit_test_parts.stl         - small samples for screw bosses, cap opening, USB clearance, ESP hole pitch
pixo_v1_parametric.py             - parameter-driven CadQuery CAD SOURCE for revising the design
pixo_v1_design_preview.png        - visual preview; the cartoon OLED eyes are illustrative only
pixo_v1_assembly_notes.txt        - these instructions

Do not 3D-print the assembly STEP. Print each individual STL. Imported STEP solids are editable in a CAD program by direct solid editing; the .py file preserves the original model parameters.

3. EXTERNAL DIMENSIONS / PLACEMENT
----------------------------------
Final closed size: WIDTH 58.0 x HEIGHT 108.0 x DEPTH 31.0 mm.
Rounded rectangle corner radius: approximately 9.5 mm.
Front skin thickness: 2.4 mm, except 1.0 mm at the capacitive-touch membrane.
Main body walls: 2.4 mm.
Front shell (without lid) 28.8 mm deep; rear cover face 2.2 mm thick.
Front left A cap blue; front right B cap green in preview (STL carries no colour).
Button centre spacing: 26.4 mm.
Visible round cap OD: 10.6 mm; nominal housing hole OD: 11.15 mm.
Flange behind cover: approx. 14.0 x 13.7 mm, so caps CANNOT fall out through holes.
Nominal button outside protrusion: ~1.65 mm; actual switch travel must be measured.
USB cutout: 16.0 mm wide x 10.6 mm high, at TOP case edge, approx. 3 mm port setback.
OLED front window: 24.7 x 15.0 mm (approx. 1.8 mm corner radii); size requires screen-glass fit test.
Touch point: thin printed, closed forehead pad behind a ~1mm membrane (no hole).
Buzzer: REAR lower-right internal seat and 5 rear outlet holes; no additional front opening.
Rear case fasteners: 4 x M2.5 x 8 mm countersunk screws, directly into printed plastic.

4. FASTENERS: PRINTED PLASTIC IS THE THREAD MATERIAL
----------------------------------------------------
Rear lid: 4 x M2.5 x 8 mm countersunk flat-head MACHINE screws, approx. 5.15 mm max head seat.
Pilot bore diameter is nominal 2.05 mm in bosses approximately 7.1 mm outside diameter.
The STL does NOT contain a fine helical female thread. The *printed PLA* takes the thread
using a screw carefully or preferably an M2.5 x 0.45 hand tap. No metal inserts or nuts.

VERY IMPORTANT: First print the separate test bosses with 1.95, 2.05 and 2.15 mm pilot
holes. Check the actual screw/tap, splitting, torque and fit. Use the sample that works
best on YOUR Bambu A1 mini and chosen PLA. The 2.05 mm bore is a starting point,
not a guarantee of the finished printed diameter. Use an M2.5 tap for frequent reopenings.
DO NOT force a machine screw into an undersized boss; PLA can crack. Stop at first resistance.

Internal ESP mounting: proposed 4 x M2 x 6 mm screws (tested pilot 1.68 mm),
with 4 PCB mounting pads integral to rear cover.
OLED mounting: proposed 4 x M2 x 4 mm screws (pilot 1.55 mm), into front shell pillars.
Button retainer mounting: proposed 2 x M2 x 5 mm screws (pilot 1.56 mm).
All internal screw lengths are starting assumptions; check actual PCB thickness and
screw head clearance before assembly. Use a shallow torque setting or screwdriver.

5. COMPONENT FIT ASSUMPTIONS (MEASURE AGAIN)
-------------------------------------------
ESP32:
  Body envelope taken as PCB 53 x 29 x approx. 2 mm.
  The measurement form gave the WIDTH as '2.9 mm', almost certainly meaning 2.9 cm / 29mm.
  Mounting hole positions guessed from ruler photos as 23.4 mm across and 47.6 mm along.
  USB-C socket on TOP edge with connector opening in shell top; no access for BOOT/RST.
  A photo of the back confirms the board carries a PCB antenna despite 'no antenna' in form.
  Avoid screws/metallized decoration near the black antenna end of the board.
  For accurate fit, remeasure PCB width and all hole-center spacing with calipers.
OLED:
  PCB clearance/visual envelope assumed 27 x 27 mm; 1.6mm actual PCB thickness used.
  Submitted values disagree between PCB width/height and glass dimensions.
  4 M2 posts currently follow approximate form centres (X=-10.5/+9.5, Y=-11.5/+11.5).
  Verify mechanical hole locations, whole display glass size AND exact visible area.
  Do NOT push on the OLED glass; remove screws if board bends or glass contacts rim.
TOUCH SENSOR:
  PCB expected around 11 x 10 mm. Printed forehead pocket is ~13.6 x 13.4 mm.
  There are side guide rails and a 1mm solid front wall for touch-through-plastic.
  Sensor can be supported at the edges with thin removable double-sided tape.
  Do not use thick foam directly on the touch electrode; it may reduce sensitivity.
PUSHBUTTONS:
  8 x 6 mm switch body envelope used, nominal 5mm switch overall front-to-back.
  The measurement form did NOT include actual plunger rest height/travel.
  Button retainer front is at z=8.6mm from face; switch seat is lightly relieved.
  Fit the cap, switch and retainer before soldering. Adjust using shims or CAD if cap
  preloads the switch, fails to click, sticks or has excessive free travel.
BUZZER:
  Ø12 x 10mm body is from the form, NOT verified from photos; no dedicated buzzer photo.
  Holder inner diameter ~12.6mm; buzzer sound hole faces outward toward rear grille.
  Use a removable thin silicone ring or friction strap if loose. Ensure wires are not
  compressed between buzzer and board. Driver transistor (if required) is NOT mounted.

6. PRINT SETTINGS: BAMBU A1 MINI, PLA, 0.4 MM NOZZLE
---------------------------------------------------
Layer height: 0.20 mm (0.16 mm if fine front lettering is desired).
Perimeters: 3-4 walls; top/bottom: 4-5 layers; infill 15-20% gyroid.
Supports: try none initially; localized build-plate supports only if bridge issues occur.
Front shell: print with FRONT (screen/button face) on build plate, interior open upward.
Rear cover: print with OUTER rear face on bed, all internal mounts upward.
Buttons: print visible knob face on bed, flange toward printer top. Use brim if needed.
Button retainer: print flat (its broad face on bed).
Fit-test STL: includes 6 DISCONNECTED calibration bodies by design; in Bambu Studio
  split into objects if desired. Use no scaling and keep model size in mm.
Color suggestions: charcoal/dark grey case; blue A and green B caps.

If supports are generated inside the OLED opening or around pilot holes, remove them
completely before assembly. Never force electronics into the plastic.

7. ASSEMBLY ORDER
----------------
A. Print fit-test piece FIRST; test M2.5 screw pilots, button hole/cap clearance,
   23.4 mm ESP mounting-hole sample, USB plug clearance.
B. Print the front shell, lid, caps and switch-retainer. Clean all holes and cutouts.
C. Insert button caps from INSIDE front shell so flange seats behind face.
D. Place both tactile switches over the caps (actuators toward cap centre); arrange
   their legs/wires into retainer side reliefs. Fit retainer bridge from inside and
   secure with 2 M2x5 screws into front shell posts. Verify both click and rebound.
E. Mount OLED to front shell posts with four M2x4 screws only after checking hole
   spacing and glass/face clearance. Small nylon/printed washers may be needed.
F. Position touch sensor behind forehead, touch side toward thin membrane. Retain
   at the edges with removable thin tape, avoiding the sensing circle.
G. Seat buzzer into ring on INSIDE of rear cover, sound hole towards rear grille.
H. Attach ESP32 board to four rear-cover posts using appropriate M2 screws,
   USB-C pointing toward the TOP cutout of the main shell.
I. Wire OLED/touch/buttons/buzzer with flexible insulated leads, leaving enough
   slack to open the rear cover without ripping wires. Verify GND is common.
J. Close lid using alignment lip, ensure nothing catches, then install FOUR M2.5x8
   countersunk screws gently and in alternating order. Confirm no screw reaches PCB.
K. Connect USB-C at top, check standing stability and function.

8. IMPORTANT SAFETY / KNOWN LIMITATIONS
--------------------------------------
- This is NOT verified against the actual hardware by a physical print; these files
  are a first-fit design based on supplied dimensions/photos and documented assumptions.
- Two especially conflicting dimensions: ESP32 width listed as 2.9mm, and OLED glass
  dimensions/visible area. Confirm them by caliper before printing the full set.
- Actual captive button travel is NOT verified. The loose-wired 6x6 button model
  varies by manufacturer. Try the retainer and cap fit first before soldering.
- No battery and no charger in this version. USB-C is for power/programming only.
- The case has no guaranteed window to the ESP32's status LED, because LED position
  and light-pipe path were not unambiguously established.
- Never over-tighten screws in PLA. Repeated assembly can damage plastic threads.
- KEEP the ESP32 Wi-Fi antenna area clear of metal and unnecessary wires.
- The printed touch membrane works only if the sensor is adjusted for this thickness.
- STL files export only solids; coloured button/rendered OLED illustration not encoded.
- All supplied files use millimetre dimensions. Do NOT rescale them in the slicer.

9. QUICK PARAMETERS FOR FUTURE REVISION
--------------------------------------
Outer width     : BODY_W = 58.0
Outer height    : BODY_H = 108.0
Overall depth   : SHELL_DEPTH + COVER_T = 31.0
ESP PCB envelope: ESP_BOARD_WH = (29.0, 53.0)
ESP centre      : (0, +22.6) ; USB TOP
OLED centre     : (0, +15.5)
Touch centre    : (0, +40.2)
Button positions: X=+13.2 (A left as seen from front) and X=-13.2 (B right), Y=-20.2
Buzzer centre   : (15.2, -34.6) (located on rear cover)
Rear screw pitch: X=+/-22.0; Y=+/-45.0
Rear screw pilot: LID_PILOT_D = 2.05
The accompanying pixo_v1_parametric.py contains the whole editable CadQuery model.
