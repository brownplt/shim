import particle as P
import json as J
import string-dict as SD
import image as I
import world as W

opts = [SD.string-dict: "acc", "404738f94e7cb4d723edc11094b761cc3845f34c",
  "core", "PLT-Core"]

P.configure-core([list:
    P.pc-digital-read(P.D4,"button"),
    P.pc-write(P.D0, "led")],
  opts)

data World:
  # bevents : button events
  | world(bevents :: Number)
end

fun on-button(a-world, edata):
  world(a-world.bevents + 1)
end

fun to-led(a-world):
  if num-modulo(a-world.bevents, 8) == 0: some(J.tojson(0))
  else if num-modulo(a-world.bevents, 4) == 0: some(J.tojson(1))
  else: none
  end
end

fun draw-square(value):
  # Start displaying only after 3rd tick
  I.text(tostring(value.bevents), 24, "black")
end

fun to-draw(a-world):
  text = draw-square(a-world)
  I.place-image(text, 100, 100, I.empty-scene(200, 200))
end

fun start():
  W.big-bang(
    # initial state
    world(0),
    # handlers
    [list:
      W.to-draw(to-draw),
      P.to-particle(to-led, "led", opts),
      P.on-particle(on-button, "button", opts)
    ])
end
