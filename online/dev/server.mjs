// SPDX-License-Identifier: GPL-3.0-only
// (this fork, ForkAde's discord-community-lobby) A local dev server: the online Worker with the tests' in-memory
// stand-in for Durable Objects (test/fake.js), a fake Discord that lets you choose who signs in, and ForkAde's
// registration pages served as the Worker's static assets serve them. Nothing leaves this machine; nothing is kept
// when it stops.
//   node online/dev/server.mjs [PORT] [SITE_DIR]      (8787; online/site, the copy tools/sync-site.sh makes)
// Then: http://127.0.0.1:PORT/join and /admin. The first administrator is Discord id 100000000000000001 (choose it on
// the fake Discord page).
import crypto from "node:crypto";
import fs from "node:fs";
import http from "node:http";
import path from "node:path";
import worker from "../src/index.js";
import { service } from "../test/fake.js";

// --real-discord: Discord itself instead of the fake one, with the app's id, its secret and the first administrator's
// id read from online/.dev.vars (which git ignores; .dev.vars.example says what goes there). The app must have
// http://127.0.0.1:PORT/v1/auth/discord/callback among its redirects.
const real = process.argv.includes("--real-discord");
const args = process.argv.slice(2).filter((a) => a !== "--real-discord");
const port = Number(args[0]) || 8787;
const here = path.dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, "$1"));
const site = path.resolve(args[1] || path.join(here, "..", "site"));
const base = `http://127.0.0.1:${port}`;
function devVars() {
  const file = path.join(path.dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, "$1")), "..", ".dev.vars");
  const vars = {};
  for (const line of fs.readFileSync(file, "utf8").split(/\r?\n/)) {
    const m = line.match(/^\s*([A-Z_]+)\s*=\s*(.*?)\s*$/);
    if (m) {
      vars[m[1]] = m[2];
    }
  }
  for (const name of ["DISCORD_CLIENT_ID", "DISCORD_CLIENT_SECRET", "ADMIN_DISCORD_ID"]) {
    if (!/^[A-Za-z0-9_-]{8,}$/.test(vars[name] || "") || /^0+$/.test(vars[name]) || vars[name] === "not-a-real-secret") {
      throw new Error(`online/.dev.vars: ${name} is missing or still the example's`);
    }
  }
  return vars;
}
const vars = real ? devVars() : {};
const ADMIN = real ? vars.ADMIN_DISCORD_ID : "100000000000000001";
const { env } = service({
  ADMIN_DISCORD_ID: ADMIN,
  DISCORD_CLIENT_ID: real ? vars.DISCORD_CLIENT_ID : "dev",
  DISCORD_CLIENT_SECRET: real ? vars.DISCORD_CLIENT_SECRET : "dev",
  PUBLIC_URL: base,
  PAGES_ORIGIN: base,
  PAGES_URL: `${base}/join`,
  PAGES_ADMIN_URL: `${base}/admin`,
  ...(real ? {} : { DISCORD_AUTHORIZE_URL: `${base}/fake-discord/authorize`, DISCORD_API: `${base}/fake-discord/api` }),
  LIMITS: "log",
});
console.log(real ? "Discord: the real one (online/.dev.vars)" : "Discord: the fake one (--real-discord for the real one)");
const ctx = { waitUntil: () => {} };

// (the fake Discord's API, answered in this process: a code "code-<id>" is that id)
const realFetch = globalThis.fetch;
globalThis.fetch = async (url, init = {}) => {
  const u = String(url);
  if (u === `${base}/fake-discord/api/oauth2/token`) {
    const code = new URLSearchParams(String(init.body)).get("code") || "";
    return code.startsWith("code-") ? new Response(JSON.stringify({ access_token: `tok-${code.slice(5)}` })) : new Response("{}", { status: 400 });
  }
  if (u === `${base}/fake-discord/api/users/@me`) {
    return new Response(JSON.stringify({ id: String(init.headers.authorization).replace("Bearer tok-", "") }));
  }
  return realFetch(url, init);
};

