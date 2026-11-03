"""Pixo V1 USB-only prototype enclosure, CadQuery 2.x.
Units: mm. All unverified component dimensions are assumptions to fit-test.

Coordinate system: XY = front face, Y+ = forehead/top, Z+ = interior/rear.
The flat exterior front face is z=0; rear cover exterior is z=31.
"""
import cadquery as cq
import math
import os

DIR = os.path.dirname(os.path.abspath(__file__))
# --- Change this section after measuring actual parts ---
BODY_W = 58.0
BODY_H = 108.0
BODY_R = 9.5
FRONT_WALL = 2.4
WALL = 2.4
SHELL_DEPTH = 28.8
COVER_T = 2.2
LID_LIP_T = 1.35
LID_LIP_H = 1.5
REAR_Z = SHELL_DEPTH + COVER_T

LID_HOLES = [(-22.0, -45.0), (+22.0, -45.0), (-22.0, +45.0), (+22.0, +45.0)]
LID_BOSS_OD = 7.1
LID_PILOT_D = 2.05  # M2.5 x 8 threads form directly in PLA; MUST test
LID_PILOT_DEPTH = 9.6
LID_CLEAR_D = 2.95
LID_HEAD_D = 5.15

OLED_CENTRE = (0.0, +15.5)
OLED_BOARD_WH = (27.0, 27.0)  # generous cut-free clearance; photo/form disagreement
OLED_WINDOW_WH = (24.7, 15.0)
OLED_WINDOW_Y = +16.8
OLED_HOLES_X = (-10.5, 9.5)   # approximate from submitted hole centres
OLED_HOLES_Y = (-11.5, 11.5)
OLED_POST_TOP = 4.8
OLED_PILOT_D = 1.55  # M2 x 4/5, test

TOUCH_CENTER = (0, 40.2)
TOUCH_POCKET_WH = (13.6, 13.4)
TOUCH_MEMBRANE = 1.0

BUTTON_CENTRES = [(+13.2, -20.2), (-13.2, -20.2)]  # front view left A, right B (camera is at -Z)
BUTTON_APERTURE_D = 11.15
CAP_KNOB_D = 10.60
CAP_FLANGE_WH = (14.0, 13.7)
RETAINER_FRONT_Z = 8.6
RETAINER_THICK = 2.4
RETAINER_HOLES = [(-22.0, -9.7), (+22.0, -9.7)]

ESP_CENTRE = (0.0, +22.6)  # USB-C TOP edge, can stand on desk with power attached
ESP_BOARD_WH = (29.0, 53.0)
ESP_HOLES_X = (-11.7, 11.7)  # measured visually, approximate. Verify!
ESP_HOLES_Y = (-23.8, 23.8)
ESP_PCB_REAR_Z = 23.6
BUZZER_CENTRE = (15.2, -34.6)
BUZZER_D = 12.0


def rb(w,h,depth,r,z0=0,x=0,y=0):
    """XY rounded rectangular extrusion with true vertical circular fillets."""
    result = cq.Workplane('XY').box(w,h,depth,centered=(True,True,False))
    if r > .01:
        result = result.edges('|Z').fillet(r)
    return result.translate((x,y,z0))


def circle(r, h, x, y, z):
    return cq.Workplane('XY').center(x,y).circle(r).extrude(h).translate((0,0,z))


def rect(w,h,d,x,y,z):
    return cq.Workplane('XY').box(w,h,d,centered=(True,True,False)).translate((x,y,z))


def cone(r1,r2,h,x,y,z):
    return cq.Workplane('XY').add(cq.Solid.makeCone(r1,r2,h,cq.Vector(x,y,z), cq.Vector(0,0,1)))


def ring(w,h,wi,hi,r,ri,z,d):
    return rb(w,h,d,r,z).cut(rb(wi,hi,d+.4,ri,z-.2))

