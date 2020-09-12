const sensel = require('bindings')('sensel');

class SenselFrame {
  constructor(frame) {
    this.frame = frame;
    this.numContacts = sensel.NumContacts(frame);
  }
  contact(cb) {
    sensel.Contact(this.frame, (c,i,n)=>{
      return cb(c,i,n);
    });
  }
}

class SenselDevice {
  constructor(handle) {
    this.handle = handle;
    this.sensorInfo = sensel.SensorInfo(handle);
    this.numLEDs = sensel.NumLEDs(handle);
  }
  close() {
    sensel.Close(this.handle);
  }
  startScanning() {
    sensel.StartScanning(this.handle);
  }
  stopScanning() {
    sensel.StopScanning(this.handle);
  }
  setContactsMask(mask) {
    sensel.SetContactsMask(this.handle, mask);
  }
  frame(cb) {
    sensel.Frame(this.handle, (f,i,n)=>{
      return cb(new SenselFrame(f), i, n);
    });
  }
  LED(i, v) {
    sensel.LED(this.handle, i, v);
  }
  LEDs(a) {
    sensel.LEDs(this.handle, a);
  }
}

exports.open = _=>{
  try {
    const h = sensel.Open();
    return new SenselDevice(h);
  } catch(e) {
    return null;
  }
};
exports.ContactsMask = sensel.ContactsMask;
exports.ContactState = sensel.ContactState;
