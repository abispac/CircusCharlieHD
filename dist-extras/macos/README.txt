Circus Charlie HD - macOS
=========================

Drag "Circus Charlie HD.app" anywhere and open it. The app is signed and
notarized with Apple, and the SDL libraries are inside it; nothing else
needs installing.

Controls
--------
  5 or C        insert a coin
  1 or Enter    START
  Left / Right  move
  U / D         grow / shrink Charlie 2% a press (remembered)
  Space or Z    jump / release the bar
  R             restart the current event
  F10           rotate the picture 90 degrees (0 -> 90 -> 180 -> 270)
  F11           toggle fullscreen
  Esc           during play: back to the coin screen
                attract mode / coin screen: quit

Arcade flow
-----------
Power on: boot screen, then the attract loop (a stage, the "insert coin"
reel, another stage...). Insert a coin and press START to play Event 1.
Clear the six events in a row, trapeze included, and the event selection
screen unlocks: Events 1-5 can each be picked five times (then they lock),
and the trapeze never ends - every round gets harder, like the arcade.
Your play counts and the unlock survive restarts.

Command-line options
--------------------
Open Terminal and run the binary inside the app to pass options:

  "/path/to/Circus Charlie HD.app/Contents/MacOS/circus_charlie_hd" --fullscreen

  --fullscreen            fullscreen
  --rotate 90|180|270     rotate the picture (a landscape panel on its side)
  --render 240x320        draw at a low-resolution cabinet's size
  --render native         draw at the display's full resolution
  --event N               jump straight to event N (1-6)

Source code and bug reports: https://github.com/abispac/CircusCharlieHD
