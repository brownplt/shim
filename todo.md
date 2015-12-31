## TODO
- [x] Instead of string-dict, consolidate information about cores
  into their own datatype
  - name
  - access code
  - shim?
- [x] Can represent use of stream without specific core in mind as
  separate version of datatype.
- [x] Change configure-core to configure each part separately, return
  a programmatic value as return that can be fed into handler functions
- [x] Add direct, imperative commands for sending events on stream
  - [ ] receiving events with callback
- [ ] Make on/to-particle work simply
  - No option return for to-particle, always send
  - Convert to/from json automatically where needed
  - Can add more complicated interfaces later
- [ ] Add specifics to documentation on how to find stuff like core short name,
  access code, etc. via web and via command line
- [ ] Mention `subscribe mine` as debugging source
- [ ] Error handling, e.g., authentication errors
  - include things like "not getting a response from the core on config in
    XX seconds"
  - to-particle needs to check the result from the handler more carefully
- [ ] Add a polling per x seconds for digital and analog inputs.
- [x] Change AnalogInputTrigger/ait- to a better name, like AnalogCondition
  (or if we need digital conditions for polling, something more general).

## Possible features:
 - [ ] Core-to-core communication via events directly, configuration?
 - [ ] Use VoodooSpark to directly control cores instead of using event stream?
   http://voodoospark.me/#api
