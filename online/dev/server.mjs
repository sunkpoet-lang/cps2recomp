// SPDX-License-Identifier: GPL-3.0-only
// (this fork, ForkAde's discord-community-lobby) A local dev server: the online Worker with the tests' in-memory
// stand-in for Durable Objects (test/fake.js), a fake Discord that lets you choose who signs in, and ForkAde's
// registration pages. Nothing leaves this machine; nothing is kept when it stops.
//   node online/dev/server.mjs [PORT] [SITE_DIR]      (8787; ../ForkAde/site)
// Then: http://127.0.0.1:PORT/site/join.html and /site/admin.html. The first administrator is Discord id
// 100000000000000001 (choose it on the fake Discord page).
import fs from "node:fs";
import http from "node:http";
import path from "node:path";
import worker from "../src/index.js";
import { service } from "../test/fake.js";

const port = Number(process.argv[2]) || 8787;
const site = path.resolve(process.argv[3] || "D:/CPS2/ForkAde/site");
const base = `http://127.0.0.1:${port}`;
const ADMIN = "100000000000000001";
const { env } = service({
  ADMIN_DISCORD_ID: ADMIN,
  DISCORD_CLIENT_ID: "dev",
  DISCORD_CLIENT_SECRET: "dev",
  PUBLIC_URL: base,
  PAGES_ORIGIN: base,
  PAGES_URL: `${base}/site/join.html`,
  PAGES_ADMIN_URL: `${base}/site/admin.html`,
  DISCORD_AUTHORIZE_URL: `${base}/fake-discord/authorize`,
  DISCORD_API: `${base}/fake-discord/api`,
  LIMITS: "log",
});
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

const types = { ".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8", ".css": "text/css" };

http.createServer(async (req, res) => {
  const url = new URL(req.url, base);
  try {
    if (url.pathname === "/fake-discord/authorize") {
      res.writeHead(200, { "content-type": "text/html; charset=utf-8" });
      return res.end(fakeAuthorize(url));
    }
    if (url.pathname.startsWith("/site/")) {
      if (url.pathname === "/site/config.js") {
        res.writeHead(200, { "content-type": types[".js"] });
        return res.end(`window.FORKADE_API = ${JSON.stringify(base)};\n`);
      }
      const file = path.join(site, path.normalize(url.pathname.slice(6)).replace(/^(\.\.[/\\])+/, ""));
      if (!file.startsWith(site) || !fs.existsSync(file)) {
        res.writeHead(404);
        return res.end("not here");
      }
      res.writeHead(200, { "content-type": types[path.extname(file)] || "application/octet-stream" });
      return res.end(fs.readFileSync(file));
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
}).listen(port, "127.0.0.1", () => console.log(`ForkAde dev server: ${base}/site/join.html (admin: ${base}/site/admin.html)`));
