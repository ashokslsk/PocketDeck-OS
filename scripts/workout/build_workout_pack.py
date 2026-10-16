#!/usr/bin/env python3
"""Builds the SD-card pack for Tools > Workout.

Writes sd-sample/tools/workout/:
  manifests/1_pranayama.json, 2_yoga.json, 3_gym.json, 4_longevity.json
  pranayama/, yoga/, gym/, chinese/, ayurveda/   pose figures (PBM, 1-bit, 160x160)

The instructions, timings and cautions are original text written for this
pack; the figures are drawn from joint angles (figures.py, poses.py). Edit or
add manifests freely: the device reads every *.json in manifests/ in file-name
order. Format: docs/productivity-tools.md ("Workout").

usage: build_workout_pack.py [--out sd-sample/tools/workout]
requires: pip install pillow
"""
import argparse, json, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figures  # noqa: E402
from poses import POSES  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GENERAL = ("Stop if you feel pain, dizziness or breathlessness. If you are pregnant, recovering from surgery or have "
           "a heart, blood-pressure or other medical condition, check with your doctor first.")


def ph(label, seconds, shape):
    return {"label": label, "seconds": seconds, "shape": shape}


PRANAYAMA = {
    "title_en": "Pranayama", "title_kn": "ಪ್ರಾಣಾಯಾಮ",
    "note": "Practise on an empty stomach, sitting tall. Breathe through the nose unless told otherwise. " + GENERAL,
    "items": [
        {"id": "anuloma_viloma", "name_en": "Alternate Nostril Breathing", "name_kn": "ಅನುಲೋಮ ವಿಲೋಮ",
         "type": "breath", "minutes": [5, 10], "image": "pranayama/anuloma_viloma.pbm",
         "phases": [ph("Inhale left", 4, "grow"), ph("Exhale right", 8, "shrink"),
                    ph("Inhale right", 4, "grow"), ph("Exhale left", 8, "shrink")],
         "accessories": ["Cushion or folded blanket", "Quiet place"],
         "how": ["Sit cross-legged or on a chair, spine tall, shoulders soft.",
                 "Rest the left hand on the knee. Fold the index and middle fingers of the right hand; use the thumb for the right nostril and the ring finger for the left.",
                 "Close the right nostril and breathe in through the left for 4 counts.",
                 "Close the left, open the right and breathe out for 8 counts.",
                 "Breathe in through the right, switch, and breathe out through the left. That is one round.",
                 "Keep the breath silent and smooth; the out-breath is twice the in-breath."],
         "caution": "Never strain. If the nose is blocked, breathe gently through both nostrils."},
        {"id": "bhastrika", "name_en": "Bellows Breath", "name_kn": "ಭಸ್ತ್ರಿಕಾ", "type": "breath",
         "minutes": [5, 10], "image": "pranayama/bhastrika.pbm",
         "phases": [ph("Breathe in fully", 1, "grow"), ph("Breathe out fully", 1, "shrink")],
         "repeat": 20, "rest": ph("Rest, breathe normally", 30, "hold"),
         "accessories": ["Cushion", "Tissue"],
         "how": ["Sit tall with hands on the knees.",
                 "Breathe in and out through the nose with equal force, about one breath a second, letting the chest and belly move like bellows.",
                 "Do 20 breaths, then rest with normal breathing for 30 seconds. Repeat.",
                 "Keep the face, neck and shoulders relaxed."],
         "caution": "Not during pregnancy, or with high blood pressure, heart disease, epilepsy, hernia or recent surgery. Stop at once if you feel light-headed."},
        {"id": "kapalbhati", "name_en": "Skull-Shining Breath", "name_kn": "ಕಪಾಲಭಾತಿ", "type": "breath",
         "minutes": [5, 10], "image": "pranayama/kapalbhati.pbm",
         "phases": [ph("Exhale sharply", 1, "pulse")], "repeat": 30,
         "rest": ph("Rest, breathe normally", 30, "hold"),
         "accessories": ["Cushion", "Empty stomach (3 hours after a meal)"],
         "how": ["Sit tall, hands on the knees, and take a comfortable breath in.",
                 "Pull the belly in quickly to push the breath out through the nose. Let the in-breath come back by itself.",
                 "Keep a steady rhythm of about one stroke a second; only the belly moves.",
                 "After 30 strokes, rest with normal breathing for 30 seconds. Repeat."],
         "caution": "Not during pregnancy or menstruation, or with high blood pressure, heart disease, hernia, ulcers, epilepsy or recent abdominal surgery."},
        {"id": "ujjayi", "name_en": "Ocean Breath", "name_kn": "ಉಜ್ಜಾಯಿ", "type": "breath",
         "minutes": [5, 10], "image": "pranayama/ujjayi.pbm",
         "phases": [ph("Inhale, soft throat", 5, "grow"), ph("Exhale, soft throat", 6, "shrink")],
         "accessories": ["Cushion"],
         "how": ["Sit tall with the mouth closed.",
                 "Narrow the back of the throat slightly, as if fogging a mirror with the mouth shut, so the breath makes a soft ocean sound.",
                 "Breathe in for 5 counts and out for 6, keeping the sound even.",
                 "Let the breath be long and quiet; the sound should only be heard by you."],
         "caution": "If the throat feels tight, ease the narrowing and breathe normally."},
        {"id": "bhramari", "name_en": "Humming Bee Breath", "name_kn": "ಭ್ರಾಮರಿ", "type": "breath",
         "minutes": [5, 10], "image": "pranayama/bhramari.pbm",
         "phases": [ph("Inhale", 4, "grow"), ph("Exhale humming 'mmm'", 8, "shrink")],
         "accessories": ["Cushion", "Quiet room"],
         "how": ["Sit tall and close the eyes.",
                 "Gently close the ears with the thumbs (or index fingers) on the small flap of cartilage.",
                 "Breathe in through the nose for 4 counts.",
                 "Breathe out for 8 counts while humming with the lips closed, feeling the vibration in the head.",
                 "Keep the jaw relaxed and the teeth slightly apart."],
         "caution": "Do not press into the ear canal. Avoid with an ear infection."},
        {"id": "sheetali", "name_en": "Cooling Breath (Sheetali / Sheetkari)", "name_kn": "ಶೀತಲಿ / ಶೀತ್ಕಾರಿ",
         "type": "breath", "minutes": [5, 10], "image": "pranayama/sheetali.pbm",
         "phases": [ph("Inhale through the mouth", 4, "grow"), ph("Hold", 2, "hold"),
                    ph("Exhale through the nose", 6, "shrink")],
         "accessories": ["Cushion", "A cool, clean room"],
         "how": ["Sheetali: roll the tongue into a tube and breathe in through it. Sheetkari: if you cannot roll the tongue, close the teeth lightly and breathe in through them.",
                 "Close the mouth and hold briefly for 2 counts.",
                 "Breathe out slowly through the nose for 6 counts.",
                 "Feel the cool air on the in-breath; keep the shoulders down."],
         "caution": "Avoid in cold weather, or with low blood pressure, asthma, a cold or sensitive teeth."},
        {"id": "nadi_shodhana", "name_en": "Channel-Cleansing Breath", "name_kn": "ನಾಡೀ ಶೋಧನ", "type": "breath",
         "minutes": [5, 10], "image": "pranayama/nadi_shodhana.pbm",
         "phases": [ph("Inhale left", 4, "grow"), ph("Hold, both closed", 4, "hold"),
                    ph("Exhale right", 8, "shrink"), ph("Inhale right", 4, "grow"),
                    ph("Hold, both closed", 4, "hold"), ph("Exhale left", 8, "shrink")],
         "accessories": ["Cushion or folded blanket"],
         "how": ["Use the same hand position as alternate nostril breathing.",
                 "Breathe in through the left nostril for 4 counts.",
                 "Close both nostrils and hold the breath gently for 4 counts.",
                 "Breathe out through the right for 8 counts, then in through the right for 4.",
                 "Hold for 4, then breathe out through the left for 8. That is one round."],
         "caution": "Leave out the holds if you are pregnant or have high blood pressure or a heart condition."},
    ],
}

