int tinkerRead(String);
int tinkerWrite(String);

int string_to_pin(const char *str) {
  switch(str[0]) {
  case 'A':
    switch(str[1]) {
    case '0': return A0;
    case '1': return A1;
    case '2': return A2;
    case '3': return A3;
    case '4': return A4;
    case '5': return A5;
    case '6': return A6;
    case '7': return A7;
    default: return -1;
    }
    break;
  case 'D':
    switch(str[1]) {
    case '0': return D0;
    case '1': return D1;
    case '2': return D2;
    case '3': return D3;
    case '4': return D4;
    case '5': return D5;
    case '6': return D6;
    case '7': return D7;
    default: return -1;
    }
    break;
  default: return -1;
  }
}

const char *pin_to_string(int pin) {
  switch(pin) {
  case A0: return "A0";
  case A1: return "A1";
  case A2: return "A2";
  case A3: return "A3";
  case A4: return "A4";
  case A5: return "A5";
  case A6: return "A6";
  case A7: return "A7";
  case D0: return "D0";
  case D1: return "D1";
  case D2: return "D2";
  case D3: return "D3";
  case D4: return "D4";
  case D5: return "D5";
  case D6: return "D6";
  case D7: return "D7";
  default: return NULL;
  }
}

int pin_to_index(int pin) {
  switch(pin) {
  case A0:
  case D0: return 0;
  case A1:
  case D1: return 1;
  case A2:
  case D2: return 2;
  case A3:
  case D3: return 3;
  case A4:
  case D4: return 4;
  case A5:
  case D5: return 5;
  case A6:
  case D6: return 6;
  case A7:
  case D7: return 7;
  default: return -1;
  }
}

int digital_index_to_pin(int idx) {
  switch(idx) {
  case 0: return D0;
  case 1: return D1;
  case 2: return D2;
  case 3: return D3;
  case 4: return D4;
  case 5: return D5;
  case 6: return D6;
  case 7: return D7;
  default: return -1;
  }
}

int analog_index_to_pin(int idx) {
  switch(idx) {
  case 0: return A0;
  case 1: return A1;
  case 2: return A2;
  case 3: return A3;
  case 4: return A4;
  case 5: return A5;
  case 6: return A6;
  case 7: return A7;
  default: return -1;
  }
}

int is_analog_input(int pin) {
  switch(pin) {
  case A0:
  case A1:
  case A2:
  case A3:
  case A4:
  case A5:
  case A6:
  case A7:
    return 1;
  default:
    return 0;
  }
}

int is_analog_output(int pin) {
  switch(pin) {
  case A0:
  case A1:
  case A4:
  case A5:
  case A6:
  case A7:
    return 1;
  default:
    return 0;
  }
}

int digital_cache[8] = { LOW };
int analog_cache[8] = { 0 };

struct pin_config {
  char *event;
  int pin;
  int input; // 0 for output, 1 for input
  // The following two are used for analog input ranges. If you compare them:
  // ==   ====>   a single value, send events when result crosses value
  // <    ====>   a range, send events when value enters range
  // >    ====>   a range, send events when value exits range
  // (You can configure the same pin multiple times, so can do latter two
  // to get entrance/exit, e.g.)
  int min_value;
  int max_value;
  struct pin_config* next;
};

void free_config(struct pin_config *p) {
  while(p != NULL) {
    struct pin_config *prev = p;
    free(p->event);
    p = p->next;
    free(prev);
  }
}

/* Pin configs should be in the following format:
 * (event_name:PN(:[IO]:\d+-\d+)?\s+)
 * where event_name is the event that should be subscribed as a string
 * and PN is the pin name (that is, [AD][0-7]).  The I/O is for input/output
 * specification, and the numeric range should only appear if the pin is an
 * input.
 */
