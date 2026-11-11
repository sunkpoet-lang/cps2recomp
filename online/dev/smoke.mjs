// SPDX-License-Identifier: GPL-3.0-only
// (this fork, discord-community-lobby) A smoke test of the local dev server (dev/server.mjs) over real HTTP and a
// real WebSocket: the administrator approves two people through the pages' routes, both sign in as the launcher
// does (a sign-in request completed through the fake Discord), connect to the lobby and talk.
//   node online/dev/smoke.mjs [BASE]       (http://127.0.0.1:8787)
// Prints "smoke: ok" or what failed; with --sessions, prints the two members' launcher sessions too (for other
// clients' tests).
const base = process.argv[2] && !process.argv[2].startsWith("--") ? process.argv[2] : "http://127.0.0.1:8787";
const ADMIN = "100000000000000001";

async function discord(forWhat, id) {
  const start = await fetch(`${base}/v1/auth/discord/start?for=${forWhat}`, { redirect: "manual" });
  const state = new URL(start.headers.get("location")).searchParams.get("state");
  return fetch(`${base}/v1/auth/discord/callback?state=${state}&code=code-${id}`, { redirect: "manual" });
}
async function web(id) {
  const back = await discord("web", id);
  return new URL(back.headers.get("location")).hash.replace("#session=", "");
}
const post = async (path, body) => (await fetch(base + path, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify(body) })).json();

async function member(id, nickname, flag, admin) {
  const asked = await post("/v1/members/request", { session: await web(id), nickname, flag });
  if (asked.error && !/already/.test(asked.error)) {
    throw new Error(`request ${nickname}: ${asked.error}`);
  }
  await post("/v1/admin/approve", { session: admin, id });
  const made = await (await fetch(`${base}/v1/auth/request`, { method: "POST" })).json();
  await discord(made.id, id);
  const done = await (await fetch(`${base}/v1/auth/request/${made.id}?secret=${made.secret}`)).json();
  if (done.state !== "done") {
    throw new Error(`sign-in ${nickname}: ${done.state}`);
  }
  return done.session;
}

function connect(session) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(`${base.replace("http", "ws")}/v1/community/socket`, ["forkade.v1", `s.${session}`]);
    const events = [];
    ws.onmessage = (m) => events.push(JSON.parse(m.data));
    ws.onopen = () => resolve({ ws, events });
    ws.onerror = (e) => reject(new Error("socket: " + (e.message || "error")));
  });
}
const wait = (ms) => new Promise((r) => setTimeout(r, ms));

const admin = await web(ADMIN);
const stamp = String(Date.now()).slice(-6);
const a = await member(`3${stamp}000000001`, `HOKUTO${stamp.slice(-3)}`, "BR", admin);
const b = await member(`3${stamp}000000002`, `KURU${stamp.slice(-3)}`, "CL", admin);
if (process.argv.includes("--sessions")) {
  console.log(JSON.stringify({ a, b }));
  process.exit(0);
}
const one = await connect(a), two = await connect(b);
await wait(300);
one.ws.send(JSON.stringify({ type: "say", text: "hola desde smoke" }));
await wait(300);
const got = two.events.filter((e) => e.type === "message").map((e) => e.message.text);
const hello = one.events.find((e) => e.type === "hello");
one.ws.close();
two.ws.close();
if (!hello || !got.includes("hola desde smoke")) {
  console.log("smoke: FAILED", JSON.stringify({ hello: Boolean(hello), got }));
  process.exit(1);
}
console.log("smoke: ok", JSON.stringify({ presence: hello.presence.map((p) => p.nickname), got }));
