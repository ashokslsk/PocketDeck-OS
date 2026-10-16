"""Original black-and-white pose figures for the Workout tool.

Each pose is a stick figure described by joint angles (degrees). The figure
is drawn with thick limbs (near side solid, far side outlined), scaled to fit
a square sprite, and stood on a ground line. Equipment (kettlebell, dumbbell,
plate, bike, cushion) is drawn at the hands or under the figure.

Angle conventions (screen coordinates, the figure faces +x):
  torso: from straight up, positive leans forward (+x)
  arms and legs: absolute direction from straight down, positive toward +x
"""
import math
from PIL import Image, ImageDraw

TORSO, NECK, HEAD_R = 40, 6, 9
UPPER_ARM, FOREARM = 22, 20
THIGH, SHIN = 27, 27
LIMB_W = 7


def _down(a, length):
    r = math.radians(a)
    return (math.sin(r) * length, math.cos(r) * length)


def _up(a, length):
    r = math.radians(a)
    return (math.sin(r) * length, -math.cos(r) * length)


def _rot(v, deg):
    r = math.radians(deg)
    return (v[0] * math.cos(r) - v[1] * math.sin(r), v[0] * math.sin(r) + v[1] * math.cos(r))


def joints(p):
    """Returns named points for a pose dict (hip at the origin). Front views get
    shoulder and hip widths; side views share one shoulder and hip point."""
    hip = (0.0, 0.0)
    t = p.get("torso", 0)
    dx, dy = _up(t, TORSO)
    neck = (hip[0] + dx, hip[1] + dy)
    hx, hy = _up(t + p.get("head", 0), NECK + HEAD_R)
    head = (neck[0] + hx, neck[1] + hy)
    pts = {"hip": hip, "neck": neck, "head": head}
    front = p.get("front", False)
    for side, sign in (("r", 1), ("l", -1)):
        so = _rot((11 * sign, 4), t) if front else (0.0, 0.0)
        ho = _rot((6 * sign, 0), t) if front else (0.0, 0.0)
        shoulder = (neck[0] + so[0], neck[1] + so[1])
        hipj = (hip[0] + ho[0], hip[1] + ho[1])
        ua, fa = p.get("arm_" + side, (0, 0))
        ex, ey = _down(ua, UPPER_ARM)
        elbow = (shoulder[0] + ex, shoulder[1] + ey)
        hx2, hy2 = _down(fa, FOREARM)
        hand = (elbow[0] + hx2, elbow[1] + hy2)
        th, sh = p.get("leg_" + side, (0, 0))
        kx, ky = _down(th, THIGH)
        knee = (hipj[0] + kx, hipj[1] + ky)
        fx, fy = _down(sh, SHIN)
        foot = (knee[0] + fx, knee[1] + fy)
        pts.update({"shoulder_" + side: shoulder, "hipj_" + side: hipj, "elbow_" + side: elbow, "hand_" + side: hand,
                    "knee_" + side: knee, "foot_" + side: foot})
    return pts


def _limb(d, a, b, near, w=LIMB_W):
    d.line([a, b], fill=0, width=w)
    for q in (a, b):
        d.ellipse([q[0] - w / 2, q[1] - w / 2, q[0] + w / 2, q[1] + w / 2], fill=0)
    if not near:  # far limb: outlined
        d.line([a, b], fill=1, width=max(1, w - 4))


def _kettlebell(d, c, s):
    x, y = c
    d.ellipse([x - 7 * s, y + 2 * s, x + 7 * s, y + 16 * s], fill=0)
    d.arc([x - 5 * s, y - 4 * s, x + 5 * s, y + 6 * s], 180, 360, fill=0, width=max(2, int(2 * s)))


def _dumbbell(d, c, s, vertical=False):
    x, y = c
    if vertical:
        d.line([(x, y - 8 * s), (x, y + 8 * s)], fill=0, width=max(2, int(3 * s)))
        d.rectangle([x - 5 * s, y - 11 * s, x + 5 * s, y - 6 * s], fill=0)
        d.rectangle([x - 5 * s, y + 6 * s, x + 5 * s, y + 11 * s], fill=0)
    else:
        d.line([(x - 8 * s, y), (x + 8 * s, y)], fill=0, width=max(2, int(3 * s)))
        d.rectangle([x - 11 * s, y - 5 * s, x - 6 * s, y + 5 * s], fill=0)
        d.rectangle([x + 6 * s, y - 5 * s, x + 11 * s, y + 5 * s], fill=0)


def _plate(d, c, s):
    x, y = c
    d.ellipse([x - 13 * s, y - 13 * s, x + 13 * s, y + 13 * s], fill=0)
    d.ellipse([x - 3 * s, y - 3 * s, x + 3 * s, y + 3 * s], fill=1)


