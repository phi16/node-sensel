#include <napi.h>
#include <sensel.h>

#define DeclareWrapper(clName, decls) \
  struct SL##clName : public Napi::ObjectWrap<SL##clName> { \
    static void Init(Napi::Env env, Napi::Object exports) { \
      Napi::Function func = DefineClass(env, "Sensel"## #clName, {}); \
      Napi::FunctionReference* constructor = new Napi::FunctionReference(); \
      *constructor = Napi::Persistent(func); \
      env.SetInstanceData(constructor); \
      exports.Set("Sensel"## #clName, func); \
    } \
    static Napi::Object New(Napi::Env env) { \
      return env.GetInstanceData<Napi::FunctionReference>()->New({}); \
    } \
    SL##clName(const Napi::CallbackInfo& info) : Napi::ObjectWrap<SL##clName>(info) {} \
    decls; \
  };
#define InitWrapper(clName) SL##clName::Init(env, exports)

DeclareWrapper(Handle,
  SENSEL_HANDLE raw;
  SenselFrameData* frame;
  unsigned char leds;
  unsigned short brightness);
DeclareWrapper(Frame, SenselFrameData* raw);

struct Check {
  Napi::Env& env;
  Check(Napi::Env& env): env(env) {}
  void operator+(SenselStatus status) {
    if(status != SENSEL_OK) {
      Napi::Error::New(this->env, "Sensel Error").ThrowAsJavaScriptException();
    }
  }
};
struct CheckExit {
  Napi::Env& env;
  bool& exited;
  CheckExit(Napi::Env& env, bool& exited): env(env), exited(exited) {}
  void operator+(SenselStatus status) {
    if(status != SENSEL_OK) {
      Napi::Error::New(this->env, "Sensel Error").ThrowAsJavaScriptException();
      exited = true;
    }
  }
};
#define CALLEXIT CheckExit(env, exited)+
#define CALL Check(env)+
#define THROW(...) if(exited) return __VA_ARGS__;

Napi::Object slOpen(const Napi::CallbackInfo& info) {
  bool exited = false;
  Napi::Env env = info.Env();
  SENSEL_HANDLE handle;
  SenselFrameData *frame = NULL;
  unsigned char leds = 0;
  unsigned short brightness = 0;
  CALLEXIT senselOpen(&handle); THROW(Napi::Object::New(env));
  CALL senselSetFrameContent(handle, FRAME_CONTENT_CONTACTS_MASK);
  CALL senselGetNumAvailableLEDs(handle, &leds);
  CALL senselGetMaxLEDBrightness(handle, &brightness);
	CALL senselAllocateFrameData(handle, &frame);
  auto o = SLHandle::New(env);
  auto h = Napi::ObjectWrap<SLHandle>::Unwrap(o);
  h->raw = handle;
  h->frame = frame;
  h->leds = leds;
  h->brightness = brightness;
  return o;
}

void slClose(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto h = Napi::ObjectWrap<SLHandle>::Unwrap(info[0].As<Napi::Object>());
  CALL senselClose(h->raw);
}

void slStartScanning(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto h = Napi::ObjectWrap<SLHandle>::Unwrap(info[0].As<Napi::Object>());
  CALL senselStartScanning(h->raw);
}

void slStopScanning(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto h = Napi::ObjectWrap<SLHandle>::Unwrap(info[0].As<Napi::Object>());
  CALL senselStopScanning(h->raw);
}

Napi::Number slNumLEDs(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto h = Napi::ObjectWrap<SLHandle>::Unwrap(info[0].As<Napi::Object>());
  return Napi::Number::New(env, h->leds);
}

void slLED(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto h = Napi::ObjectWrap<SLHandle>::Unwrap(info[0].As<Napi::Object>());
  unsigned int i = info[1].As<Napi::Number>();
  float v = info[2].As<Napi::Number>();
  int b = v * v * h->brightness;
  if(b < 0) b = 0;
  if(b > h->brightness) b = h->brightness;
  CALL senselSetLEDBrightness(h->raw, i, (unsigned short) b);
}

void slLEDs(const Napi::CallbackInfo& info) {
  bool exited = false;
  Napi::Env env = info.Env();
  auto h = Napi::ObjectWrap<SLHandle>::Unwrap(info[0].As<Napi::Object>());
  auto ab = info[1].As<Napi::TypedArray>().ArrayBuffer();
  float* d = reinterpret_cast<float*>(ab.Data());
  int l = ab.ByteLength() / sizeof(float);
  if(h->leds < l) l = h->leds;
  for(int i = 0; i < l; i++) {
    float v = d[i];
    int b = v * v * h->brightness;
    if(b < 0) b = 0;
    if(b > h->brightness) b = h->brightness;
    CALLEXIT senselSetLEDBrightness(h->raw, i, (unsigned short) b); THROW();
  }
}

void slFrame(const Napi::CallbackInfo& info) {
  bool exited = false;
  Napi::Env env = info.Env();
  auto h = Napi::ObjectWrap<SLHandle>::Unwrap(info[0].As<Napi::Object>());
  unsigned int frames = 0;
  CALLEXIT senselReadSensor(h->raw); THROW();
  CALL senselGetNumAvailableFrames(h->raw, &frames);
  Napi::Function cb = info[1].As<Napi::Function>();
  Napi::Number numFrames = Napi::Number::New(env, frames);
  auto o = SLFrame::New(env);
  auto f = Napi::ObjectWrap<SLFrame>::Unwrap(o);
  f->raw = h->frame;
  for(unsigned int i = 0; i < frames; i++) {
    CALL senselGetFrame(h->raw, h->frame);
    Napi::Value v = cb.Call(env.Global(), {
      o,
      Napi::Number::New(env, i),
      numFrames
    });
    if(v.IsBoolean() && v.As<Napi::Boolean>()) {
      break;
    }
  }
}

#define Prop(n, v) o.Set(Napi::String::New(env, #n), Napi::Number::New(env, v))

Napi::Object slSensorInfo(const Napi::CallbackInfo& info) {
  bool exited = false;
  Napi::Env env = info.Env();
  auto h = Napi::ObjectWrap<SLHandle>::Unwrap(info[0].As<Napi::Object>());
  SenselSensorInfo i;
  CALLEXIT senselGetSensorInfo(h->raw, &i); THROW(Napi::Object::New(env));
  auto o = Napi::Object::New(env);
  Prop(max_contacts, i.max_contacts);
  Prop(rows, i.num_rows);
  Prop(cols, i.num_cols);
  Prop(width, i.width);
  Prop(height, i.height);
  return o;
}

Napi::Number slNumContacts(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto f = Napi::ObjectWrap<SLFrame>::Unwrap(info[0].As<Napi::Object>());
  return Napi::Number::New(env, f->raw->n_contacts);
}

void slContact(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto f = Napi::ObjectWrap<SLFrame>::Unwrap(info[0].As<Napi::Object>());
  Napi::Function cb = info[1].As<Napi::Function>();
  unsigned int n = f->raw->n_contacts;
  Napi::Number numContacts = Napi::Number::New(env, n);
  for(unsigned int i = 0; i < n; i++) {
    SenselContact &c = f->raw->contacts[i];
    Napi::Object o = Napi::Object::New(env);
    Prop(id, c.id);
    Prop(state, c.state);
    Prop(x, c.x_pos);
    Prop(y, c.y_pos);
    Prop(force, c.total_force);
    Prop(area, c.area);
    if(c.content_bit_mask & CONTACT_MASK_ELLIPSE) {
      Prop(orientation, c.orientation);
      Prop(major_axis, c.major_axis);
      Prop(minor_axis, c.minor_axis);
    }
    if(c.content_bit_mask & CONTACT_MASK_DELTAS) {
      Prop(delta_x, c.delta_x);
      Prop(delta_y, c.delta_y);
      Prop(delta_force, c.delta_force);
      Prop(delta_area, c.delta_area);
    }
    if(c.content_bit_mask & CONTACT_MASK_BOUNDING_BOX) {
      Prop(min_x, c.min_x);
      Prop(min_y, c.min_y);
      Prop(max_x, c.max_x);
      Prop(max_y, c.max_y);
    }
    if(c.content_bit_mask & CONTACT_MASK_PEAK) {
      Prop(peak_x, c.peak_x);
      Prop(peak_y, c.peak_y);
      Prop(peak_force, c.peak_force);
    }

    Napi::Value v = cb.Call(env.Global(), {
      o,
      Napi::Number::New(env, i),
      numContacts
    });
    if(v.IsBoolean() && v.As<Napi::Boolean>()) {
      break;
    }
  }
}

void slSetContactsMask(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto h = Napi::ObjectWrap<SLHandle>::Unwrap(info[0].As<Napi::Object>());
  unsigned int mask = info[1].As<Napi::Number>();
  CALL senselSetContactsMask(h->raw, mask);
}

#define Declare(name) exports.Set(Napi::String::New(env, #name), Napi::Function::New(env, sl##name))

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  InitWrapper(Handle);
  Declare(Open);
  Declare(Close);
  Declare(StartScanning);
  Declare(StopScanning);
  Declare(NumLEDs);
  Declare(LED);
  Declare(LEDs);
  Declare(Frame);
  Declare(SensorInfo);
  Declare(NumContacts);
  Declare(Contact);
  Declare(SetContactsMask);
  auto contactsMask = Napi::Object::New(env);
  contactsMask.Set(Napi::String::New(env, "NONE"), Napi::Number::New(env, 0));
  contactsMask.Set(Napi::String::New(env, "ELLIPSE"), Napi::Number::New(env, CONTACT_MASK_ELLIPSE));
  contactsMask.Set(Napi::String::New(env, "DELTAS"), Napi::Number::New(env, CONTACT_MASK_DELTAS));
  contactsMask.Set(Napi::String::New(env, "BOUNDING_BOX"), Napi::Number::New(env, CONTACT_MASK_BOUNDING_BOX));
  contactsMask.Set(Napi::String::New(env, "PEAK"), Napi::Number::New(env, CONTACT_MASK_PEAK));
  contactsMask.Set(Napi::String::New(env, "ALL"), Napi::Number::New(env, 0xf));
  exports.Set(Napi::String::New(env, "ContactsMask"), contactsMask);
  auto contactState = Napi::Object::New(env);
  contactState.Set(Napi::String::New(env, "INVALID"), Napi::Number::New(env, CONTACT_INVALID));
  contactState.Set(Napi::String::New(env, "START"), Napi::Number::New(env, CONTACT_START));
  contactState.Set(Napi::String::New(env, "MOVE"), Napi::Number::New(env, CONTACT_MOVE));
  contactState.Set(Napi::String::New(env, "END"), Napi::Number::New(env, CONTACT_END));
  exports.Set(Napi::String::New(env, "ContactState"), contactState);
  return exports;
}

NODE_API_MODULE(sensel, Init)
