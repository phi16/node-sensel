const sensel = require('bindings')('sensel');

const h = sensel.Open();
console.log(h);
sensel.StartScanning(h);

while(true) {
  sensel.Frame(h, (f,i,n)=>{
    console.log(`${sensel.NumContacts(f)} / ${n}`);
    return true;
  });
}
