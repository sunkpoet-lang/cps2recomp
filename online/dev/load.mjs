// SPDX-License-Identifier: GPL-3.0-only
// (this fork, ForkAde's discord-community-lobby 6.1) The public lobby's load, measured: N members over one hour,
// against the Worker with the tests' in-memory stand-in for Durable Objects (test/fake.js) on a simulated clock, so
// that an hour runs in seconds. It counts what Cloudflare's Free plan limits by the day, then scales the hour to a day:
//   - Worker requests (each call to the Worker: sign-ins, the socket's opening);
//   - Durable Object requests (each fetch of an object, and each WebSocket message in, at 20 to 1);
//   - Durable Object duration (each object awake from an event until 10 seconds after its last, at 128 MB);
//   - rows written (each storage put) and the largest value written.
//   node online/dev/load.mjs [MEMBERS...]          (10 20 50)
// The members' hour (a busy evening): each connects in the first minute, says something about every 3 minutes,
// starts and ends a game twice, and loses its connection and comes back once.
import { service } from "../test/fake.js";

const kHour = 3600 * 1000;
const kAwake = 10 * 1000;          // (an object hibernates about 10 seconds after its last event)
const kGigabytes = 128 / 1024;     // (an object's memory, as Cloudflare bills it)
const kFree = { workerRequests: 100000, objectRequests: 100000, gbSeconds: 13000, rowsWritten: 100000 };
const kMessagesPerHour = 20, kGamesPerHour = 2, kDropsPerHour = 1;

// The simulated clock: Date.now() is `clock`
let clock = Date.UTC(2026, 10, 10, 20, 0, 0);
const realNow = Date.now;
Date.now = () => clock;

// (a seeded random: the same run each time)
let seed = 20261110;
function random() {
  seed = (seed * 1103515245 + 12345) % 2147483648;
  return seed / 2147483648;
}

