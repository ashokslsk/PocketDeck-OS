# Extra demo data for screenshots: 8 habits (with Yoga, Workout) and a long quote for tomorrow.
import datetime as dt, os
open("tools/habits/habits.txt","w").write("Water\nReading\nExercise\nMeditation\nStudy\nSleep\nYoga\nWorkout\n")
ist = dt.datetime.utcnow() + dt.timedelta(hours=5, minutes=30)
tomorrow = (ist.date() + dt.timedelta(days=1)).isoformat()
long_quote = ("Do not dwell in the past, do not dream of the future, concentrate the mind on the present moment. "
  "Every morning we are born again; what we do today is what matters most. Peace comes from within; do not seek it without. "
  "Three things cannot be long hidden: the sun, the moon, and the truth. Holding on to anger is like grasping a hot coal "
  "with the intent of throwing it at someone else; you are the one who gets burned. There is no path to happiness: "
  "happiness is the path. Work out your own salvation, and do not depend on others. The mind is everything; what you "
  "think, you become. You yourself, as much as anybody in the entire universe, deserve your love and affection. "
  "Just as a candle cannot burn without fire, men cannot live without a spiritual life. Better than a thousand hollow "
  "words is one word that brings peace. Health is the greatest gift, contentment the greatest wealth, faithfulness the "
  "best relationship. To keep the body in good health is a duty, otherwise we shall not be able to keep our mind strong "
  "and clear. No one saves us but ourselves; no one can and no one may; we ourselves must walk the path. Thousands of "
  "candles can be lit from a single candle, and the life of the candle will not be shortened; happiness never decreases "
  "by being shared. What we think, we become. All that we are is the result of what we have thought. The tongue like a "
  "sharp knife kills without drawing blood. An idea that is developed and put into action is more important than an idea "
  "that exists only as an idea. Every human being is the author of his own health or disease. Do not overrate what you "
  "have received, nor envy others; he who envies others does not obtain peace of mind.")
q = open("tools/quotes.txt").read()
open("tools/quotes.txt","w").write(f"{tomorrow}|{long_quote} — Traditional, attributed to the Buddha\n" + q)
