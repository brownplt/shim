The following code currently works only with the [Horizon](http://pyret-horizon.herokuapp.com) version of CPO.

```
import particle as P
import json as J
```

The `particle` module contains the tools for working with Particle
cores.  In particular, it allows events on the Particle stream to be
received and sent when working with world programs, and for cores that
have our shim code installed, it allows for remotely configuring pins.

The `json` module provides tools for creating JSON values and
converting between Pyret values and JSON values.  Like many Particle
projects, we have chosen JSON as the serialization method of choice
for data sent via Particle events.

```
import string-dict as SD
opts = [SD.string-dict: "acc", "46aa662f4b2fe1beb009a49709e66bcd498ef85e",
  "core", "SOURCE"]
```

Most of the functions provided by the `particle` API take a dictionary
of options.  Since these functions either expect or allow the same set
of keys in this dictionary, it's often useful to just create the option
dictionary once and just pass it to each function as needed.  

* `"acc" -> String` : your Particle access code for authorization,
* `"core" -> String` : the short name of the core being configured/used,
* `"host" -> String` : the hostname for a private Particle server, if one is being used, and
* `"raw" -> Boolean` : false (the default) if this code is being used with a core that has our shim installed, and true if you want to avoid our encoding of events for use with non-shim cores or with other Particle stream-using programs.

Of these, only `"acc"` and `"core"` are required; the rest are optional.

```
fun config():
  P.configure-core([list:
      P.pc-digital-read(P.D0,"button"),
      P.pc-digital-read(P.D2, "motion"),
      P.pc-analog-read(P.A4, "temperature", P.ait-crosses(200))
      P.pc-write(P.D4, "led")],
    opts)
end
```

`configure-core :: (List<PinConfig>, StringDict) -> Nothing`

A core is configured using the `configure-core` function.  It takes a
list of configuration options and the options dictionary described
above.  Each configuration option takes at least a pin (A0-A7 and
D0-D7, which are exported from the `particle` module) and the name
of the event which is either expected for setting the pin or that will
be sent when the value of the pin changes appropriately.

* `pc-write :: (Pin, String) -> PinConfig` : This configures the given pin to be written to whenever the corresponding event is received by the core.  The data associated with the event is the numeric value to write to the pin.  If the pin is a digital output, then a 0 value is considered LOW and non-zero values considered HIGH.
* `pc-digital-read :: (Pin, String) -> PinConfig` : This configures the given pin to be read as (digital) input by the core, so that an event is sent whenever the pin value changes from LOW to HIGH or from HIGH to LOW.  The data attached to the event is 0 if the value of the pin is (now) LOW, and non-zero (specifically, 1) if it is HIGH.
* `pc-analog-read :: (Pin, String, AnalogInputTrigger) -> PinConfig` : This configures the given pin to be read as (analog) input by the core.

For analog input, whether or not an event is generated depends on an additional argument, which specifies the following:

 * `ait-crosses :: (Number) -> AnalogInputTrigger` : This constructor takes a single numeric value, and an event will be sent whenever the given pin's value crosses the provided threshold.
 * `ait-enters :: (Number, Number) -> AnalogInputTrigger` : This constructor takes two numeric values, a low value and a high value, and an event will be sent whenever the given pin's value crosses into the provided range.
 * `ait-exits :: (Number, Number) -> AnalogInputTrigger` : This constructor takes two numeric values, a low value and a high value, and an event will be sent whenever the given pin's value crosses out of the provided range.

The `Number`s used to create `AnalogInputTrigger`s here should range between 0-4095.

If configuration succeeds, then the on-board LED will turn on and then off again a second later.  If configuration fails, then it will not.  (There are generated events which describe what failed, but currently that information is not received and translated by the Pyret layer.  This is being worked on currently.)

Analog input pins: A0, A1, A2, A3, A4, A5, A6, A7
Digital input pins: D0, D1, D2, D3, D4, D5, D6, D7

Analog output pins: A0, A1, A4, A5, A6, A7
Digital output pins: D0, D1, D2, D3, D4, D5, D6, D7, A2, A3

[Describe how different pins respond to pc-write, due to digital/analog difference.]

```
import image as I
import world as W
import image-structs as IS

data World:
  | world(bevents :: Number, mevents :: Number)
end

fun draw-square(value):
  # Start displaying only after 3rd tick
  text-img = I.text(tostring(value.bevents) + "   " +
    tostring(value.mevents), 24, "black")
  component = 150
  I.place-image(text-img, 50, 50,
    # the 100 is the dimension of the square that varies in color,
    # containing the text
    I.square(100, "solid", IS.color(component, component, component, 1)))
end

fun to-draw(a-world):
  square-img = draw-square(a-world)
  I.place-image(square-img, 100, 100, I.empty-scene(300, 300))      
end

fun to-led(a-world):
  if num-modulo(a-world.bevents,8) == 0: some(J.tojson(0))
  else if num-modulo(a-world.bevents,4) == 0: some(J.tojson(1))
  else: none
  end
end

fun on-button(a-world,edata):
  world(a-world.bevents + 1, a-world.mevents)
end

fun on-motion(a-world,edata):
  world(a-world.bevents, a-world.mevents + 1)
end

fun start():
  W.big-bang(
    # initial state
    world(0,0),
    # handlers
    [list:
      W.to-draw(to-draw),      
      P.to-particle(to-led,"led",opts),
      P.on-particle(on-button,"button",opts),
      P.on-particle(on-motion,"motion",opts)
    ])
end
```

`on-particle :: ((World, JSON) -> World, String, StringDict) -> WorldConfig`

`to-particle :: ((World) -> Option<JSON>, String, StringDict) -> WorldConfig`

In order to incorporate use of the Particle stream within `world`
programs, there are two new event registration functions.  The
`on-particle` function takes a callback that expects a world and the
`JSON` data associated with a Particle event, an event name, and the options
dictionary described above.  The `to-particle` function also takes the
same three arguments, where its callback expects a world and returns
an `Option<JSON>` value that, if not `none`, causes a Particle
event with the provided name to be emitted.

Again, if `"raw"` is either missing or mapped to `false` in the
options dictionary, then the events that are emitted/received are
encoded to work with our Particle core shim.  If it is mapped to
`true`, then no encoding occurs.