# --------- FRONT SHELL ----------
outer = rb(BODY_W, BODY_H, SHELL_DEPTH, BODY_R)
outer = outer.edges("<Z").fillet(0.9)  # softer edge around the visible front face
inside = rb(BODY_W-2*WALL, BODY_H-2*WALL, SHELL_DEPTH-FRONT_WALL+0.5,
            BODY_R-WALL, FRONT_WALL)
front = outer.cut(inside)
# Bezel through front face; lit field larger than nominal active area, will require glass check.
front = front.cut(rb(OLED_WINDOW_WH[0],OLED_WINDOW_WH[1], FRONT_WALL+.6,
                    1.8,-.25,0,OLED_WINDOW_Y))
# display glass perimeter relief inside front face, leaving minimum ~1.2mm plastic rim
front = front.cut(rb(27.1,19.4,1.20,1.2,FRONT_WALL-1.16,0,OLED_WINDOW_Y))
# Chamfered/display lip details: create circular corner accents not on front face

# OLED support posts and blind pilot holes (M2x4 or M2x5 after test).
for ox in OLED_HOLES_X:
    for oy in OLED_HOLES_Y:
        px,py=OLED_CENTRE[0]+ox,OLED_CENTRE[1]+oy
        front = front.union(circle(2.65, OLED_POST_TOP-FRONT_WALL, px,py,FRONT_WALL))
        front = front.cut(circle(OLED_PILOT_D/2, 3.7, px,py,1.9))

# Capacitive touch 1.0mm thin membrane pocket behind forehead.
front = front.cut(rb(TOUCH_POCKET_WH[0],TOUCH_POCKET_WH[1],FRONT_WALL-TOUCH_MEMBRANE+.1,
                    1.0,TOUCH_MEMBRANE,TOUCH_CENTER[0],TOUCH_CENTER[1]))
# Touch PCB side guide rails; small pocket accepts approx 11x10 board with soldered wires.
# Long rails outside 13.6mm pocket, x +/-7.65, connected to front skin.
for sx in (-1,1):
    front = front.union(rect(1.25,12.6,3.9, TOUCH_CENTER[0]+sx*8.25,TOUCH_CENTER[1],FRONT_WALL))
# top end stops, leaving cable exit at bottom of touch pocket
for sx in (-1,1):
    front = front.union(rect(3.2,1.5,3.0,TOUCH_CENTER[0]+sx*5.7,TOUCH_CENTER[1]+7.3,FRONT_WALL))

# front-face thin embossed touch ring in recess; no through-hole, preserved sealed plastic.
touch_ring = circle(5.5,0.29,*TOUCH_CENTER,-.01).cut(circle(5.08,.50,*TOUCH_CENTER,-.10))
front=front.cut(touch_ring)
# subtle 'PIXO' text recessed 0.30 mm at the forehead above touch
try:
    label=cq.Workplane('XY').text('PIXO',4.4,.42,font='DejaVu Sans',kind='bold',
                                  halign='center',valign='center',combine=True).mirror('YZ',union=False).translate((0,49.4,-.02))
    front=front.cut(label)
except Exception as exc:
    print('Text font not installed; skip label',exc)

# Rounded cap apertures in the front.
for bx,by in BUTTON_CENTRES:
    front = front.cut(circle(BUTTON_APERTURE_D/2,FRONT_WALL+.8,bx,by,-.3))

# Internal mounts for removable retainer plate, deliberately above cap-flange envelope.
for rx,ry in RETAINER_HOLES:
    front=front.union(circle(2.85,RETAINER_FRONT_Z-FRONT_WALL,rx,ry,FRONT_WALL))
    front=front.cut(circle(.78, 5.2,rx,ry,RETAINER_FRONT_Z-4.8))

# Four deep rear-lid screw pillars anchored to the front face and blind piloted.
for lx,ly in LID_HOLES:
    front=front.union(circle(LID_BOSS_OD/2, SHELL_DEPTH-FRONT_WALL,lx,ly,FRONT_WALL))
    front=front.cut(circle(LID_PILOT_D/2,LID_PILOT_DEPTH+.1,lx,ly,
                            SHELL_DEPTH-LID_PILOT_DEPTH))