struct pin_config* parse_config(const char *str) {
  struct pin_config *ret = NULL;
  const char *cur_pos = str;
  while(*cur_pos != '\0') {
    struct pin_config *prev = ret;
    unsigned int i = 0;
    for(i = 0; cur_pos[i] != ':'; i++) {
      if(cur_pos[i] == '\0') {
        delay(1000);
        Spark.publish("config_unterminated_event", cur_pos, 60, PRIVATE);
        free_config(prev);
        return NULL;
      }
      if(i > 63) {
        delay(1000);
        Spark.publish("config_event_too_long", cur_pos, 60, PRIVATE);
        free_config(prev);
        return NULL;
      }
    }
    ret = (struct pin_config*)malloc(sizeof(struct pin_config));
    if(ret == NULL) {
      delay(1000);
      Spark.publish("config_malloc_error", cur_pos, 60, PRIVATE);
      free_config(prev);
      return NULL;
    }
    ret->next = prev;
    ret->event = (char *)malloc((i + 1) * sizeof(char));
    if(ret->event == NULL) {
      free_config(ret);
      delay(1000);
      Spark.publish("config_malloc_error", cur_pos, 60, PRIVATE);
      return NULL;
    }
    ret->event[i] = '\0';
    strncpy(ret->event, cur_pos, i);
    i++; /* skip past the colon */
    ret->pin = string_to_pin(cur_pos + i);
    if(ret->pin == -1) {
      free_config(ret);
      delay(1000);
      Spark.publish("config_bad_pin", cur_pos + i, 60, PRIVATE);
      return NULL;
    }
    i += 2; /* Skip past the pin */
    if(cur_pos[i] == ':') {
      switch(cur_pos[i+1]) {
      case 'I':
        ret->input = 1;
        i += 2; // skip past ":I"
        if(is_analog_input(ret->pin)) {
          i++; // skip past ":"
          if(sscanf(cur_pos + i, "%d", &ret->min_value) <= 0 ||
             ret->min_value < 0 || ret->min_value > 4095) {
            free_config(ret);
            delay(1000);
            Spark.publish("config_bad_min", cur_pos, 60, PRIVATE);
            return NULL;
          }
          while(isdigit(cur_pos[i])) { i++; }
          i++; // Skip past '-'
          if(sscanf(cur_pos + i, "%d", &ret->max_value) <= 0 ||
             ret->min_value < 0 || ret->max_value > 4095) {
            free_config(ret);
            delay(1000);
            Spark.publish("config_bad_max", cur_pos, 60, PRIVATE);
            return NULL;
          }
          while(isdigit(cur_pos[i])) { i++; }
        }
        break;
      case 'O':
        ret->input = 0;
        i += 2; // skip past ":O"
        break;
      default:
        free_config(ret);
        delay(1000);
        Spark.publish("config_bad_spec", cur_pos, 60, PRIVATE);
        return NULL;
      }
    } else {
      ret->input = 0;
    }
    if(!isspace(cur_pos[i])) {
        free_config(ret);
        delay(1000);
        Spark.publish("config_delim_error", cur_pos, 60, PRIVATE);
        return NULL;
    }
    while(isspace(cur_pos[i])) { i++; }
    cur_pos += i;
  }
  return ret;
}

void setup_config(struct pin_config *p) {
  while(p != NULL) {
    if(p->input) {
      pinMode(p->pin, INPUT_PULLDOWN);
      if(!is_analog_input(p->pin)) {
        digital_cache[pin_to_index(p->pin)] = digitalRead(p->pin);
      } else {
        analog_cache[pin_to_index(p->pin)] = analogRead(p->pin);
      }
    } else {
      pinMode(p->pin, OUTPUT);
    }
    p = p->next;
  }
}

struct pin_config *config = NULL;

void configure(const char *event, const char *edata) {
  struct pin_config *new_config_item = NULL;
  char data[255];
  if(edata == NULL || edata[0] == '\0') {
    free_config(config);
    config = NULL;
    delay(1000);
    Spark.publish("config_cleared", NULL, 60, PRIVATE);
    return;
  }
  memcpy(data, edata, 255);
  new_config_item = parse_config(data);
  if(new_config_item != NULL) {
    struct pin_config *prev = new_config_item;
    for(; prev->next != NULL; prev = prev->next) {}
    prev->next = config;
    config = new_config_item;
    pinMode(D7, OUTPUT);
    digitalWrite(D7, HIGH);
    setup_config(config);
    delay(50);
    digitalWrite(D7, LOW);
  }
}

