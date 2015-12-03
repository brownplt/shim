## A Quick Guide to using Particle cores with code.pyret.org

*Important Note*: The following code currently works only with the [Horizon](http://pyret-horizon.herokuapp.com) version of CPO.

### Modules

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

### Shared Configuration Information

Most of the functions provided by the `particle` API take a dictionary
of options, represented as a `StringDict`.  Since these functions
either expect or allow the same set of keys in this dictionary, it's
often useful to just create the option dictionary once and just pass
it to each function as needed.  Here, we'll create an options
dictionary that contains a Particle access code, used for
authentication, and the name of the core with which we'll be working:

```
import string-dict as SD
opts = [SD.string-dict: "acc", "46aa662f4b2fe1beb009a49709e66bcd498ef85e",
  "core", "SOURCE"]
```
The currently available set of keys are:

* `"acc" -> String` : your Particle access code for authorization,
* `"core" -> String` : the short name of the core being configured/used,
* `"host" -> String` : the hostname for a private Particle server, if one is being used, and
* `"raw" -> Boolean` : false (the default) if this code is being used with a core that has our shim installed, and true if you want to avoid our encoding of events for use with non-shim cores or with other Particle stream-using programs.

Of these, only `"acc"` and `"core"` are required; the rest are optional.

### Core Configuration with the Shim

To configure the core, we'll need to provide a mapping from onboard
pins to events that should be generated when those pins match certain
conditions.  For example, the function call below configures the core
to generate `"button"` and `"motion"` events when pins D0 and D2 goes
from `LOW` to `HIGH` and vice versa, respectively.  It also will
generate a `"temperature"` event whenever the A4 pin either goes from
below the value 200 to above the value 200 or vice versa.  Finally,
the core will watch for `"led"` events, and set the value of the D4
pin according to the value attached to the event.

```
P.configure-core([list:
    P.pc-digital-read(P.D0,"button"),
    P.pc-digital-read(P.D2, "motion"),
    P.pc-analog-read(P.A4, "temperature", P.ait-crosses(200))
    P.pc-write(P.D4, "led")],
  opts)
```

`configure-core :: (List<PinConfig>, StringDict) -> Nothing`

A core is configured using the `configure-core` function.  It takes a
list of `PinConfig` configuration options and the general options
`StringDict` described above.  Each configuration option takes at
least a pin and the name of the event which is either expected for
setting the pin or that will be sent when the value of the pin changes
appropriately.  The pins are values exported from the `particle`
module that are bound to their corresponding onboard names, so `A0`, `A1`,
... `A7`, `D0`, `D1`, ... `D7`.

* `pc-write :: (Pin, String) -> PinConfig` : This configures the given pin to be written to whenever the corresponding event is received by the core.  The data associated with the event is the numeric value to write to the pin.  If the pin is a digital output, then a 0 value is considered LOW and non-zero values considered HIGH.  Unlike reading, whether a read is digital or analog is automatically determined from the pin (since no extra information is needed in either case).
* `pc-digital-read :: (Pin, String) -> PinConfig` : This configures the given pin to be read as (digital) input by the core, so that an event is sent whenever the pin value changes from LOW to HIGH or from HIGH to LOW.  The data attached to the event is 0 if the value of the pin is (now) LOW, and non-zero (specifically, 1) if it is HIGH.
* `pc-analog-read :: (Pin, String, AnalogInputTrigger) -> PinConfig` : This configures the given pin to be read as (analog) input by the core.  The value of the pin when the event is generated is attached to the event.

For analog input, whether or not an event is generated based on the value of the pin depends on an additional `AnalogInputTrigger` argument, which specifies the condition under which the event will fire:

 * `ait-crosses :: (Number) -> AnalogInputTrigger` : This constructor takes a single numeric value, and an event will be sent whenever the given pin's value crosses the provided threshold.
 * `ait-enters :: (Number, Number) -> AnalogInputTrigger` : This constructor takes two numeric values, a low value and a high value, and an event will be sent whenever the given pin's value crosses into the provided range.
 * `ait-exits :: (Number, Number) -> AnalogInputTrigger` : This constructor takes two numeric values, a low value and a high value, and an event will be sent whenever the given pin's value crosses out of the provided range.

The `Number`s used to create `AnalogInputTrigger`s here should range between 0 and 4095 (inclusively).

If configuration succeeds, then the on-board LED to the right of the power connection will turn on and then off again a second later.  If configuration fails, then it will not.  (There are generated events which describe what failed, but currently that information is not received and translated by the Pyret layer.  This is being worked on currently.)

Each pin is limited to either being analog or digital input or output.
Not every digital input pin also allows for digital output, and
similarly for analog input and analog output.  The list of pins that
can be used with digital (or analog) input (or output) are provided
below:

- Pins with analog input: `A0`, `A1`, `A2`, `A3`, `A4`, `A5`, `A6`, `A7`
- Pins with digital input: `D0`, `D1`, `D2`, `D3`, `D4`, `D5`, `D6`, `D7`
- Pins with analog output: `A0`, `A1`, `A4`, `A5`, `A6`, `A7`
- Pins with digital output: `A2`, `A3`, `D0`, `D1`, `D2`, `D3`, `D4`, `D5`, `D6`, `D7`

### Creating a World Program that uses the Particle Stream

To illustrate using the Particle Stream from a Pyret program that uses
the world module, we'll write a program that reads from two of the
pins we configured above (`"motion"` on `D0` and `"button"` on `D2`)
and writes to the `"led"` pin on `D4` whenever a certain number of
motion events or button presses have occurred.  First, we'll need to
create a data structure to describe the current state of the world.

```
data World:
  # bevents : button events, mevents : motion events
  | world(bevents :: Number, mevents :: Number)
end
```

Next, we'll create functions that we will later install as handlers to
run when their corresponding event is received and increment the
appropriate count.

```
fun on-button(a-world,edata):
  world(a-world.bevents + 1, a-world.mevents)
end

fun on-motion(a-world,edata):
  world(a-world.bevents, a-world.mevents + 1)
end
```

For the LED, we'll switch it on and off every 4 button presses. When
the number of button presses is not a multiple of 4, we'll signal that
no event should be sent by using the `none` value from `Option`. Here,
a value of `0` corresponds to the `LOW` value used by digital writes
in Arduino C, and a non-zero value corresponds to `HIGH`.

```
fun to-led(a-world):
  if num-modulo(a-world.bevents,8) == 0: some(J.tojson(0))
  else if num-modulo(a-world.bevents,4) == 0: some(J.tojson(1))
  else: none
  end
end
```

Having some visual feedback other than the LED is also useful, so we'll
display a square on the canvas that includes the two counts.

```
import image as I
import image-structs as IS

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
```

Finally, we'll create a function that, when run, starts up the world with
an initial state of no button presses or motion events and installs the
handlers appropriately.

```
import world as W

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

As illustrated above, there are two new event registration functions
to add use of the Particle stream within `world` programs.  The
`on-particle` function takes a callback that expects a world and the
`JSON` data associated with a Particle event, an event name, and the
general options `StringDict` described in the Shared Configuration
Information section above.  The `to-particle` function also takes the
same three arguments, where its callback expects a world and returns
an `Option<JSON>` value that, if not `none`, causes a Particle event
with the provided name to be emitted.

Again, if `"raw"` is either missing or mapped to `false` in the
options dictionary, then the events that are emitted/received are
encoded to work with our Particle core shim.  If it is mapped to
`true`, then no encoding occurs.