# Side ribs tying the tall lid bosses against the housing walls. No support required.
for lx,ly in LID_HOLES:
    sx=1 if lx>0 else -1
    front=front.union(rect(2.2,3.3,SHELL_DEPTH-FRONT_WALL,
                           lx+sx*2.8,ly,FRONT_WALL))

# USB-C access, centred TOP of shell: keeps bottom flat for an upright desk pet.
# Actual metal port ~9x4, plug body 12x6; 16 x 10.6mm entry allows plug clearance.
front=front.cut(rect(16.0,8.0,10.6,0,+52.0,15.8))
# Small key notch/lip for cover alignment is implemented on rear cover.

# -------- BUTTON CAPS (printed separately, insert from behind) ------
def make_cap(index):
    knob = circle(CAP_KNOB_D/2,4.35,0,0,-1.65)  # -1.65..2.70
    try:
        knob=knob.edges('<Z').fillet(.48)
    except Exception:
        pass
    flange = rb(CAP_FLANGE_WH[0],CAP_FLANGE_WH[1],.90,2.15,2.46)
    stem = circle(2.05,.27,0,0,3.36)
    cap=knob.union(flange).union(stem)
    # tactile markers: one or two shallow round recesses on the visible button face
    markers = [(0.,0.)] if index==0 else [(-1.25,0.),(1.25,0.)]
    for mx,my in markers:
        cap=cap.cut(circle(.58,.28,mx,my,-1.70))
    return cap
cap_a=make_cap(0)
cap_b=make_cap(1)

# -------- BUTTON RETAINER (needed for loose 6x6 tactile buttons) ------
# Screw-fastened replaceable bridge that holds both switches just behind captive caps.
retainer=rb(44.0,14.4,RETAINER_THICK,2.3,RETAINER_FRONT_Z,0,-20.2)
for rx,ry in RETAINER_HOLES:
    # eared mounting tabs integral to bridge
    ear=rb(7.7,14.0,RETAINER_THICK,2.0,RETAINER_FRONT_Z,rx,-14.2)
    retainer=retainer.union(ear)
    # clearance holes for two M2x5 screws
    retainer=retainer.cut(circle(2.25/2,RETAINER_THICK+.3,rx,ry,RETAINER_FRONT_Z-.1))
for bx,by in BUTTON_CENTRES:
    # shallow seating cavity to centre 8x6mm switch body without requiring soldered PCB
    retainer=retainer.cut(rb(8.55,6.55,.55,.7,RETAINER_FRONT_Z-.05,bx,by))
    # Channels for four legs and wires along left/right edges of switch, two per side
    for sx in [-1,1]:
        for sy in [-1,1]:
            retainer=retainer.cut(rect(2.3,1.6,RETAINER_THICK+.3,bx+sx*5.15,
                                       by+sy*2.2,RETAINER_FRONT_Z-.1))

# -------- REAR COVER ----------
cover=rb(BODY_W-.32,BODY_H-.32,COVER_T,BODY_R-.16,SHELL_DEPTH)
cover=cover.edges(">Z").fillet(0.6)
# insert lip inside front cavity wall by >=.5 mm, clean corner relief at bosses
lip_outer_w=BODY_W-2*(WALL+.48)
lip_outer_h=BODY_H-2*(WALL+.48)
lip=ring(lip_outer_w,lip_outer_h,lip_outer_w-2*1.25,lip_outer_h-2*1.25,
         BODY_R-WALL-.48,BODY_R-WALL-.48-1.25,SHELL_DEPTH-LID_LIP_H,LID_LIP_H+.20)
for lx,ly in LID_HOLES:
    lip=lip.cut(circle(4.5,LID_LIP_H+.6,lx,ly,SHELL_DEPTH-LID_LIP_H-.2))