SURYA = [
    ("Pranamasana", "ಪ್ರಣಾಮಾಸನ", "Stand tall, palms together at the chest. Breathe out.", "pranamasana"),
    ("Hasta Uttanasana", "ಹಸ್ತ ಉತ್ತಾನಾಸನ", "Breathe in, raise the arms and arch back gently.", "hasta_uttanasana"),
    ("Padahastasana", "ಪಾದಹಸ್ತಾಸನ", "Breathe out, fold forward and bring the hands beside the feet (bend the knees if needed).", "padahastasana"),
    ("Ashwa Sanchalanasana", "ಅಶ್ವ ಸಂಚಲನಾಸನ", "Breathe in, step the right leg back, knee down, and look up.", "ashwa_sanchalanasana"),
    ("Dandasana", "ದಂಡಾಸನ", "Hold the breath, step the left leg back into a straight plank.", "plank"),
    ("Ashtanga Namaskara", "ಅಷ್ಟಾಂಗ ನಮಸ್ಕಾರ", "Breathe out, lower knees, chest and chin to the floor, hips raised.", "ashtanga_namaskara"),
    ("Bhujangasana", "ಭುಜಂಗಾಸನ", "Breathe in, slide forward and lift the chest into cobra.", "bhujangasana"),
    ("Parvatasana", "ಪರ್ವತಾಸನ", "Breathe out, lift the hips into an inverted V.", "adho_mukha_svanasana"),
    ("Ashwa Sanchalanasana", "ಅಶ್ವ ಸಂಚಲನಾಸನ", "Breathe in, step the right foot forward between the hands and look up.", "ashwa_sanchalanasana"),
    ("Padahastasana", "ಪಾದಹಸ್ತಾಸನ", "Breathe out, bring the left foot forward and fold.", "padahastasana"),
    ("Hasta Uttanasana", "ಹಸ್ತ ಉತ್ತಾನಾಸನ", "Breathe in, rise with the arms overhead and arch gently.", "hasta_uttanasana"),
    ("Pranamasana", "ಪ್ರಣಾಮಾಸನ", "Breathe out, palms together at the chest. Next round: lead with the left leg.", "pranamasana"),
]