function fakeAuthorize(url) {
  const state = url.searchParams.get("state") || "";
  const back = url.searchParams.get("redirect_uri") || "";
  const choices = [[ADMIN, "Daniel (administrador)"], ["200000000000000002", "FI3RO"], ["200000000000000003", "HOKUTO"],
                   ["200000000000000004", "DJFRIJOLES"], ["200000000000000005", "KURUVEGA"]];
  return `<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Discord (simulado)</title>
<style>body{font:16px system-ui;background:#313338;color:#f2f3f5;max-width:420px;margin:48px auto;padding:0 16px}a{display:block;margin:8px 0;padding:10px 14px;background:#5865f2;color:#fff;border-radius:8px;text-decoration:none}</style>
<h1>Discord (simulado)</h1><p>Solo para pruebas locales: elige con qué cuenta entrar.</p>
${choices.map(([id, name]) => `<a href="${back}?code=code-${id}&state=${encodeURIComponent(state)}">${name}<br><small>${id}</small></a>`).join("")}`;
}

const types = { ".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8", ".css": "text/css", ".png": "image/png",
                ".txt": "text/plain; charset=utf-8" };
// A site file for a path, as Cloudflare's static assets find it: "/join" is join.html ("/join.html" is not served
// here, where Cloudflare would redirect it), "/flags.png" itself; _headers and anything outside the site are not
function assetOf(pathname) {
  let name;
  try {
    name = decodeURIComponent(pathname).replace(/^\/+/, "");
  } catch {
    return null;
  }
  if (!name || name.includes("..") || name.includes("\\") || name.startsWith("_") || name.endsWith(".html")) {
    return null;
  }
  for (const file of [path.join(site, `${name}.html`), path.join(site, name)]) {
    if (file.startsWith(site + path.sep) && fs.existsSync(file) && fs.statSync(file).isFile()) {
      return file;
    }
  }
  return null;
}
// The "/*" block of the site's _headers, as headers
function siteHeaders() {
  const out = {};
  const file = path.join(site, "_headers");
  if (fs.existsSync(file)) {
    for (const line of fs.readFileSync(file, "utf8").split(/\r?\n/)) {
      const m = line.match(/^\s+([A-Za-z-]+):\s*(.+)$/);
      if (m) {
        out[m[1].toLowerCase()] = m[2];
      }
    }
  }
  return out;
}

// A WebSocket for a real client (the launcher): the handshake, then text frames both ways, bridged to the Community
// object's fake socket (test/fake.js). Enough for testing: text frames, ping/pong, close; no extensions.
function frame(opcode, payload) {
  const body = Buffer.from(payload);
  const head = body.length < 126 ? Buffer.from([0x80 | opcode, body.length])
             : body.length < 65536 ? Buffer.from([0x80 | opcode, 126, body.length >> 8, body.length & 255])
             : Buffer.concat([Buffer.from([0x80 | opcode, 127]), (() => { const b = Buffer.alloc(8); b.writeBigUInt64BE(BigInt(body.length)); return b; })()]);
  return Buffer.concat([head, body]);
}

