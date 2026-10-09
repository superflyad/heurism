// Ephemeral, one-device PXE service. Ordinary DHCP clients receive no response.
const fs=require('fs'),path=require('path'),crypto=require('crypto'),dgram=require('dgram'),http=require('http');
const root=path.resolve(__dirname,'..');
const serverIP='10.8.22.122',targetIP='10.8.22.238',targetMAC=Buffer.from('7cc2c61db2f5','hex');
const ip=s=>Buffer.from(s.split('.').map(Number));
function options(message){
 if(message.length<240 || message.readUInt32BE(236)!==0x63825363)return null;
 const result=new Map();
 for(let p=240;p<message.length;){const tag=message[p++];if(tag===255)break;if(!tag)continue;
  if(p>=message.length)return null;const n=message[p++];if(p+n>message.length)return null;
  result.set(tag,message.subarray(p,p+n));p+=n;}return result;
}
function dhcpReply(message,filename){
 const opts=options(message);
 if(!opts || message[0]!==1 || message[1]!==1 || message[2]!==6 || !message.subarray(28,34).equals(targetMAC) || message.readUInt32BE(24)!==0)return null;
 if(!(opts.get(60)||Buffer.alloc(0)).toString().startsWith('PXEClient'))return null;
 const type=(opts.get(53)||[])[0];if(![1,3].includes(type))return null;
 if(opts.has(54)&&!opts.get(54).equals(ip(serverIP)))return null;
 if(opts.has(50)&&!opts.get(50).equals(ip(targetIP)))return null;
 const body=Buffer.alloc(240);message.copy(body,0,0,44);body[0]=2;body.writeUInt16BE(0x8000,10);
 ip(targetIP).copy(body,16);ip(serverIP).copy(body,20);body.fill(0,44,236);body.write(filename,108,'ascii');body.writeUInt32BE(0x63825363,236);
 const opt=(tag,data)=>Buffer.concat([Buffer.from([tag,data.length]),data]);const lease=Buffer.alloc(4);lease.writeUInt32BE(600);
 return Buffer.concat([body,opt(53,Buffer.from([type===1?2:5])),opt(54,ip(serverIP)),opt(1,ip('255.255.255.0')),
  opt(3,ip('10.8.22.1')),opt(6,ip('10.8.22.1')),opt(51,lease),opt(66,Buffer.from(serverIP)),opt(67,Buffer.from(filename)),
  opts.has(61)?opt(61,opts.get(61)):Buffer.alloc(0),Buffer.from([255])]);
}
module.exports={options,dhcpReply};
if(require.main===module){
 const mode=process.argv[2];if(!['probe','ram'].includes(mode))throw Error('Select probe or ram');
 const filename=mode==='ram'?'companion-netboot.efi':'companion-probe.efi';
 const file=mode==='ram'?'build/network-root/companion-netboot.efi':'build/udp-probe/fvprobex64.efi';
 const payload=fs.readFileSync(path.join(root,file));const sha=crypto.createHash('sha256').update(payload).digest('hex');
 if(mode==='ram'&&sha!==JSON.parse(fs.readFileSync(path.join(root,'build/network-root/efi-manifest.json'))).efi_sha256)throw Error('Image hash mismatch');
 const logpath=path.join(root,'artifacts/firmware/network-boot-events.jsonl');
 const log=event=>fs.appendFileSync(logpath,JSON.stringify({time:new Date().toISOString(),mode,...event})+'\n');
 const sockets=new Set();const bind=(port,callback)=>{const s=dgram.createSocket('udp4');sockets.add(s);s.on('error',e=>{log({error:e.message,port});process.exitCode=1;shutdown();});s.on('message',callback);s.bind(port,serverIP);return s;};
 let closing=false;const transfers=new Set();let health;
 function shutdown(){if(closing)return;closing=true;for(const t of transfers)t.close();for(const s of sockets)s.close();if(health)health.close();}
 const dhcp=bind(67,(message,peer)=>{const reply=dhcpReply(message,filename);if(!reply)return;
  log({protocol:'DHCP',type:reply[242],xid:message.readUInt32BE(4),peer:peer.address,filename});dhcp.send(reply,68,'255.255.255.255',error=>{if(error)log({protocol:'DHCP',send_error:error.message});});});
 dhcp.on('listening',()=>dhcp.setBroadcast(true));
 const tftp=bind(69,(message,peer)=>{
  if(peer.address!==targetIP && peer.address!==serverIP && peer.address!=='127.0.0.1')return;
  if(message.length<4||message.length>2048||message.readUInt16BE(0)!==1)return;
  const fields=message.subarray(2).toString('ascii').split('\0');
  if(fields[0]!==filename||fields[1].toLowerCase()!=='octet')return;
  const requested=new Map();for(let i=2;i+1<fields.length-1;i+=2)requested.set(fields[i].toLowerCase(),fields[i+1]);
  let blockSize=512;const accepted=[];
  if(requested.has('blksize')){const n=Number(requested.get('blksize'));if(!Number.isInteger(n)||n<8)return;blockSize=Math.min(n,1468);accepted.push('blksize',String(blockSize));}
  if(requested.get('tsize')==='0')accepted.push('tsize',String(payload.length));
  if(requested.has('timeout')&&requested.get('timeout')==='5')accepted.push('timeout','5');
  const socket=dgram.createSocket('udp4');let timer,last,retries=0,block=0,closed=false;
  const transfer={close(){if(closed)return;closed=true;clearTimeout(timer);socket.close();transfers.delete(transfer);}};transfers.add(transfer);
  function send(packet){last=packet;socket.send(packet,peer.port,peer.address);clearTimeout(timer);timer=setTimeout(()=>{if(++retries>6){log({protocol:'TFTP',event:'timeout',block});transfer.close();}else send(last);},5000);}
  function next(){block++;const offset=(block-1)*blockSize;const body=payload.subarray(offset,offset+blockSize);const p=Buffer.alloc(4+body.length);p.writeUInt16BE(3);p.writeUInt16BE(block&65535,2);body.copy(p,4);send(p);}
  socket.on('error',e=>{log({protocol:'TFTP',event:'error',error:e.message});transfer.close();});
  socket.on('message',(p,r)=>{if(r.address!==peer.address||r.port!==peer.port||p.length<4)return;
   if(p.readUInt16BE(0)===5){log({protocol:'TFTP',event:'client-error',detail:p.subarray(4).toString().slice(0,100)});transfer.close();return;}
   if(p.readUInt16BE(0)!==4||p.readUInt16BE(2)!==(block&65535))return;
   retries=0;if(block>0&&last.length<blockSize+4){log({protocol:'TFTP',event:'complete',peer:peer.address,bytes:payload.length,sha256:sha});transfer.close();}else next();});
  socket.bind(0,serverIP,()=>{log({protocol:'TFTP',event:'start',peer:peer.address,filename,bytes:payload.length,blockSize});
   if(accepted.length){const head=Buffer.from([0,6]);send(Buffer.concat([head,Buffer.from(accepted.join('\0')+'\0')]));}else next();});
 });
 const extension=fs.readFileSync(path.join(root,'build/extension/companionextx64.efi'));const corrupt=Buffer.from(extension);corrupt[0]^=1;
 const udp=bind(18081,(message,peer)=>{if(peer.address!==targetIP||message.length!==16||message.subarray(0,8).toString()!=='CMPNET01')return;
  const part=message.readUInt32LE(8),kind=message.readUInt32LE(12);if(part>1||![0,1,99].includes(kind))return;
  log({protocol:'OWNER-UDP',port:peer.port,part,kind});udp.send(kind===99?message:Buffer.concat([message,(kind===1?corrupt:extension).subarray(part*1024,(part+1)*1024)]),peer.port,peer.address);});
 health=http.createServer((req,res)=>{if(req.url!=='/health'){res.writeHead(404);res.end();return;}res.end(JSON.stringify({mode,filename,bytes:payload.length,sha256:sha,pid:process.pid}));});
 health.listen(18080,serverIP);log({event:'ready',filename,bytes:payload.length,sha256:sha,pid:process.pid});
 process.on('SIGINT',shutdown);process.on('SIGTERM',shutdown);setTimeout(shutdown,1800000);
}