# Surya Namaskara mantras (user-supplied, scripts/workout/surya_namaskara_mantras.json):
# per-pose mantras go into the steps, the verses into yoga/surya_mantras.json.
MANTRAS_SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "surya_namaskara_mantras.json")


def surya_steps():
    src = json.load(open(MANTRAS_SRC, encoding="utf-8"))["surya_namaskara"]["steps"]
    out = []
    for i, (en, kn, t, img) in enumerate(SURYA):
        m = src[i]
        out.append({"name": f"{i + 1}. {m['posture_english']}", "name_kn": m["posture_kannada"],
                    "text": m["movement_english"], "image": f"yoga/{img}.pbm",
                    "mantra_kn": m["mantra_kannada"], "mantra_en": m["mantra_english"],
                    "meaning": m["meaning_english"], "breath": m["breath"]})
    return out


def surya_pages():
    d = json.load(open(MANTRAS_SRC, encoding="utf-8"))
    sn = d["surya_namaskara"]

    def verse(sec_kn, sec_en, v, n=0, kn="kannada", en="english", meaning="meaning_english"):
        return {"section_kn": sec_kn, "section_en": sec_en, "number": n, "kn": v.get(kn, ""),
                "en": v.get(en, ""), "meaning": v.get(meaning, "")}

    pages = [verse("ಸಮಾಪ್ತಿ ಶ್ಲೋಕ", "Surya Namaskara: closing verse", sn["closing_verse"])]
    for key in ("bhaskara_stotram", "surya_ashtakam", "guru_namaskara"):
        sec = d[key]
        for v in sec["verses"]:
            pages.append(verse(sec["title_kannada"], sec["title_english"], v, v["verse_number"]))
        if "phalashruti" in sec:
            pages.append(verse(sec["title_kannada"], sec["title_english"] + ": phalashruti", sec["phalashruti"]))
    dd = d["dheerga_danda_namaskara"]
    pages.append(verse(dd["title_kannada"], dd["title_english"], dd, 0, "description_kannada", "description_english", "-"))
    pages.append(verse(dd["title_kannada"], dd["title_english"], dd, 0, "mantra_kannada", "mantra_english", "-"))
    pages.append(verse(dd["title_kannada"], dd["title_english"], dd, 0, "kannada_shloka", "english_shloka"))
    return {"opening": verse("ಧ್ಯಾನ ಶ್ಲೋಕ", "Surya Namaskara: opening verse", sn["opening_verse"]), "pages": pages}


def asana(i, en, kn, pose, secs, how, acc, caution="", sides=1, benefit=""):
    return {"id": i, "name_en": en, "name_kn": kn, "type": "hold", "seconds": secs, "sides": sides,
            "image": f"yoga/{pose}.pbm", "how": how, "accessories": acc, "caution": caution, "benefit": benefit}


