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

DeclareWrapper(Handle, SENSEL_HANDLE raw; SenselFrameData* frame; unsigned short brightness);
DeclareWrapper(Frame, SenselFrameData* raw);

struct Check {
  Napi::Env& env;
  Check(Napi::Env& env): env(env) {}
  int operator+(SenselStatus status) {
    if(status != SENSEL_OK) {
      throw Napi::Error::New(this->env, "Sensel Error");
    }
    return 0;
  }
};
#define CALL Check(env)+

Napi::Object slOpen(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  SENSEL_HANDLE handle;
  SenselFrameData *frame = NULL;
  unsigned short brightness = 0;
  CALL senselOpen(&handle);
  CALL senselSetFrameContent(handle, FRAME_CONTENT_CONTACTS_MASK);
  CALL senselGetMaxLEDBrightness(handle, &brightness);
  if(brightness != 100) {
    CALL senselSetLEDBrightness(handle, 0, brightness);
  }
	CALL senselAllocateFrameData(handle, &frame);
  auto o = SLHandle::New(env);
  auto h = Napi::ObjectWrap<SLHandle>::Unwrap(o);
  h->raw = handle;
  h->frame = frame;
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

void slLED(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto h = Napi::ObjectWrap<SLHandle>::Unwrap(info[0].As<Napi::Object>());
  unsigned int i = info[1].As<Napi::Number>();
  float v = info[2].As<Napi::Number>();
  int b = v * h->brightness;
  if(b < 0) b = 0;
  if(b > h->brightness) b = h->brightness;
  CALL senselSetLEDBrightness(h->raw, i, (unsigned short) b);
}

void slFrame(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto h = Napi::ObjectWrap<SLHandle>::Unwrap(info[0].As<Napi::Object>());
  unsigned int frames = 0;
  CALL senselReadSensor(h->raw);
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
    if(v.As<Napi::Boolean>()) {
      break;
    }
  }
}

Napi::Number slNumContacts(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto f = Napi::ObjectWrap<SLFrame>::Unwrap(info[0].As<Napi::Object>());
  return Napi::Number::New(env, f->raw->n_contacts);
}

#define Declare(name) exports.Set(Napi::String::New(env, #name), Napi::Function::New(env, sl##name))

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  InitWrapper(Handle);
  Declare(Open);
  Declare(Close);
  Declare(StartScanning);
  Declare(StopScanning);
  Declare(LED);
  Declare(Frame);
  Declare(NumContacts);
  return exports;
}

NODE_API_MODULE(sensel, Init)