function upgrade(req, socket) {
  const url = new URL(req.url, base);
  if (url.pathname !== "/v1/community/socket") {
    socket.destroy();
    return;
  }
  const headers = new Headers();
  for (const [k, v] of Object.entries(req.headers)) {
    if (typeof v === "string") {
      headers.set(k, v);
    }
  }
  worker.fetch(new Request(url, { headers }), env, ctx).then((answer) => {
    if (answer.status !== 101) {
      socket.end(`HTTP/1.1 ${answer.status} Refused\r\nConnection: close\r\nContent-Length: 0\r\n\r\n`);
      return;
    }
    const accept = crypto.createHash("sha1").update(req.headers["sec-websocket-key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").digest("base64");
    socket.write(`HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${accept}\r\nSec-WebSocket-Protocol: forkade.v1\r\n\r\n`);
    const fake = answer.webSocket; // (the client's end of the object's pair)
    for (const event of fake.inbox.splice(0)) {
      socket.write(frame(1, JSON.stringify(event)));
    }
    fake.deliver = (data) => socket.write(frame(1, data));
    fake.onclose = () => {
      const code = fake.closed ? fake.closed.code : 1000;
      const reason = fake.closed ? fake.closed.reason : "";
      socket.end(frame(8, Buffer.concat([Buffer.from([code >> 8, code & 255]), Buffer.from(reason)])));
    };
    let pending = Buffer.alloc(0);
    socket.on("data", (chunk) => {
      pending = Buffer.concat([pending, chunk]);
      for (;;) {
        if (pending.length < 2) {
          return;
        }
        const opcode = pending[0] & 15, masked = pending[1] & 128;
        let length = pending[1] & 127, at = 2;
        if (length === 126) {
          if (pending.length < 4) return;
          length = pending.readUInt16BE(2);
          at = 4;
        } else if (length === 127) {
          if (pending.length < 10) return;
          length = Number(pending.readBigUInt64BE(2));
          at = 10;
        }
        const mask = masked ? pending.subarray(at, at + 4) : null;
        at += masked ? 4 : 0;
        if (pending.length < at + length) {
          return;
        }
        const payload = Buffer.from(pending.subarray(at, at + length));
        pending = pending.subarray(at + length);
        if (mask) {
          for (let i = 0; i < payload.length; i++) payload[i] ^= mask[i & 3];
        }
        if (opcode === 1) {
          Promise.resolve(fake.send(payload.toString("utf8"))).catch((e) => console.error(e));
        } else if (opcode === 9) {
          socket.write(frame(10, payload));
        } else if (opcode === 8) {
          fake.onclose = null;
          fake.close(1000, "bye");
          socket.end(frame(8, payload.subarray(0, 2)));
          return;
        }
      }
    });
    socket.on("close", () => {
      if (!fake.closed) {
        fake.onclose = null;
        fake.close(1001, "gone");
      }
    });
    socket.on("error", () => {});
  });
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, base);
  try {
    if (url.pathname === "/fake-discord/authorize") {
      res.writeHead(200, { "content-type": "text/html; charset=utf-8" });
      return res.end(fakeAuthorize(url));
    }
    // (the static assets, as Cloudflare serves them: a file of the site at its path, an .html one without its
    // extension, with _headers' headers; anything else goes to the Worker)
    const asset = assetOf(url.pathname);
    if (asset && ["GET", "HEAD"].includes(req.method)) {
      res.writeHead(200, { "content-type": types[path.extname(asset)] || "application/octet-stream", ...siteHeaders() });
      return res.end(req.method === "HEAD" ? undefined : fs.readFileSync(asset));
    }
    const chunks = [];
    for await (const chunk of req) {
      chunks.push(chunk);
    }
    const body = chunks.length && !["GET", "HEAD"].includes(req.method) ? Buffer.concat(chunks) : undefined;
    const headers = new Headers();
    for (const [k, v] of Object.entries(req.headers)) {
      if (typeof v === "string") {
        headers.set(k, v);
      }
    }
    headers.set("cf-connecting-ip", req.socket.remoteAddress || "127.0.0.1");
    const answer = await worker.fetch(new Request(url, { method: req.method, headers, body }), env, ctx);
    const out = {};
    answer.headers.forEach((v, k) => (out[k] = v));
    res.writeHead(answer.status, out);
    res.end(Buffer.from(await answer.arrayBuffer()));
  } catch (error) {
    console.error(error);
    res.writeHead(500);
    res.end(String(error));
  }
});
server.on("upgrade", upgrade);
server.listen(port, "127.0.0.1", () => console.log(`ForkAde dev server: ${base}/join (admin: ${base}/admin)`));