YOGA = {
    "title_en": "Yoga", "title_kn": "ಯೋಗ",
    "note": "Move slowly and breathe through the nose. Hold times are for building up gently: start with Beginner. " + GENERAL,
    "items": [
        {"id": "surya_namaskara", "name_en": "Sun Salutation", "name_kn": "ಸೂರ್ಯ ನಮಸ್ಕಾರ", "type": "sequence",
         "rounds": [3, 12], "step_seconds": [8, 4], "image": "yoga/pranamasana.pbm",
         "steps": surya_steps(), "mantra_pages": "yoga/surya_mantras.json",
         "accessories": ["Yoga mat"],
         "how": ["Twelve poses linked with the breath; one round leads with the right leg, the next with the left.",
                 "Beginner holds each pose for 8 seconds; Advanced flows at 4 seconds a pose.",
                 "Each pose shows its mantra, meaning and breath. The opening verse comes first; after the last round the closing verse, Bhaskara Stotram, Surya Ashtakam, Guru Namaskara and Dheerga Danda Namaskara follow, one verse per page.",
                 "Follow the pose name and cue on screen; keep the movements smooth."],
         "caution": "Skip the back-bend if you have a back injury. Avoid in late pregnancy."},
        asana("tadasana", "Mountain Pose", "ತಾಡಾಸನ", "tadasana", [30, 60],
              ["Stand with feet together or hip-width apart, weight even on both feet.",
               "Lengthen the spine, relax the shoulders, arms by the sides.",
               "Breathe slowly and feel the body stand tall and steady."], ["Yoga mat"],
              benefit="Posture and balance."),
        asana("vrikshasana", "Tree Pose", "ವೃಕ್ಷಾಸನ", "vrikshasana", [30, 90],
              ["Stand on the left leg. Place the right foot on the inner calf or thigh (never on the knee).",
               "Bring the palms together at the chest or overhead.",
               "Fix the gaze on one point and breathe evenly. Change legs."], ["Yoga mat", "Wall for support"],
              "Use a wall if balance is unsteady.", sides=2, benefit="Balance and focus."),
        asana("trikonasana", "Triangle Pose", "ತ್ರಿಕೋಣಾಸನ", "trikonasana", [30, 60],
              ["Step the feet wide; turn the right foot out.",
               "Reach to the right and lower the right hand to the shin or a block; raise the left arm.",
               "Keep both legs straight and the chest open. Change sides."], ["Yoga mat", "Yoga block"],
              "Keep the neck neutral if it feels strained.", sides=2, benefit="Stretches the sides and legs."),
        asana("virabhadrasana2", "Warrior II", "ವೀರಭದ್ರಾಸನ ೨", "virabhadrasana2", [30, 60],
              ["Step the feet wide, right foot out, left foot slightly in.",
               "Bend the right knee over the ankle; stretch the arms out at shoulder height.",
               "Look over the right hand; keep the torso upright. Change sides."], ["Yoga mat"],
              "Keep the knee over the ankle, not past the toes.", sides=2, benefit="Leg strength and stamina."),
        asana("bhujangasana", "Cobra Pose", "ಭುಜಂಗಾಸನ", "bhujangasana", [20, 45],
              ["Lie on the front with hands under the shoulders, legs together.",
               "Breathe in and lift the chest, elbows slightly bent, hips on the floor.",
               "Keep the shoulders away from the ears; lower down as you breathe out."], ["Yoga mat"],
              "Avoid with a recent back injury or in pregnancy.", benefit="Strengthens the back."),
        asana("setu_bandhasana", "Bridge Pose", "ಸೇತುಬಂಧಾಸನ", "setu_bandhasana", [20, 60],
              ["Lie on the back, knees bent, feet hip-width apart near the hips.",
               "Press into the feet and lift the hips; arms rest on the floor.",
               "Breathe steadily, then roll down slowly."], ["Yoga mat", "Yoga block under the hips (supported)"],
              "Do not turn the head while in the pose.", benefit="Opens the chest, strengthens legs and back."),
        asana("adho_mukha_svanasana", "Downward Dog", "ಅಧೋಮುಖ ಶ್ವಾನಾಸನ", "adho_mukha_svanasana", [30, 60],
              ["From hands and knees, lift the hips up and back into an inverted V.",
               "Spread the fingers, press the floor away, heels reaching down.",
               "Bend the knees as much as needed to keep the back long."], ["Yoga mat"],
              "Avoid with uncontrolled high blood pressure or a wrist injury.",
              benefit="Stretches the back of the body."),
        asana("balasana", "Child's Pose", "ಬಾಲಾಸನ", "balasana", [30, 120],
              ["Kneel, sit back on the heels and fold forward.",
               "Rest the forehead on the mat, arms forward or beside the body.",
               "Breathe into the back; rest here whenever you need."], ["Yoga mat", "Cushion"],
              "Place a cushion between the hips and heels if the knees hurt.", benefit="Rest and gentle back stretch."),
        asana("paschimottanasana", "Seated Forward Bend", "ಪಶ್ಚಿಮೋತ್ತಾನಾಸನ", "paschimottanasana", [30, 90],
              ["Sit with the legs straight in front.",
               "Breathe in to lengthen; breathe out and fold from the hips, reaching for the shins or feet.",
               "Keep the back long rather than rounding; use a strap around the feet."], ["Yoga mat", "Strap"],
              "Bend the knees if the lower back pulls.", benefit="Stretches hamstrings and spine."),
        asana("vajrasana", "Thunderbolt Pose", "ವಜ್ರಾಸನ", "vajrasana", [60, 300],
              ["Kneel and sit on the heels, big toes touching.",
               "Hands on the thighs, spine tall, breathe slowly.",
               "Traditionally practised after meals to aid digestion."], ["Yoga mat", "Cushion"],
              "Avoid with knee injuries.", benefit="Digestion and calm."),
        asana("marjaryasana", "Cat-Cow", "ಮಾರ್ಜರ್ಯಾಸನ", "cat_cow", [45, 90],
              ["Come to hands and knees, wrists under shoulders, knees under hips.",
               "Breathe in, let the belly drop and look up (cow).",
               "Breathe out, round the back and tuck the chin (cat). Keep flowing."], ["Yoga mat"],
              benefit="Mobility for the spine."),
        asana("shavasana", "Corpse Pose", "ಶವಾಸನ", "shavasana", [180, 600],
              ["Lie on the back, legs apart, arms away from the body, palms up.",
               "Close the eyes and let the whole body become heavy.",
               "Breathe naturally. End every session here."], ["Yoga mat", "Blanket", "Eye pillow"],
              benefit="Deep rest."),
    ],
}

