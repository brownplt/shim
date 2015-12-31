## A Quick Guide to using Particle cores with code.pyret.org

*Important Note*: The following code currently works only with the [Horizon](http://pyret-horizon.herokuapp.com) version of CPO.

The code for the shim to put onto Particle cores is in this repository as [core_shim.ino](https://raw.githubusercontent.com/sstrickl/shim/master/core_shim.ino).  Information about flashing this shim onto your device is available at the [Web IDE](https://docs.particle.io/guide/getting-started/build/core/) documentation on [Particle](http://particle.io)'s site.

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
for data sent via Particle events.  The module provides the following
function for converting Pyret values into `json`:

`tojson :: (Any) -> JSON`

JSON values themselves consist of a datatype with the following
constructors:

* `j-null :: JSON`: Represents the JavaScript value `null`.
* `j-bool :: (Boolean) -> JSON`: Represents a JavaScript boolean.
* `j-num :: (Number) -> JSON`: Represents a JavaScript number.
* `j-str :: (String) -> JSON`: Represents a JavaScript string.
* `j-arr :: (List<JSON>) -> JSON`: Represents a JavaScript array.
* `j-obj :: (StringDict<JSON>) -> JSON`: Represents a JavaScript object.

JSON values also contain the following methods:

* `native :: () -> Any`: Converts the JSON value into the equivalent Pyret value.  This conversion is deep, in that any contents of the value is converted as well.
* `serialize :: () -> String`: Converts the JSON value into a String representation that can be read by any standard JSON parser.

### Shared Configuration Information

Most of the functions provided by the `particle` API take a dictionary
of options, represented as a `StringDict`.  Since these functions
either expect or allow the same set of keys in this dictionary, it's
often useful to just create the option dictionary once and just pass
it to each function as needed.  In the following example, we create an
options dictionary that contains a Particle access code, used for
authentication, and the name of the core with which we'll be working:

```
import string-dict as SD
opts = [SD.string-dict: "acc", "46aa662f4b2fe1beb009a49709e66bcd498ef85e",
  "core", "SOURCE"]
```
The currently available set of keys are:

* `"acc" -> String` : your Particle access token for authorization,
* `"core" -> String` : the short name of the core being configured/used,
* `"host" -> String` : the hostname for a private Particle server, if one is being used, and
* `"raw" -> Boolean` : false (the default) if this code is being used with a core that has our shim installed, and true if you want to avoid our encoding of events for use with non-shim cores or with other Particle stream-using programs.

Of these, only `"acc"` and `"core"` are required; the rest are optional.

### Core Configuration with the Shim

To configure the core, we'll need to provide a mapping from onboard
pins to events that should be generated when those pins match certain
conditions.  To do this,  we provide the function `configure-core`:

`configure-core :: (List<PinConfig>, StringDict) -> Nothing`

It takes a list of `PinConfig` configuration options, which we describe
shortly, and the general options
`StringDict` described above.  Each configuration option takes at
least a pin and the name of the event which is either expected for
setting the pin or that will be sent when the value of the pin changes
appropriately.  The pins are values exported from the `particle`
module that are bound to their corresponding onboard names, so `A0`, `A1`,
... `A7`, `D0`, `D1`, ... `D7`.

For example, the function call below configures the core
to generate a `"button"` event when pin D4 goes
from `LOW` to `HIGH` and vice versa.  Finally,
the core will watch for `"led"` events, and set the value of the D0
pin according to the value attached to the event.

```
P.configure-core([list:
    P.pc-digital-read(P.D4,"button"),
    P.pc-write(P.D0, "led")],
  opts)
```

The `PinConfig` datatype contains the following constructors:

* `pc-write :: (Pin, String) -> PinConfig` : This configures the given pin to be written to whenever the corresponding event is received by the core.  The data associated with the event is the numeric value to write to the pin.  If the pin is a digital output, then a 0 value is considered LOW and non-zero values considered HIGH.  Unlike reading, whether a read is digital or analog is automatically determined from the pin (since no extra information is needed in either case).
* `pc-digital-read :: (Pin, String) -> PinConfig` : This configures the given pin to be read as (digital) input by the core, so that an event is sent whenever the pin value changes from LOW to HIGH or from HIGH to LOW.  The data attached to the event is 0 if the value of the pin is (now) LOW, and non-zero (specifically, 1) if it is HIGH.
* `pc-analog-read :: (Pin, String, AnalogInputTrigger) -> PinConfig` : This configures the given pin to be read as (analog) input by the core.  The value of the pin when the event is generated is attached to the event.

For analog input, whether or not an event is generated based on the value of the pin depends on an additional `AnalogInputTrigger` argument, which specifies the condition under which the event will fire.  Here are the constructors for `AnalogInputTrigger` values:

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

To work with events, we'll need two ways of registering callbacks: one for
receiving events from the Particle event stream,

`on-particle :: ((World, JSON) -> World, String, StringDict) -> WorldConfig`

and one for sending events to the Particle event stream,

`to-particle :: (World -> Option<JSON>, String, StringDict) -> WorldConfig`

The `on-particle` function takes a callback that expects a world and
the `JSON` data associated with a Particle event, an event name, and
the general options `StringDict` described in the Shared Configuration
Information section above.  The `to-particle` function also takes the
same three arguments, where its callback expects a world and returns
an `Option<JSON>` value that, if not `none`, causes a Particle event
with the provided name to be emitted.  If `"raw"` is either missing or
mapped to `false` in the options dictionary, then the events that are
emitted/received are encoded to work with our Particle core shim.  If
it is mapped to `true`, then no encoding occurs.

To illustrate the use of these registration functions, we'll write a
world program that reads from the `"button"` pin on `D4` that we configured
above and writes to the `"led"` pin on `D0` whenever a certain number of
button presses have occurred.  First, we'll need to
create a data structure to describe the current state of the world.

```
data World:
  # bevents : button events
  | world(bevents :: Number)
end
```

Next, we'll create a function that we will later install as a handler to
run when its corresponding event is received and increment the
appropriate count.  That is, this function will be installed using
the `on-particle` registration function.

```
fun on-button(a-world, edata):
  world(a-world.bevents + 1)
end
```

For the LED, we'll create a handler function that tells the core to toggle
the LED every 4 button presses.  That is, this function will be installed
via the `to-particle` registration function to send events to the core. When
the number of button presses is not a multiple of 4, we'll signal that
no event should be sent by using the `none` value from `Option`. Here,
a value of `0` corresponds to the `LOW` value used by digital writes
in Arduino C, and a non-zero value corresponds to `HIGH`.

```
fun to-led(a-world):
  if num-modulo(a-world.bevents, 8) == 0: some(J.tojson(0))
  else if num-modulo(a-world.bevents, 4) == 0: some(J.tojson(1))
  else: none
  end
end
```

Having some visual feedback other than the LED is also useful, so we'll
display the 'bevents' count on the canvas.

```
import image as I

fun draw-square(value):
  # Start displaying only after 3rd tick
  I.text(tostring(value.bevents), 24, "black")
end

fun to-draw(a-world):
  text = draw-square(a-world)
  I.place-image(text, 100, 100, I.empty-scene(200, 200))
end
```

Finally, we'll create a function that, when run, starts up the world with
an initial state of no button presses and installs the
handlers using `on-particle` and `to-particle` appropriately.

```
import world as W

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
```
The complete Pyret program for this example is in [shim-demo.arr](./shim-demo.arr).
