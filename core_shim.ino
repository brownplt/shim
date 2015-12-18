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

struct pin_config {
  char *event;
  int pin;
  int input; // 0 for output, 1 for input
  int old_value; // used for checking edges
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
 * (event_name1:PN(:[IO]:\d+-\d+)?\s+)
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
        {
          int min = 0, max = 0;
          ret->input = 1;
          i += 2;
          if(is_analog_input(ret->pin) &&
             (sscanf(cur_pos + i, "%u-%u", &min, &max) < 0 ||
              min > 4095 || max > 4095)) {
            free_config(ret);
            Spark.publish("config_bad_range", cur_pos, 60, PRIVATE);
            delay(1000);
            return NULL;
          }
          ret->min_value = min;
          ret->max_value = max;
          while(!isspace(cur_pos[i])) { i++; }
          break;
        }
      case 'O':
        ret->input = 0;
        i += 2;
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
        p->old_value = digitalRead(p->pin);
      } else {
        p->old_value = analogRead(p->pin);
      }
    } else {
      pinMode(p->pin, OUTPUT);
    }
    p = p->next;
  }
}

struct pin_config *config = NULL;

void configure(const char *event, const char *edata) {
  struct pin_config *new_config = NULL;
  char data[255];
  if(edata == NULL) {
    delay(1000);
    Spark.publish("config_missing", NULL, 60, PRIVATE);
    return;
  }
  memcpy(data, edata, 255);
  new_config = parse_config(data);
  if(new_config != NULL) {
    free_config(config);
    config = new_config;
    pinMode(D7, OUTPUT);
    digitalWrite(D7, HIGH);
    delay(1000);
    Spark.publish("config_stored", NULL, 60, PRIVATE);
    digitalWrite(D7, LOW);
    setup_config(config);
  }
}

void dump(const char *event, const char *edata) {
  struct pin_config *curr = config;
  char s[255];
  char data[255];
  int i = 0;
  memcpy(data, edata, 255);
  for(curr = config; curr != NULL; curr = curr->next) {
    int name_len = strlen(curr->event);
    // 10 = ':' + 4 digit num + '-' + 4 digit num
    int curr_size = name_len + 6 + 10;
    if(i + curr_size + 1 > 255) {
      delay(1000);
      Spark.publish("config_too_long", NULL, 60, PRIVATE);
    }
    strncpy(s + i, curr->event, strlen(curr->event));
    s[i + name_len] = ':';
    strncpy(s + i + name_len + 1, pin_to_string(curr->pin), 2);
    s[i + name_len + 3] = ':';
    if(curr->input) {
      int printed;
      if(is_analog_input(curr->pin)) {
        printed = sprintf(s + i + name_len + 4, "I:%u-%u\n",
                          curr->min_value, curr->max_value);
        curr_size = i + name_len + 4 + printed;
      } else {
        sprintf(s + i + name_len + 4, "I\n");
        curr_size = i + name_len + 6;
      }
    } else {
      sprintf(s + i + name_len + 4, "O\n");
      curr_size = i + name_len + 6;
    }
    i += curr_size;
  }
  delay(1000);
  Spark.publish("core_dump", s, 60, PRIVATE);
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

void setup() {
  Spark.subscribe("spark/", name_handler);
  Spark.publish("spark/device/name", NULL, 60, PRIVATE);
}

void loop() {
  struct pin_config *curr = config;
  for(; curr != NULL; curr = curr->next) {
    if(curr->input) {
      int curr_val;
      if(!is_analog_input(curr->pin)) {
        curr_val = digitalRead(curr->pin);
        if(curr->old_value != curr_val) {
          curr->old_value = curr_val;
          char s[10];
          sprintf(s, "%d", curr_val);
          Spark.publish(curr->event, s, 60, PRIVATE);
          delay(1000);
        }
      } else {
        curr_val = analogRead(curr->pin);
        if(curr->min_value == curr->max_value) {
          if((curr->old_value > curr->min_value &&
              curr_val <= curr->min_value) ||
             (curr->old_value < curr->min_value &&
              curr_val >= curr->min_value)) {
            char s[10];
            sprintf(s, "%d", curr_val);
            Spark.publish(curr->event, s, 60, PRIVATE);
            delay(1000);
          }
        } else if (curr->min_value < curr->max_value) {
          if(!(curr->old_value >= curr->min_value &&
               curr->old_value <= curr->max_value) &&
             (curr_val >= curr->min_value && curr_val <= curr->max_value)) {
            char s[10];
            sprintf(s, "%d", curr_val);
            Spark.publish(curr->event, s, 60, PRIVATE);
            delay(1000);
          }
        } else {
          if(!(curr->old_value < curr->min_value ||
               curr->old_value > curr->max_value) &&
             (curr_val < curr->min_value || curr_val > curr->max_value)) {
            char s[10];
            sprintf(s, "%d", curr_val);
            Spark.publish(curr->event, s, 60, PRIVATE);
            delay(1000);
          }
        }
      }
      curr->old_value = curr_val;
    }
  }
}
