import particle as P
import json as J
import image as I
import world as W

core = P.core("PLT-Core", "404738f94e7cb4d723edc11094b761cc3845f34c", true)

P.clear-core-config(core)
led-config = P.configure-core(core, P.write(P.D7, "led"))
button-config = P.configure-core(core, P.digital-read(P.D4, "button"))

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
      P.to-particle(to-led, led-config),
      P.on-particle(on-button, button-config)
    ])
end