CHECKLIST = ["Warm up for 5 minutes", "Gloves", "Belt for heavy lifts", "Wrist straps",
             "Heart-rate monitor", "Water bottle", "Towel"]


def lift(i, en, pose, cat, how, acc, caution, work=(40, 45), rest=(20, 15), sets=(3, 4), reps=12):
    return {"id": i, "name_en": en, "name_kn": "", "group": cat, "type": "interval", "work": list(work),
            "rest_seconds": list(rest), "sets": list(sets), "reps": reps, "image": f"gym/{pose}.pbm",
            "how": how, "accessories": acc, "caution": caution}


GYM = {
    "title_en": "Physical Workouts", "title_kn": "ವ್ಯಾಯಾಮ",
    "note": "Warm up first and move with control. Choose a weight you can move with good form for every rep. " + GENERAL,
    "checklist": CHECKLIST,
    "items": [
        lift("kb_swing", "Kettlebell Swing", "kb_swing", "Kettlebell",
             ["Feet a little wider than the hips, kettlebell in front.",
              "Hinge at the hips, hike the bell back between the legs.",
              "Drive the hips forward to float the bell to chest height; let it fall back into the hinge.",
              "The power comes from the hips, not the arms."], ["Kettlebell", "Gloves", "Flat floor"],
             "Keep the back flat; stop if the lower back hurts."),
        lift("goblet_squat", "Goblet Squat", "goblet_squat", "Kettlebell",
             ["Hold the kettlebell by the horns at the chest.",
              "Sit down between the heels, chest up, knees tracking over the toes.",
              "Stand by pushing the floor away."], ["Kettlebell"], "Go only as deep as you can with a flat back."),
        lift("kb_deadlift", "Kettlebell Deadlift", "kb_deadlift", "Kettlebell",
             ["Stand with the bell between the feet.",
              "Hinge, grip the handle with straight arms, back flat.",
              "Stand tall by driving the hips forward; lower with control."], ["Kettlebell", "Belt (optional)"],
             "Keep the bell close to the body."),
        lift("db_press", "Dumbbell Shoulder Press", "db_press", "Dumbbell",
             ["Stand or sit tall with the dumbbells at shoulder height.",
              "Press up until the arms are straight, without arching the back.",
              "Lower slowly to the shoulders."], ["Pair of dumbbells", "Wrist straps (optional)"],
             "Brace the belly; reduce the weight if the back arches."),
        lift("db_row", "Dumbbell Row", "db_row", "Dumbbell",
             ["Hinge forward with a flat back, one hand on a bench or knee.",
              "Pull the dumbbell toward the hip, elbow close to the body.",
              "Lower slowly; complete all reps, then switch arms."], ["Dumbbell", "Bench"], "Do not twist the torso."),
        lift("db_curl", "Dumbbell Biceps Curl", "db_curl", "Dumbbell",
             ["Stand tall with dumbbells by the sides, palms forward.",
              "Curl up without swinging, elbows at the sides.",
              "Lower slowly to full arm length."], ["Pair of dumbbells"], "Keep the wrists straight."),
        lift("db_lunge", "Dumbbell Lunge", "db_lunge", "Dumbbell",
             ["Hold dumbbells by the sides and step forward.",
              "Lower until both knees are bent about 90 degrees, back knee near the floor.",
              "Push back to standing; alternate legs."], ["Pair of dumbbells", "Clear floor space"],
             "Keep the front knee over the ankle."),
        lift("ab_rollout", "Ab Wheel Rollout, Kneeling", "ab_rollout", "Ab wheel",
             ["Kneel on a mat and hold the wheel under the shoulders.",
              "Brace the belly and tuck the hips slightly.",
              "Roll forward slowly, as far as you can keep the back flat.",
              "Pull back with the abdominals, not the arms."], ["Ab wheel", "Mat or knee pad"],
             "Stop short before the lower back sags; avoid with a back injury.", work=(30, 40), reps=10),
        lift("ab_standing", "Ab Wheel Rollout, Standing (advanced)", "ab_standing", "Ab wheel",
             ["Stand with feet hip-width apart and the wheel on the floor in front.",
              "Fold down to the wheel and roll out until the body is long and straight.",
              "Roll back to the fold using the core; keep the hips from dropping.",
              "Master kneeling rollouts first."], ["Ab wheel", "Non-slip floor"],
             "Only when kneeling rollouts are easy; stop if the lower back aches.", work=(30, 40), sets=(2, 3),
             reps=6),
        lift("ab_oblique", "Ab Wheel Oblique Rollout", "ab_rollout", "Ab wheel",
             ["Kneel and hold the wheel slightly to the right.",
              "Roll forward and out at an angle to the right, keeping the hips square.",
              "Return, then roll to the left. Each side is one rep."], ["Ab wheel", "Mat or knee pad"],
             "Keep the range short until you are strong at the side.", work=(30, 40), reps=8),
        {"id": "ab_plank", "name_en": "Ab Wheel Plank Hold", "name_kn": "", "group": "Ab wheel", "type": "hold",
         "seconds": [20, 60], "sides": 1, "image": "gym/ab_plank.pbm",
         "how": ["Kneel or come to a full plank with the hands on the wheel under the shoulders.",
                 "Hold the wheel still, belly tight, back flat, breathing steadily."],
         "accessories": ["Ab wheel", "Mat"], "caution": "Lower to the knees if the back sags."},
        {"id": "bike_hiit", "name_en": "Spin Bike Intervals (10 kg flywheel)", "name_kn": "", "group": "Spin bike", "type": "interval",
         "work": [30, 40], "rest_seconds": [60, 20], "sets": [6, 10], "reps": 0, "image": "gym/bike.pbm",
         "how": ["Set the saddle so the knee is slightly bent at the bottom of the stroke; handlebar at saddle height or a little higher.",
                 "Warm up for 3 minutes with light resistance.",
                 "Work: turn the resistance knob up until it feels hard, and pedal strongly for the work time.",
                 "Rest: turn the resistance down and pedal easily until the next interval.",
                 "The heavy 10 kg flywheel keeps spinning: always slow it with the brake or resistance knob, never by stopping your legs suddenly.",
                 "Cool down with 3 minutes of easy pedalling."],
         "accessories": ["Spin bike (10 kg flywheel)", "Cycling shoes or firm trainers", "Heart-rate monitor",
                         "Water bottle", "Towel"],
         "caution": "Build up gradually; stop if you feel chest pain or faintness."},
        {"id": "bike_steady", "name_en": "Spin Bike Steady Ride (10 kg flywheel)", "name_kn": "", "group": "Spin bike", "type": "hold",
         "seconds": [900, 1800], "sides": 1, "image": "gym/bike.pbm",
         "how": ["Set a moderate resistance and ride at a pace where you can still talk in short sentences.",
                 "Keep a smooth, steady cadence (about 70-90 turns a minute) and relaxed shoulders.",
                 "Use the resistance knob or brake to slow the flywheel at the end."],
         "accessories": ["Spin bike (10 kg flywheel)", "Water bottle", "Towel"],
         "caution": "Stop if you feel chest pain, faintness or sharp knee pain."},
    ],
}