void dump(const char *event, const char *edata) {
  emit_dump("config_dump", config);
}

int ext_dump(String arg) {
  emit_dump("config_dump", config);
  return 0;
}

void emit_dump(const char *ename, struct pin_config *config) {
  struct pin_config *curr = config;
  char s[255] = {0};
  int i = 0;
  for(curr = config; curr != NULL; curr = curr->next) {
    int name_len = strlen(curr->event);
    // 10 = ':' + 4 digit num + '-' + 4 digit num -> worst case length
    int curr_size = name_len + 6 + 10;
    if(i + curr_size + 1 > 255) {
      delay(1000);
      Spark.publish("config_too_long", NULL, 60, PRIVATE);
    }
    strncpy(s + i, curr->event, name_len);
    i += name_len;
    s[i] = ':';
    i++;
    strncpy(s + i, pin_to_string(curr->pin), 2);
    i += 2;
    s[i] = ':';
    i++;
    if(curr->input) {
      int printed;
      if(is_analog_input(curr->pin)) {
        printed = sprintf(s + i, "I:%d-%d",
                          curr->min_value, curr->max_value);
        i += printed;
      } else {
        sprintf(s + i, "I");
        i++;
      }
    } else {
      sprintf(s + i, "O");
      i++;
    }
    sprintf(s + i, "\n");
    i++;
  }
  delay(1000);
  Spark.publish(ename, s, 60, PRIVATE);
}

/* Meta-event handler. String in data should be of the form:
 * event_name:actual_data
 */
void event_handler(const char *event, const char *edata) {
  struct pin_config *curr = config;
  char data[255];
  const char *real_data = data;
  unsigned int esize = 0;
  unsigned int i = 0;

  memcpy(data, edata, 255);
  while(*real_data != ':' && *real_data != '\0') { ++real_data; }
  esize = real_data - data;
  if(*real_data == ':') { ++real_data; }
  if(sscanf(real_data, "%u", &i) <= 0 || i > 255) {
    delay(1000);
    Spark.publish("event_bad_data", real_data, 60, PRIVATE);
    return;
  }

  for(curr = config; curr != NULL; curr = curr->next) {
    if(!strncmp(data, curr->event, esize) && !curr->input) {
      if(is_analog_output(curr->pin)) {
        analogWrite(curr->pin, i);
      } else {
        if(i == 0) {
          digitalWrite(curr->pin, LOW);
          return;
        } else {
          digitalWrite(curr->pin, HIGH);
          return;
        }
      }
    }
  }

  if(curr == NULL) {
    delay(1000);
    Spark.publish("event_unknown", data, 60, PRIVATE);
  }
}

void name_handler(const char *event, const char *data) {
  char s[64];
  unsigned i = 0;
  strncpy(s, data, 63);
  while(s[i] != '\0' && i < 56) { i++; } // include space for suffixes below
  if(i >= 56) { Spark.publish("name_too_long", NULL, 60, PRIVATE); }
  Spark.publish("name_received", s, 60, PRIVATE);
  strcpy(s + i, "_config");
  Spark.subscribe(s, configure, MY_DEVICES);
  strcpy(s + i, "_event");
  Spark.subscribe(s, event_handler, MY_DEVICES);
  strcpy(s + i, "_dump");
  Spark.subscribe(s, dump, MY_DEVICES);
}

int dump_caches(String data) {
 char s[255];
 int i, printed = 0;
 printed += sprintf(s + printed, "{digital:[");
 for(i = 0; i < 8; i++) {
   printed += sprintf(s + printed, "%d,", digital_cache[i]);
 }
 printed += sprintf(s + printed, "],analog:[");
 for(i = 0; i < 8; i++) {
   printed += sprintf(s + printed, "%d,", analog_cache[i]);
 }
 printed += sprintf(s + printed, "]}");
 delay(1000);
 Spark.publish("cache_dump", s, 60, PRIVATE);
 return 0;
}

void setup() {
  Spark.function("read", tinkerRead);
  Spark.function("write", tinkerWrite);
  Spark.function("dump_config", ext_dump);
  Spark.function("dump_caches", dump_caches);
  Spark.subscribe("spark/", name_handler);
  Spark.publish("spark/device/name", NULL, 60, PRIVATE);
}

