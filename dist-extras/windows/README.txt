Circus Charlie HD - Windows 64-bit
==================================

Double-click CircusCharlieHD.exe. Keep the assets\ folder and the two DLLs
next to the .exe.

Controls
--------
  5 or C        insert a coin
  1 or Enter    START
  Left / Right  move (A / D also work)
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

Vertical monitors
-----------------
The game is 480x640, so a monitor mounted in portrait fills the screen
with no rotation:

  CircusCharlieHD.exe --fullscreen

A landscape panel turned on its side needs the picture rotated 90 or 270
degrees depending on which way you turned it:

  CircusCharlieHD.exe --rotate 90 --fullscreen
  CircusCharlieHD.exe --rotate 270 --fullscreen

Try F10 while the game runs when you are setting up the cabinet. The
.bat files in this folder already carry these options.

Render resolution
-----------------
The game always draws at 480x640 and scales that picture to the display.
--render 240x320 draws at a low-resolution cabinet's size (low-detail mode
plus integer scaling) and --render native draws at the monitor's full
resolution:

  CircusCharlieHD.exe --render 240x320 --fullscreen
  CircusCharlieHD.exe --render native --fullscreen

Arcade CRT (15 kHz): for progressive 240p use
  CircusCharlieHD.exe --mode 320x240 --rotate 90 --fullscreen
A 15 kHz monitor shows 640x480 interlaced. Never send it a mode it does
not support.

If something goes wrong
-----------------------
Run CircusCharlieHD-console.exe from a command prompt: it is the same game
but prints errors (a missing file under assets\, for example) to the
console.

Jump straight to an event, skipping the boot and attract sequence:

  CircusCharlieHD.exe --event 4

Source code and bug reports: https://github.com/abispac/CircusCharlieHD
