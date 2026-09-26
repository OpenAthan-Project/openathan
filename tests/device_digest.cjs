const {spawn}=require('node:child_process');
const {createInterface}=require('node:readline');
const {join}=require('node:path');
const {randomBytes}=require('node:crypto');

// Run the actual firmware verifier/challenge generator, with only its hashing
// backend and monotonic clock supplied by the host test environment.
module.exports=function firmwareDigest(){
  const executable=process.env.OPENATHAN_DIGEST_TEST_BRIDGE || join(__dirname,'../build/digest_test_bridge');
  const child=spawn(executable,[],{stdio:['pipe','pipe','pipe']});
  const pending=[];
  let failure,stderr='';
  const fail=error=>{failure=error;for(const request of pending.splice(0))request.reject(error);};
  child.stdin.on('error',fail);
  child.stderr.on('data',data=>{stderr+=data;});
  child.on('error',error=>fail(new Error(`Build digest_test_bridge before browser tests: ${error.message}`)));
  const exited=new Promise(resolve=>child.on('close',code=>{
    if(code!==0 || pending.length)fail(new Error(`Digest adapter exited (${code}): ${stderr}`));
    resolve();
  }));
  createInterface({input:child.stdout}).on('line',line=>{
    const request=pending.shift();
    if(!request){fail(new Error('Unexpected Digest adapter response'));return;}
    try{request.resolve(JSON.parse(line));}catch(error){request.reject(error);fail(error);}
  });
  return {
    authorize(now,method,uri,header){
      if(failure)return Promise.reject(failure);
      return new Promise((resolve,reject)=>{
        pending.push({resolve,reject});
        const fields=[method,uri,header].map(value=>JSON.stringify(value)).join(' ');
        child.stdin.write(`${now} ${fields} ${randomBytes(24).toString('hex')}\n`);
      });
    },
    async close(){child.stdin.end();await exited;if(failure)throw failure;}
  };
};
