import datetime as dt, json, os
today = dt.datetime.utcnow().date()
mon = today - dt.timedelta(days=today.weekday())
names = ["Water","Reading","Exercise","Meditation","Study","Sleep"]
pat = {0:"1111111",1:"1101111",2:"1010101",3:"0111011",4:"1111100",5:"1111111"}
for w in range(0, 4):
    m = mon - dt.timedelta(weeks=w); y, wk, _ = m.isocalendar()
    days = today.weekday() if w == 0 else 7   # this week: leave today unticked
    lines = [f"{n}|" + "".join(pat[i][d] if d < days else "0" for d in range(7)) for i, n in enumerate(names)]
    open(f"tools/habits/{y:04d}-W{wk:02d}.txt", "w").write("\n".join(lines) + "\n")
open("tools/pomodoro.txt","w").write(f"focus=25\nbreak=5\ndate={today}\ncompleted=3\n")
json.dump({"date": str(today), "todos": [
  {"text": "Review pull request #42", "done": True},
  {"text": "Finish chapter 3 of the novel", "done": False},
  {"text": "Run two Pomodoro sessions", "done": True},
  {"text": "Review Python flashcards", "done": False},
  {"text": "Call the bank", "done": False}]}, open("tools/daily.json","w"), indent=2)