async function run(count) {
  const ADMIN = "100000000000000001";
  const counts = { workerRequests: 0, objectFetches: 0, messagesIn: 0, rowsWritten: 0, largest: 0, largestKey: "" };
  const events = new Map(); // object -> event times
  const touch = (name) => {
    if (!events.has(name)) {
      events.set(name, []);
    }
    events.get(name).push(clock);
  };
  // (a fake Discord: code-<id> is <id>)
  const real = globalThis.fetch;
  globalThis.fetch = async (url, init = {}) => {
    const u = String(url);
    if (u.endsWith("/oauth2/token")) {
      return new Response(JSON.stringify({ access_token: `tok-${new URLSearchParams(String(init.body)).get("code").slice(5)}` }));
    }
    if (u.endsWith("/users/@me")) {
      return new Response(JSON.stringify({ id: init.headers.authorization.replace("Bearer tok-", "") }));
    }
    return real(url, init);
  };
  const { env, call: rawCall } = service({ ADMIN_DISCORD_ID: ADMIN, DISCORD_CLIENT_ID: "1", DISCORD_CLIENT_SECRET: "s", PUBLIC_URL: "https://online.test",
                                           PAGES_ORIGIN: "https://online.test", PAGES_URL: "https://online.test/join", DISCORD_API: "https://discord.fake/api" });
  // (counting: each namespace's objects' fetches, their storage puts and the sockets' messages in)
  for (const [binding, ns] of Object.entries(env)) {
    if (!ns || typeof ns.get !== "function") {
      continue;
    }
    const get = ns.get.bind(ns);
    ns.get = (id) => {
      const stub = get(id);
      const object = ns.objects.get(id);
      const name = `${binding}:${id}`;
      if (!object.__counted) {
        object.__counted = true;
        const storage = object.state.storage;
        const put = storage.put.bind(storage);
        storage.put = async (key, value) => {
          counts.rowsWritten++;
          const size = JSON.stringify(value).length;
          if (size > counts.largest) {
            counts.largest = size;
            counts.largestKey = `${name} ${key}`;
          }
          return put(key, value);
        };
        if (object.webSocketMessage) {
          const message = object.webSocketMessage.bind(object);
          object.webSocketMessage = async (ws, data) => {
            counts.messagesIn++;
            touch(name);
            return message(ws, data);
          };
        }
      }
      return { fetch: (url, init) => { counts.objectFetches++; touch(name); return stub.fetch(url, init); } };
    };
  }
  const call = (...args) => { counts.workerRequests++; return rawCall(...args); };

  // Members: asked, approved, signed in from the launcher (not counted: once each, not every day)
  const web = async (id) => {
    const start = await rawCall("GET", "/v1/auth/discord/start?for=web");
    const state = new URL(start.headers.get("location")).searchParams.get("state");
    const back = await rawCall("GET", `/v1/auth/discord/callback?state=${state}&code=code-${id}`);
    return new URL(back.headers.get("location")).hash.replace("#session=", "");
  };
  const admin = await web(ADMIN);
  const sessions = [];
  for (let i = 0; i < count; i++) {
    const id = String(300000000000000000n + BigInt(i));
    await rawCall("POST", "/v1/members/request", { session: await web(id), nickname: `MEMBER${i}`, flag: "MX" });
    await rawCall("POST", "/v1/admin/approve", { session: admin, id });
    const made = await (await rawCall("POST", "/v1/auth/request", "")).json();
    const start = await rawCall("GET", `/v1/auth/discord/start?for=${made.id}`);
    const state = new URL(start.headers.get("location")).searchParams.get("state");
    await rawCall("GET", `/v1/auth/discord/callback?state=${state}&code=code-${id}`);
    sessions.push((await (await rawCall("GET", `/v1/auth/request/${made.id}?secret=${made.secret}`)).json()).session);
  }
  for (const k of Object.keys(counts)) {
    counts[k] = typeof counts[k] === "number" ? 0 : "";
  }
  events.clear();

  // The hour: every member's actions, in time order
  const begin = clock;
  const actions = [];
  for (let i = 0; i < count; i++) {
    actions.push({ at: begin + random() * 60000, who: i, kind: "connect" });
    for (let k = 0; k < kMessagesPerHour; k++) {
      actions.push({ at: begin + 60000 + random() * (kHour - 60000), who: i, kind: "say" });
    }
    for (let k = 0; k < kGamesPerHour; k++) {
      const at = begin + 60000 + random() * (kHour - 20 * 60000);
      actions.push({ at, who: i, kind: "playing" }, { at: at + 15 * 60000, who: i, kind: "lobby" });
    }
    for (let k = 0; k < kDropsPerHour; k++) {
      const at = begin + 120000 + random() * (kHour - 240000);
      actions.push({ at, who: i, kind: "drop" }, { at: at + 5000, who: i, kind: "connect" });
    }
  }
  actions.sort((a, b) => a.at - b.at);
  const sockets = new Array(count).fill(null);
  let refused = 0, delivered = 0;
  for (const a of actions) {
    clock = Math.round(a.at);
    const ws = sockets[a.who];
    if (a.kind === "connect") {
      const answer = await call("GET", "/v1/community/socket", undefined, { upgrade: "websocket", "sec-websocket-protocol": `forkade.v1, s.${sessions[a.who]}` });
      sockets[a.who] = answer.status === 101 ? answer.webSocket : null;
      continue;
    }
    if (!ws || ws.closed) {
      continue;
    }
    if (a.kind === "drop") {
      ws.close(1006, "gone");
      sockets[a.who] = null;
    } else if (a.kind === "say") {
      const before = ws.inbox.length;
      await ws.send(JSON.stringify({ type: "say", text: `Mensaje de prueba ${a.who}: alguien para unas de Super Turbo? FT5 ${"x".repeat(Math.floor(random() * 120))}` }));
      const got = ws.inbox.slice(before);
      refused += got.some((e) => e.type === "refused");
      delivered += got.some((e) => e.type === "message");
    } else {
      await ws.send(JSON.stringify({ type: "state", state: a.kind, game: a.kind === "playing" ? "ssf2t" : undefined }));
    }
  }
  clock = begin + kHour;
  globalThis.fetch = real;

  // Awake time: each object from an event until 10 seconds after the last of a run of them
  let awake = 0;
  for (const times of events.values()) {
    times.sort((a, b) => a - b);
    let from = times[0], to = times[0] + kAwake;
    for (const t of times.slice(1)) {
      if (t > to) {
        awake += to - from;
        from = t;
      }
      to = t + kAwake;
    }
    awake += to - from;
  }
  const hour = {
    workerRequests: counts.workerRequests,
    objectRequests: counts.objectFetches + counts.messagesIn / 20,
    gbSeconds: (awake / 1000) * kGigabytes,
    rowsWritten: counts.rowsWritten,
  };
  return { count, hour, counts, refused, delivered, said: count * kMessagesPerHour };
}

const sizes = process.argv.slice(2).map(Number).filter((n) => n > 0);
console.log("| Members | Worker requests/h | DO requests/h (fetches + WS in/20) | DO GB-s/h | Rows written/h | Largest value | Messages delivered | A day at this rate (worst limit) |");
console.log("|---|---|---|---|---|---|---|---|");
for (const n of sizes.length ? sizes : [10, 20, 50]) {
  const r = await run(n);
  const worst = Object.keys(kFree).map((k) => [k, (r.hour[k] * 24) / kFree[k]]).sort((a, b) => b[1] - a[1])[0];
  console.log(`| ${n} | ${r.hour.workerRequests} | ${Math.round(r.hour.objectRequests)} (${r.counts.objectFetches} + ${r.counts.messagesIn}/20) | ${r.hour.gbSeconds.toFixed(0)} | ${r.hour.rowsWritten} | ${(r.counts.largest / 1024).toFixed(1)} KB (${r.counts.largestKey}) | ${r.delivered}/${r.said} (${r.refused} refused) | ${(worst[1] * 100).toFixed(0)}% of ${worst[0]} |`);
}
Date.now = realNow;