cover=cover.union(lip)
# Lid clearances with shallow 90-degree countersink for flathead M2.5 screws.
for lx,ly in LID_HOLES:
    cover=cover.cut(circle(LID_CLEAR_D/2,COVER_T+.6,lx,ly,SHELL_DEPTH-.25))
    cover=cover.cut(cone(LID_CLEAR_D/2,LID_HEAD_D/2,1.25,lx,ly,REAR_Z-1.25))

# ESP dev board standoffs: M2 x6 PCB screws, on rear cover interior surface
# board plane back is z=23.6; header solder pins have ~5.2mm rear clearance.
esp_screw_xy=[]
for hx in ESP_HOLES_X:
    for hy in ESP_HOLES_Y:
        px,py=ESP_CENTRE[0]+hx,ESP_CENTRE[1]+hy
        esp_screw_xy.append((px,py))
        cover=cover.union(circle(3.00,SHELL_DEPTH-ESP_PCB_REAR_Z,px,py,ESP_PCB_REAR_Z))
        cover=cover.cut(circle(.84,5.0,px,py,ESP_PCB_REAR_Z-.1))
# Separate tiny perimeter guide rails for ESP (upper centre and side), avoid antenna region.
# USB cable is inserted at the top edge, no blocking features over the port.
for sx in (-1,1):
    cover=cover.union(rect(1.2,14.0,3.7,sx*16.35,+31.0,SHELL_DEPTH-3.7))

# Buzzer cage on the inside of rear cover, cylinder sound face toward rear grille.
# Accommodates up to 12mm Ø, 10mm high buzzer and provides removable press/silicone fit.
bx,by=BUZZER_CENTRE
buzzer_outer=circle(7.1,8.5,bx,by,SHELL_DEPTH-8.5)
buzzer_core=circle(6.3,9.2,bx,by,SHELL_DEPTH-9.0)
buzzer_cage=buzzer_outer.cut(buzzer_core)
# Relief slot for two wires along lower edge of the buzzer holder
buzzer_cage=buzzer_cage.cut(rect(5.2,4.8,9.4,bx,by-6.25,SHELL_DEPTH-9.1))
cover=cover.union(buzzer_cage)
# Rear sound openings to the exposed face of buzzer: five Ø1.8 holes in cross pattern.
for dx,dy in [(0,0),(-3.0,0),(3.0,0),(0,-3.0),(0,3.0)]:
    cover=cover.cut(circle(.88,COVER_T+.6,bx+dx,by+dy,SHELL_DEPTH-.25))

# Extra minuscule alignment mark on rear cover indicating top (three dot divots).
for dx in [-1.6,0,1.6]:
    cover=cover.cut(circle(.38,.25,dx,49.6,REAR_Z-.18))

# ---------- FIT TEST STRIP ----------
# Multi-body calibration sheet; individual tests are separated with 4mm gaps for easy slicing.
# 3 screw pilot samples: M2.5x8 and M2x5, 0.10mm spread.
fit = cq.Workplane('XY')
for i, dia in enumerate([1.95,2.05,2.15]):
    fx=-29+i*15
    sample=rect(12.5,12.5,3.0,fx,0,0).union(circle(3.55,8.5,fx,0,3))
    sample=sample.cut(circle(dia/2,8.65,fx,0,3))
    fit=fit.union(sample)
# test cap aperture: 16mm square plate, proper front thickness and aperture
fit=fit.union(rect(17.5,17.5,2.4,24,0,0).cut(circle(BUTTON_APERTURE_D/2,2.8,24,0,-.2)))
# miniature ESP 2-hole pitch gauge with slotted clearance markings
pitch=23.4
jig=rb(32.0,9.0,2.2,1.5,0,0,26.0)
for hx in [-pitch/2,pitch/2]:
    jig=jig.cut(circle(2.8/2,2.6,hx,26,-.2))
fit=fit.union(jig)
# usb cable-port gauge (16x10 rectangular notch)
port=rect(23,10,8.0,34,26,0).cut(rect(16.0,8.0,9,34,26,-.5))
fit=fit.union(port)

