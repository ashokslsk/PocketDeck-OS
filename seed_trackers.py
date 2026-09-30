# Demo history for the new trackers (habit times, medicine courses, mood,
# Pomodoro / Today / flashcard logs) so every stats screen has real data.
import datetime as dt, os, random
random.seed(7)
ist = dt.datetime.utcnow() + dt.timedelta(hours=5, minutes=30)
today = ist.date()
now_min = ist.hour * 60 + ist.minute

def log(feature, day, minute, fields):
    os.makedirs(f"tools/{feature}", exist_ok=True)
    minute = max(0, min(1439, minute))
    with open(f"tools/{feature}/log-{day:%Y-%m}.txt", "a") as f:
        f.write(f"{day:%Y-%m-%d} {minute // 60:02d}:{minute % 60:02d}|{fields}\n")

# ---- Habits: 9 habits over 12 weeks, with tick times for the last 30 days.
names = ["Water", "Reading", "Exercise", "Meditation", "Study", "Sleep", "Yoga", "Workout", "Tender coconut water"]
open("tools/habits/habits.txt", "w").write("\n".join(names) + "\n")
rate = {"Water": .95, "Reading": .8, "Exercise": .6, "Meditation": .7, "Study": .75, "Sleep": .9, "Yoga": .85,
        "Workout": .55, "Tender coconut water": .8}
usual = {"Water": 7 * 60 + 30, "Reading": 21 * 60 + 45, "Exercise": 18 * 60, "Meditation": 6 * 60 + 40,
         "Study": 20 * 60, "Sleep": 22 * 60 + 50, "Yoga": 6 * 60 + 10, "Workout": 17 * 60 + 30,
         "Tender coconut water": 11 * 60}
spread = {"Tender coconut water": 55, "Yoga": 12, "Water": 20}
mon = today - dt.timedelta(days=today.weekday())
ticks = {}
for w in range(12):
    m = mon - dt.timedelta(weeks=w)
    y, wk, _ = m.isocalendar()
    lines = []
    for n in names:
        bits = ""
        for d in range(7):
            day = m + dt.timedelta(days=d)
            # Today stays open so the screenshots can tick it; recent weeks trend upwards.
            on = day < today and random.random() < min(.98, rate[n] + (.08 if w < 3 else -.05 * (w > 8)))
            bits += "1" if on else "0"
            if on:
                ticks.setdefault(n, []).append(day)
        lines.append(f"{n}|{bits}")
    open(f"tools/habits/{y:04d}-W{wk:02d}.txt", "w").write("\n".join(lines) + "\n")
for n in names:
    for day in ticks.get(n, []):
        if (today - day).days < 30:
            minute = usual[n] + int(random.gauss(0, spread.get(n, 25)))
            log("habits", day, minute, f"{day:%Y-%m-%d}|{n}|1")

# ---- Medicine & supplement courses.
def course(cid, name, doses, days, start, stopped, times, taken_rate, delay):
    stop = "-" if stopped is None else f"{stopped:%Y-%m-%d}"
    line = f"{cid}|{name}|{doses}|{days}|{start:%Y-%m-%d}|{stop}|{','.join(times)}"
    tmins = [int(t[:2]) * 60 + int(t[3:]) for t in times]
    end = start + dt.timedelta(days=days - 1)
    if stopped is not None:
        end = min(end, stopped)
    d = start
    while d <= min(end, today):
        for s, tm in enumerate(tmins):
            if d == today and tm > now_min:
                continue
            if random.random() < taken_rate:
                log("medicine", d, tm + int(random.gauss(delay, 25)), f"{cid}|{d:%Y-%m-%d}|{s}|1")
        d += dt.timedelta(days=1)
    return line
os.makedirs("tools/medicine", exist_ok=True)
lines = ["# PocketDeck-OS medicine & supplement courses",
         "# id|name|doses per day (1-4)|days (1-90)|start YYYY-MM-DD|stopped YYYY-MM-DD or -|dose times HH:MM,..."]
lines.append(course("c0000p01", "Paracetamol 500 mg", 3, 4, today - dt.timedelta(days=1), None,
                    ["08:00", "13:00", "19:00"], .9, 12))
lines.append(course("c0000v02", "Vitamin D3", 1, 30, today - dt.timedelta(days=19), None, ["08:30"], .85, 20))
lines.append(course("c0000a03", "Amoxicillin", 3, 5, today - dt.timedelta(days=24), None,
                    ["08:00", "14:00", "20:00"], .96, 8))
lines.append(course("c0000i04", "Iron supplement", 1, 21, today - dt.timedelta(days=15),
                    today - dt.timedelta(days=6), ["21:00"], .8, -10))