def seq(i, en, kn, group, img_dir, steps, secs, rounds, how, acc, caution=""):
    return {"id": i, "name_en": en, "name_kn": kn, "group": group, "type": "sequence", "rounds": rounds,
            "step_seconds": secs, "image": f"{img_dir}/{steps[0][2]}.pbm",
            "steps": [{"name": f"{n + 1}. {name}", "text": text, "image": f"{img_dir}/{img}.pbm"}
                      for n, (name, text, img) in enumerate(steps)],
            "how": how, "accessories": acc, "caution": caution}


LONGEVITY = {
    "title_en": "Longevity Practices", "title_kn": "ದೀರ್ಘಾಯುಷ್ಯ ಅಭ್ಯಾಸಗಳು",
    "note": "Gentle, slow movement for every age. Move within a comfortable range and breathe naturally. " + GENERAL,
    "items": [
        seq("baduanjin", "Baduanjin (Eight Brocades)", "", "Chinese qigong", "chinese", [
            ("Holding up the sky", "Interlace the fingers and press the palms up, rising tall; lower slowly.", "hold_sky"),
            ("Drawing the bow", "In a horse stance, push one hand out and draw the other back like pulling a bow; change sides.", "draw_bow"),
            ("Separating heaven and earth", "Press one palm up and the other down; change hands.", "separate_heaven"),
            ("Looking back", "Turn the head and shoulders to look behind; return and change sides.", "look_back"),
            ("Swaying head and tail", "In a horse stance, lean and circle the upper body side to side.", "sway_head"),
            ("Touching the toes", "Slide the hands down the legs toward the feet, then rise slowly.", "touch_toes"),
            ("Punching with focus", "In a horse stance, punch slowly forward, alternating hands.", "punch"),
            ("Bouncing on the toes", "Rise onto the toes, then drop gently onto the heels.", "on_toes"),
        ], [45, 60], [1, 2], ["Eight slow movements, each repeated about 8 times.",
                             "Breathe naturally and keep the knees soft."], ["Loose clothing", "Flat shoes or bare feet"]),
        seq("wu_qin_xi", "Wu Qin Xi (Five Animals)", "", "Chinese qigong", "chinese", [
            ("Tiger", "Raise clawed hands slowly overhead, then press down with strength.", "tiger"),
            ("Deer", "Turn the waist with hands raised like antlers, reaching behind.", "deer"),
            ("Bear", "Sway heavily side to side, arms rounded, rotating from the waist.", "bear"),
            ("Monkey", "Rise on the toes, hand at the brow as if looking out; light and alert.", "monkey"),
            ("Crane", "Stand on one leg with the arms spread like wings; change legs.", "crane"),
        ], [90, 180], [1, 2], ["Imitate each animal's movement and spirit with slow, flowing breath."],
            ["Loose clothing", "Open space"]),
        seq("yi_jin_jing", "Yi Jin Jing (Tendon Strengthening)", "", "Chinese qigong", "chinese", [
            ("Palms together at the chest", "Stand quietly, palms joined, shoulders relaxed.", "pranamasana"),
            ("Arms stretched to the sides", "Open the arms to shoulder height, palms down; hold and breathe.", "taichi_open"),
            ("Pressing up to the sky", "Rise on the toes and press both palms overhead.", "hold_sky"),
            ("Reaching up and back", "Reach one hand up and back, the other low behind; change sides.", "separate_heaven"),
            ("Pulling back in a bow stance", "Step forward and draw the arms back slowly with strength.", "taichi_bow"),
            ("Pushing the palms forward", "Push the palms forward slowly as if moving a heavy door.", "plate_raise"),
            ("Bowing forward", "Fold forward gently, hands toward the feet, then rise.", "touch_toes"),
            ("Swinging the arms", "Let the arms swing loosely while turning the waist.", "taichi_clouds"),
        ], [30, 60], [1, 2], ["Slow tendon-stretching movements held with steady breath."], ["Loose clothing"]),
        seq("tai_chi", "Tai Chi Short Form", "", "Tai Chi", "chinese", [
            ("Commencing", "Feet shoulder-width apart; float the arms up to shoulder height and lower them.", "taichi_open"),
            ("Parting the wild horse's mane", "Step into a bow stance and part the hands, one forward, one back.", "taichi_bow"),
            ("White crane spreads its wings", "Weight back, one hand up, one down, front toe resting lightly.", "taichi_crane"),
            ("Brush knee and push", "Brush one hand past the knee and push the other forward.", "taichi_bow"),
            ("Wave hands like clouds", "Shift side to side with the hands circling before the body.", "taichi_clouds"),
            ("Closing", "Bring the hands down slowly and stand quietly.", "tadasana"),
        ], [40, 80], [2, 4], ["Move continuously and slowly, weight shifting smoothly from leg to leg."],
            ["Flat shoes or bare feet", "Loose clothing"]),
        seq("dinacharya", "Dinacharya (Morning Routine)", "ದಿನಚರ್ಯೆ", "Ayurveda", "ayurveda", [
            ("Wake early", "Rise before sunrise when you can; sit quietly for a moment.", "sunrise"),
            ("Warm water", "Drink a glass of warm water.", "tadasana"),
            ("Tongue scraping", "Scrape the tongue gently from back to front a few times.", "tadasana"),
            ("Oil pulling", "Swish a spoon of sesame or coconut oil in the mouth, then spit it out (not in the sink).", "seated"),
            ("Self-massage (abhyanga)", "Massage warm oil into the body with long strokes on limbs, circles on joints.", "joints"),
            ("Movement", "A few rounds of sun salutation or a short walk.", "hasta_uttanasana"),
            ("Breathing and stillness", "Sit for a few minutes of calm breathing or meditation.", "seated"),
        ], [60, 180], [1, 1], ["A gentle daily rhythm from Ayurveda; adapt the times to your day."],
            ["Tongue scraper", "Sesame or coconut oil", "Towel"],
            "Skip oil pulling if you might swallow the oil; skip abhyanga on broken skin."),
        seq("joint_rotations", "Joint Rotations (Sukshma Vyayama)", "ಸೂಕ್ಷ್ಮ ವ್ಯಾಯಾಮ", "Ayurveda", "ayurveda", [
            ("Neck", "Nod slowly, turn side to side, then tilt ear to shoulder.", "look_back"),
            ("Shoulders", "Circle the shoulders forward, then back.", "joints"),
            ("Elbows and wrists", "Bend and straighten the elbows; circle the wrists both ways.", "taichi_open"),
            ("Waist", "Hands on the hips, circle the waist slowly both ways.", "sway_head"),
            ("Hips", "Stand on one leg with support and circle the other knee; change legs.", "crane"),
            ("Knees", "Bend the knees slightly and circle them together.", "bear"),
            ("Ankles", "Lift one foot and circle the ankle both ways; change feet.", "on_toes"),
        ], [40, 60], [1, 2], ["Loosens each joint from the top down, about 10 slow circles each."],
            ["Chair or wall for support"]),
        {"id": "sama_vritti", "name_en": "Gentle Equal Breathing", "name_kn": "ಸಮ ವೃತ್ತಿ", "group": "Ayurveda",
         "type": "breath", "minutes": [5, 10], "image": "ayurveda/seated.pbm",
         "phases": [ph("Inhale", 4, "grow"), ph("Exhale", 4, "shrink")],
         "how": ["Sit or lie comfortably.", "Breathe in for 4 counts and out for 4 counts through the nose.",
                 "Let the belly rise and fall; keep it easy."], "accessories": ["Cushion"], "caution": ""},
    ],
}