# --------- export ---------
PRINTED = {
 'pixo_v1_front_shell.stl':front,
 'pixo_v1_rear_cover.stl':cover,
 'pixo_v1_button_A_cap.stl':cap_a,
 'pixo_v1_button_B_cap.stl':cap_b,
 'pixo_v1_button_retainer.stl':retainer,
 'pixo_v1_fit_test_parts.stl':fit,
}
for fn,shape in PRINTED.items():
    path=os.path.join(DIR,fn)
    cq.exporters.export(shape,path,exportType='STL',tolerance=.08,angularTolerance=.12)
    print('EXPORTED', fn,os.path.getsize(path),'bytes',end='  ')
    try:
        bb=shape.val().BoundingBox()
        print('bounds {:.1f}x{:.1f}x{:.1f} volume {:.0f}'.format(bb.xlen,bb.ylen,bb.zlen,shape.val().Volume()))
    except Exception as ex: print(ex)

assembly=cq.Assembly(name='PIXO_V1_USB_ONLY')
assembly.add(front,name='01_Front_shell',color=cq.Color(0.20,0.24,0.29))
assembly.add(cover,name='02_Rear_cover',color=cq.Color(0.23,0.28,0.34))
for index,(cx,cy) in enumerate(BUTTON_CENTRES):
    part=cap_a if index==0 else cap_b
    assembly.add(part.translate((cx,cy,0)),name=f'0{index+3}_Button_{"A" if index==0 else "B"}',
                 color=cq.Color(0.25,0.60,0.95) if index==0 else cq.Color(0.48,0.82,0.45))
assembly.add(retainer,name='05_Button_retainer',color=cq.Color(0.64,0.68,0.72))
assembly.save(os.path.join(DIR,'pixo_v1_assembly.step'),exportType='STEP',mode='default')

# A second STEP with simple component placeholders helps inspect clearances in 3D.
component_assembly=cq.Assembly(name='PIXO_V1_WITH_COMPONENT_ENVELOPES')
component_assembly.add(front,name='Front_shell',color=cq.Color(0.21,0.25,0.28))
component_assembly.add(cover,name='Rear_cover',color=cq.Color(0.29,0.35,0.38))
for index,(cx,cy) in enumerate(BUTTON_CENTRES):
    component_assembly.add((cap_a if index==0 else cap_b).translate((cx,cy,0)),
                           name=f'Cap_{index+1}',color=cq.Color(0.2,0.5,0.9) if index==0 else cq.Color(0.4,0.8,0.4))
component_assembly.add(retainer,name='Button_retainer',color=cq.Color(0.75,0.75,0.75))
# physical reference solids (NOT for printing):
ref_esp=rb(ESP_BOARD_WH[0],ESP_BOARD_WH[1],2.0,1.3,ESP_PCB_REAR_Z-2.0,*ESP_CENTRE)
component_assembly.add(ref_esp,name='REFERENCE_ESP32_PCB_29x53',color=cq.Color(.05,.45,.28))
ref_oled=rb(27,27,1.6,1.0,OLED_POST_TOP,*OLED_CENTRE)
component_assembly.add(ref_oled,name='REFERENCE_OLED_PCB_27x27',color=cq.Color(.1,.25,.75))
ref_touch=rb(11,10,1.0,.6,TOUCH_MEMBRANE,*TOUCH_CENTER)
component_assembly.add(ref_touch,name='REFERENCE_TOUCH_PCB_11x10',color=cq.Color(.7,.12,.15))
for ix,(cx,cy) in enumerate(BUTTON_CENTRES):
    ref_switch=rb(8,6,3.35,.4,5.1,cx,cy)
    component_assembly.add(ref_switch,name=f'REFERENCE_TACTILE_SWITCH_{ix+1}',color=cq.Color(.32,.34,.36))
component_assembly.add(circle(BUZZER_D/2,10,bx,by,SHELL_DEPTH-10),name='REFERENCE_PIEZO_12x10',color=cq.Color(.12,.12,.12))
component_assembly.save(os.path.join(DIR,'pixo_v1_clearance_reference.step'),exportType='STEP',mode='default')
print('DONE')
