// Read-only fixed payloads, using the installed Node runtime's existing LAN rule.
const http = require('http');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const dgram = require('dgram');
const root = path.resolve(__dirname, '..');
const payload = fs.readFileSync(path.join(root, 'build/extension/companionextx64.efi'));
const sha = data => crypto.createHash('sha256').update(data).digest('hex');
if (payload.length !== 2048 || sha(payload) !== 'b89ffa86d43a68a5296ed762702b25617d805e29a15f1f596dc6aed2f0d0151a') throw Error('Pinned payload differs');
const corrupt = Buffer.from(payload); corrupt[0] ^= 1;
const routes = new Map([
 ['/health', Buffer.from('COMPANION_HTTP_READY_01\n')],
 ['/extension/companion-http-01', payload],
 ['/corrupt/companion-http-01', corrupt],
]);
const allowed = new Set(['10.8.22.238', '10.8.22.122', '127.0.0.1']);
const server = http.createServer((request, response) => {
 const peer = request.socket.remoteAddress.replace(/^::ffff:/, '');
 if (!allowed.has(peer) || request.method !== 'GET') { response.writeHead(403); response.end(); return; }
 const body = routes.get(request.url);
 if (!body) { response.writeHead(404); response.end(); return; }
 fs.appendFileSync(path.join(root, 'artifacts/firmware/http-server-requests.jsonl'), JSON.stringify({
  unix_time: Date.now()/1000, peer, path: request.url,
  user_agent: String(request.headers['user-agent'] || '').slice(0,128), body_sha256: sha(body), bytes: body.length,
 })+'\n');
 response.writeHead(200, {'Content-Type':'application/octet-stream','Content-Length':body.length,'Connection':'close'});
 response.end(body);
});
server.requestTimeout = 3000; server.headersTimeout = 3000; server.timeout = 3000;
server.listen(18080, '10.8.22.122', () => console.log('Read-only Companion test server ready; expires in 30 minutes'));
const udp = dgram.createSocket('udp4');
udp.on('message', (message, remote) => {
 if (remote.address !== '10.8.22.238' || message.length !== 16 || message.subarray(0,8).toString() !== 'CMPNET01') return;
 const part = message.readUInt32LE(8), mode = message.readUInt32LE(12);
 if (part > 1 || ![0,1,99].includes(mode)) return;
 const body = mode === 99 ? message : Buffer.concat([message, (mode === 1 ? corrupt : payload).subarray(part*1024,(part+1)*1024)]);
 fs.appendFileSync(path.join(root, 'artifacts/firmware/http-server-requests.jsonl'), JSON.stringify({
  unix_time: Date.now()/1000, protocol:'UDP', peer:remote.address, source_port:remote.port,
  part, mode, bytes:body.length, body_sha256:sha(body),
 })+'\n');
 udp.send(body, remote.port, remote.address);
});
udp.bind(18081,'10.8.22.122');
setTimeout(() => { udp.close(); server.close(); if (server.closeAllConnections) server.closeAllConnections(); }, 1800000);
