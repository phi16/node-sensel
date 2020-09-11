const sensel = require('bindings')('sensel');
console.log(sensel.hello());

exports.hello = _=>{
  return sensel.hello();
};