void loop() {
  struct pin_config *curr = config;
  int analog_vals[8] = {0};
  int digital_vals[8] = {LOW};
  int i;
  bool published = false;
  for(i = 0; i < 8; i++) {
    analog_vals[i] = analogRead(analog_index_to_pin(i));
    digital_vals[i] = digitalRead(digital_index_to_pin(i));
  }
  for(; curr != NULL; curr = curr->next) {
    if(curr->input) {
      char s[10] = { 0 };
      int pin_index = pin_to_index(curr->pin);
      if(!is_analog_input(curr->pin)) {
        int curr_val = digital_vals[pin_index];
        int old_val = digital_cache[pin_index];
        if(old_val != curr_val) {
          old_val = curr_val;
          sprintf(s, "%d", curr_val);
          Spark.publish(curr->event, s, 60, PRIVATE);
          published = true;
        }
      } else {
        int curr_val = analog_vals[pin_index];
        int old_val = analog_cache[pin_index];
        if(curr->min_value == curr->max_value) {
          if((old_val > curr->min_value && curr_val <= curr->min_value) ||
             (old_val < curr->min_value && curr_val >= curr->min_value)) {
            sprintf(s, "%d", curr_val);
            Spark.publish(curr->event, s, 60, PRIVATE);
            published = true;
          }
        } else if (curr->min_value < curr->max_value) {
          if((old_val  <  curr->min_value || old_val  >  curr->max_value) &&
             (curr_val >= curr->min_value && curr_val <= curr->max_value)) {
            sprintf(s, "%d", curr_val);
            Spark.publish(curr->event, s, 60, PRIVATE);
            published = true;
          }
        } else {
          // Remember, min and max are INVERTED for this case!
          if((old_val  >= curr->max_value && old_val  <= curr->min_value) &&
             (curr_val <  curr->max_value || curr_val >  curr->min_value)) {
            sprintf(s, "%d", curr_val);
            Spark.publish(curr->event, s, 60, PRIVATE);
            published = true;
          }
        }
      }
    }
  }
  for(i = 0; i < 8; i++) {
    analog_cache[i] = analog_vals[i];
    digital_cache[i] = digital_vals[i];
  }
  if(published) { delay(1000); }
}

// The following is taken with changes from Particle's Tinker app.
/*******************************************************************************
 * Function Name  : tinkerRead
 * Description    : Reads the value of a given pin
 * Input          : Pin
 * Output         : None.
 * Return         : Value of the pin in INT type
                    Returns a negative number on failure
 *******************************************************************************/
int tinkerRead(String cmd) {
        const char *command = cmd.c_str();
        int pin = string_to_pin(command);
        if(pin < 0) { return -1; }

        pinMode(pin, INPUT_PULLDOWN);
        if(is_analog_input(pin)) {
          return analogRead(pin);
        } else {
          return digitalRead(pin);
        }
}

/*******************************************************************************
 * Function Name  : tinkerWrite
 * Description    : Sets the specified pin HIGH or LOW if digital,
 *                  or to numeric value if analog
 * Input          : Pin and value
 * Output         : None.
 * Return         : 1 on success and a negative number on failure
 *******************************************************************************/
int tinkerWrite(String cmd)
{
        const char *command = cmd.c_str();
	int value = 0;
	//convert ascii to integer
	int pinNumber = string_to_pin(command);
	//Sanity check to see if the pin numbers are within limits
	if (pinNumber < 0) return -1;

	const char *valstr = command + 3;

        if(is_analog_output(pinNumber)) {
          if(sscanf(valstr, "%d", &value) <= 0 || value < 0 || value > 4095) {
            return -2;
          }
          pinMode(pinNumber, OUTPUT);
          analogWrite(pinNumber, value);
        } else {
          if(!strncmp(valstr, "HIGH", 4)) value = 1;
	  else if(!strncmp(valstr, "LOW", 3)) value = 0;
	  else return -2;
          pinMode(pinNumber, OUTPUT);
          digitalWrite(pinNumber, value);
        }
        return 1;

}