open("tools/medicine/courses.txt", "w").write("\n".join(lines) + "\n")

# ---- Mood: most days of the last 60, with a few notes.
notes = {2: "Slept badly", 5: "Long walk by the lake", 9: "Deadline stress", 12: "Family dinner",
         16: "Finished a good book"}
for back in range(60, 0, -1):
    day = today - dt.timedelta(days=back)
    if random.random() < .15:
        continue
    base = 3.4 + (0.5 if back < 14 else 0) + (0.3 if day.weekday() >= 5 else 0)
    mood = max(1, min(5, round(random.gauss(base, .8))))
    log("mood", day, 21 * 60 + int(random.gauss(15, 30)), f"{day:%Y-%m-%d}|{mood}|{notes.get(back, '')}")

# ---- Pomodoro focus sessions, Today summaries and flashcard sessions.
for back in range(45, 0, -1):
    day = today - dt.timedelta(days=back)
    if day.weekday() < 5 and random.random() < .8:
        start = 9 * 60 + int(random.gauss(30, 25))
        for k in range(random.randint(1, 5)):
            log("pomodoro", day, start + k * 30 + 25, "focus|25")
    if random.random() < .7:
        total = random.randint(3, 7)
        done = max(0, min(total, total - random.randint(0, 3)))
        log("today", day + dt.timedelta(days=1), 8 * 60 + 5, f"{day:%Y-%m-%d}|{done}|{total}")
    if random.random() < .5:
        deck = random.choice(["python_basics", "sql_basics", "ai_concepts"])
        rev = random.randint(5, 20)
        rem = rev - random.randint(0, rev // 3)
        log("flashcards", day, 20 * 60 + int(random.gauss(30, 30)), f"{deck}|{rev}|{rem}|{rev - rem}")

# ---- World clock: default four cities (pickable on the device).
open("tools/panchanga.txt", "w").write("lat=12.9716\nlon=77.5946\ntz=5.5\nanimate=1\nlang=kn\n")

# Today's three sessions (pomodoro.txt says completed=3) belong in the log too.
for k in range(3):
    log("pomodoro", today, 9 * 60 + 25 + 30 * k, "focus|25")

# ---- Reading history (global stats v3) so the reading-stats charts have data.
import struct
os.makedirs(".pocketdeck-os", exist_ok=True)
anchor = (today - dt.date(2000, 1, 1)).days
bits = bytearray(92)
read_days = 0
for back in range(0, 120):
    if back == 0 or random.random() < (0.8 if back < 30 else 0.55):
        bits[back // 8] |= 1 << (back % 8)
        read_days += 1
tod = [5 * 3600 + 1200, 3 * 3600 + 600, 17 * 3600 + 2400, 9 * 3600 + 300]
dow = [4 * 3600, 3 * 3600 + 1800, 5 * 3600, 3 * 3600, 4 * 3600 + 2400, 7 * 3600, 8 * 3600 + 1200]
total = sum(tod)
blob = struct.pack("<BIIII", 3, 96, total, 5100, 3) + struct.pack("<4I", *tod) + struct.pack("<7I", *dow)
blob += struct.pack("<I", anchor) + bytes(bits) + struct.pack("<H", 14)
assert len(blob) == 159, len(blob)
open(".pocketdeck-os/global_stats.bin", "wb").write(blob)

# ---- Knowledge study history and days the daily quote was opened.
topics = {"AI": 4, "Python": 5, "SQL": 4}
for back in range(40, 0, -1):
    day = today - dt.timedelta(days=back)
    if random.random() < .6:
        for k in range(random.randint(1, 4)):
            t = random.choice(list(topics))
            log("knowledge", day, 19 * 60 + int(random.gauss(40, 30)), f"{t}|{random.randrange(topics[t])}")
    if random.random() < .75:
        log("quotes", day, 7 * 60 + int(random.gauss(15, 20)), f"open|{day:%Y-%m-%d}")

# ---- Japa sessions (Mantras): mostly one mala at dawn, sometimes two, for 45 days.
deities = ["vishnu", "shiva", "ganesha", "lakshmi", "hanuman", "Morning routine"]
for back in range(45, 0, -1):
    day = today - dt.timedelta(days=back)
    if random.random() < .8:
        for k in range(random.choice([1, 1, 1, 2])):
            count = random.choice([108, 108, 108, 216, 54])
            log("mantras", day, 6 * 60 + int(random.gauss(10, 25)) + 600 * k,
                f"{count}|{random.choice(deities)}|{random.randrange(20)}|Om Namo Narayanaya")