CATEGORIES = [("1_pranayama.json", PRANAYAMA), ("2_yoga.json", YOGA), ("3_gym.json", GYM),
              ("4_longevity.json", LONGEVITY)]


def images_needed():
    out = set()
    for _, cat in CATEGORIES:
        for it in cat["items"]:
            out.add(it["image"])
            for st in it.get("steps", []):
                out.add(st["image"])
    return sorted(out)


# Image file name -> pose (pranayama techniques share seated poses).
ALIASES = {"anuloma_viloma": "seated_nostril", "nadi_shodhana": "seated_nostril", "bhastrika": "seated",
           "kapalbhati": "seated_belly", "ujjayi": "seated", "bhramari": "seated_ears", "sheetali": "seated"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "sd-sample", "tools", "workout"))
    args = ap.parse_args()
    os.makedirs(os.path.join(args.out, "manifests"), exist_ok=True)
    for name, cat in CATEGORIES:
        with open(os.path.join(args.out, "manifests", name), "w", encoding="utf-8") as f:
            json.dump(cat, f, ensure_ascii=False, indent=1)
    os.makedirs(os.path.join(args.out, "yoga"), exist_ok=True)
    with open(os.path.join(args.out, "yoga", "surya_mantras.json"), "w", encoding="utf-8") as f:
        json.dump(surya_pages(), f, ensure_ascii=False, indent=1)
    n = 0
    for rel in images_needed():
        stem = os.path.splitext(os.path.basename(rel))[0]
        pose = POSES[ALIASES.get(stem, stem)]
        path = os.path.join(args.out, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(figures.to_pbm(figures.render(pose)))
        n += 1
    total = sum(len(c["items"]) for _, c in CATEGORIES)
    print(f"wrote {len(CATEGORIES)} manifests ({total} practices) and {n} figures to {args.out}")


if __name__ == "__main__":
    main()