def _wheel(d, c, s, ground):
    x, _ = c
    r = 11 * s
    d.ellipse([x - r, ground - 2 * r, x + r, ground], outline=0, width=max(3, int(4 * s)))
    d.ellipse([x - 3 * s, ground - r - 3 * s, x + 3 * s, ground - r + 3 * s], fill=0)


def render(pose, size=160, margin=10):
    """Draws a pose; returns a 1-bit PIL image (0 = ink)."""
    pts = joints(pose)
    xs = [v[0] for v in pts.values()]
    ys = [v[1] for v in pts.values()]
    minx, maxx = min(xs) - HEAD_R - 6, max(xs) + HEAD_R + 6
    miny, maxy = min(ys) - HEAD_R - 6, max(ys) + 4
    extra = pose.get("extra_height", 0)
    w, h = maxx - minx, maxy - miny + extra
    s = min((size - 2 * margin) / w, (size - 2 * margin - 6) / h)
    ox = (size - w * s) / 2 - minx * s
    ground = size - margin
    oy = ground - maxy * s - pose.get("lift", 0) * s

    def P(name):
        x, y = pts[name]
        return (ox + x * s, oy + y * s)

    img = Image.new("1", (size, size), 1)
    d = ImageDraw.Draw(img)
    lw = max(3, int(LIMB_W * s * 0.9))
    if pose.get("ground", True):
        d.line([(margin // 2, ground + 2), (size - margin // 2, ground + 2)], fill=0, width=2)
    if pose.get("cushion"):
        hx, hy = P("hip")
        d.rounded_rectangle([hx - 26 * s, hy + 2 * s, hx + 26 * s, ground + 2], radius=int(4 * s), fill=0)
    if pose.get("bike"):
        hx, hy = P("hip")
        r = 16 * s
        for wx in (hx - 34 * s, hx + 40 * s):
            d.ellipse([wx - r, ground - 2 * r, wx + r, ground], outline=0, width=max(2, int(2 * s)))
        d.line([(hx, hy + 4 * s), (hx - 34 * s, ground - r)], fill=0, width=max(2, int(3 * s)))
        d.line([(hx, hy + 4 * s), (hx + 40 * s, ground - r)], fill=0, width=max(2, int(3 * s)))
        hr = P("hand_r")
        d.line([hr, (hx + 40 * s, ground - r)], fill=0, width=max(2, int(3 * s)))
    order = [("l", False), ("r", True)] if not pose.get("front") else [("l", True), ("r", True)]
    for side, near in order:
        _limb(d, P("hipj_" + side), P("knee_" + side), near, lw)
        _limb(d, P("knee_" + side), P("foot_" + side), near, lw)
    _limb(d, P("hip"), P("neck"), True, lw + 2)
    if pose.get("front"):  # shoulder and hip bars
        _limb(d, P("shoulder_l"), P("shoulder_r"), True, lw)
        _limb(d, P("hipj_l"), P("hipj_r"), True, lw)
    for side, near in order:
        _limb(d, P("shoulder_" + side), P("elbow_" + side), near, lw)
        _limb(d, P("elbow_" + side), P("hand_" + side), near, lw)
    hx, hy = P("head")
    r = HEAD_R * s
    d.ellipse([hx - r, hy - r, hx + r, hy + r], fill=0)
    eq = pose.get("equip")
    if eq == "kettlebell":
        a, b = P("hand_r"), P("hand_l")
        _kettlebell(d, ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2), s)
    elif eq in ("dumbbell", "dumbbell_v"):
        for side in ("r", "l"):
            _dumbbell(d, P("hand_" + side), s * 0.8, eq == "dumbbell_v")
    elif eq == "dumbbell1":
        _dumbbell(d, P("hand_r"), s * 0.8)
    elif eq == "wheel":  # ab wheel on the floor under the hands
        a, b = P("hand_r"), P("hand_l")
        _wheel(d, ((a[0] + b[0]) / 2, 0), s, ground)
    elif eq == "plate":
        a, b = P("hand_r"), P("hand_l")
        _plate(d, ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2), s)
    if pose.get("toes"):  # on the balls of the feet
        for side in ("r", "l"):
            fx, fy = P("foot_" + side)
            d.line([(fx, fy), (fx + 6 * s, fy + 3 * s)], fill=0, width=max(2, int(3 * s)))
    return img


def to_pbm(img):
    """Binary PBM (P4): 1 = black."""
    w, h = img.size
    px = img.load()
    out = bytearray(f"P4\n{w} {h}\n".encode())
    for y in range(h):
        row = bytearray((w + 7) // 8)
        for x in range(w):
            if px[x, y] == 0:
                row[x >> 3] |= 0x80 >> (x & 7)
        out += row
    return bytes(out)
