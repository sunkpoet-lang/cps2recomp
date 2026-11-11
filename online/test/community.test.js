// SPDX-License-Identifier: GPL-3.0-only
// (this fork, ForkAde's discord-community-lobby 4.1-4.2) The public lobby: members only, messages to everyone,
// presence, history kept 500 / 24 hours, the rate limit, text cleaning, moderation and revocation.
import assert from "node:assert/strict";
import test from "node:test";
import { cleanChat, kHistory } from "../src/community.js";
import { service } from "./fake.js";

const ADMIN = "100000000000000001";
const env = { ADMIN_DISCORD_ID: ADMIN, DISCORD_CLIENT_ID: "123", DISCORD_CLIENT_SECRET: "s3cret", PUBLIC_URL: "https://online.test",
              PAGES_ORIGIN: "https://pages.test", PAGES_URL: "https://pages.test/join.html", DISCORD_API: "https://discord.fake/api" };

function fakeDiscord() {
  const real = globalThis.fetch;
  globalThis.fetch = async (url, init = {}) => {
    const u = String(url);
    if (u === "https://discord.fake/api/oauth2/token") {
      return new Response(JSON.stringify({ access_token: `tok-${new URLSearchParams(String(init.body)).get("code").slice(5)}` }));
    }
    if (u === "https://discord.fake/api/users/@me") {
      return new Response(JSON.stringify({ id: init.headers.authorization.replace("Bearer tok-", "") }));
    }
    return real(url, init);
  };
  return () => (globalThis.fetch = real);
}

// A lobby with members: each signed in from "the launcher", approved, with a role
async function lobby(names) {
  const { env: e, call } = service(env);
  const web = async (id) => {
    const start = await call("GET", "/v1/auth/discord/start?for=web");
    const state = new URL(start.headers.get("location")).searchParams.get("state");
    const back = await call("GET", `/v1/auth/discord/callback?state=${state}&code=code-${id}`);
    return new URL(back.headers.get("location")).hash.replace("#session=", "");
  };
  const admin = await web(ADMIN);
  const people = {};
  let n = 2;
  for (const [nickname, flag, role] of names) {
    const id = String(200000000000000000n + BigInt(n++));
    await call("POST", "/v1/members/request", { session: await web(id), nickname, flag });
    await call("POST", "/v1/admin/approve", { session: admin, id });
    if (role && role !== "member") {
      await call("POST", "/v1/admin/role", { session: admin, id, role });
    }
    const made = await (await call("POST", "/v1/auth/request", "")).json();
    const start = await call("GET", `/v1/auth/discord/start?for=${made.id}`);
    const state = new URL(start.headers.get("location")).searchParams.get("state");
    await call("GET", `/v1/auth/discord/callback?state=${state}&code=code-${id}`);
    const done = await (await call("GET", `/v1/auth/request/${made.id}?secret=${made.secret}`)).json();
    people[nickname] = { id, session: done.session };
  }
  const connect = async (nickname, session = people[nickname].session) => {
    const answer = await call("GET", "/v1/community/socket", undefined, { upgrade: "websocket", "sec-websocket-protocol": `forkade.v1, s.${session}` });
    if (answer.status !== 101) {
      return { status: answer.status };
    }
    const ws = answer.webSocket;
    return { status: 101, ws, say: (event) => ws.send(JSON.stringify(event)), events: (type) => ws.inbox.filter((m) => !type || m.type === type) };
  };
  return { e, call, admin, people, connect };
}

test("text is cleaned: no zero-width or direction overrides, spaces collapsed, at most 500", () => {
  assert.equal(cleanChat("hola​ ‮mundo"), "hola mundo");
  assert.equal(cleanChat("  a \n\n b  "), "a b");
  assert.equal(cleanChat("x".repeat(600)).length, 500);
  assert.equal(cleanChat("\u0007"), "");
});

test("members see each other's messages and presence; others are refused", async () => {
  const restore = fakeDiscord();
  try {
    const l = await lobby([["FI3RO", "MX"], ["HOKUTO", "BR"]]);
    const a = await l.connect("FI3RO"), b = await l.connect("HOKUTO");
    assert.equal(a.status, 101);
    assert.deepEqual(b.events("hello")[0].presence.map((p) => p.nickname), ["FI3RO", "HOKUTO"]);
    await a.say({ type: "say", text: "someone for Super Turbo? FT5" });
    const got = b.events("message").map((m) => m.message);
    assert.equal(got.length, 1);
    assert.deepEqual([got[0].nickname, got[0].flag, got[0].text], ["FI3RO", "MX", "someone for Super Turbo? FT5"]);
    // (playing, said to everyone)
    await b.say({ type: "state", state: "playing" });
    const last = a.events("presence").at(-1).presence.find((p) => p.nickname === "HOKUTO");
    assert.equal(last.state, "playing");
    // (no session, or a made-up one: refused)
    assert.equal((await l.connect("FI3RO", "")).status, 401);
    assert.equal((await l.connect("FI3RO", "0".repeat(64))).status, 401);
    // (leaving: gone from the others' list)
    b.ws.close();
    assert.deepEqual(a.events("presence").at(-1).presence.map((p) => p.nickname), ["FI3RO"]);
  } finally {
    restore();
  }
});

test("a burst over the limit is refused with the time to wait", async () => {
  const restore = fakeDiscord();
  try {
    const l = await lobby([["FI3RO", "MX"], ["HOKUTO", "BR"]]);
    const a = await l.connect("FI3RO"), b = await l.connect("HOKUTO");
    for (let i = 0; i < 30; i++) {
      await a.say({ type: "say", text: `spam ${i}` });
    }
    assert.equal(b.events("message").length, 5);
    const refused = a.events("refused");
    assert.equal(refused.length, 25);
    assert.ok(refused[0].wait >= 1);
  } finally {
    restore();
  }
});

test("history keeps the last 500, and nothing older than 24 hours even with nobody there", async () => {
  const restore = fakeDiscord();
  const realNow = Date.now;
  try {
    const l = await lobby([["FI3RO", "MX"]]);
    let now = realNow();
    Date.now = () => now;
    const a = await l.connect("FI3RO");
    for (let i = 0; i < 600; i++) {
      now += 1100;
      await a.say({ type: "say", text: `message ${i}` });
    }
    a.ws.close();
    const again = await l.connect("FI3RO");
    const history = again.events("hello")[0].history;
    assert.equal(history.length, kHistory);
    assert.equal(history[0].text, "message 100");
    again.ws.close();
    // (25 hours later, nobody connected: the alarm trims)
    now += 25 * 3600 * 1000;
    await l.e.COMMUNITY.objects.get("all").alarm();
    const later = await l.connect("FI3RO");
    assert.equal(later.events("hello")[0].history.length, 0);
  } finally {
    Date.now = realNow;
    restore();
  }
});

test("moderation: members can't; a moderator deletes, mutes and kicks, and it is logged", async () => {
  const restore = fakeDiscord();
  try {
    const l = await lobby([["DJFRIJOLES", "MX", "moderator"], ["KURUVEGA", "CL"], ["FI3RO", "MX"]]);
    const mod = await l.connect("DJFRIJOLES"), kuru = await l.connect("KURUVEGA"), fiero = await l.connect("FI3RO");
    await kuru.say({ type: "say", text: "https://sitio-raro.example" });
    const mid = fiero.events("message")[0].message.mid;
    // (a member can't)
    await fiero.say({ type: "delete", mid });
    assert.match(fiero.events("refused")[0].error, /Only moderators/);
    // (the moderator deletes it for everyone)
    await mod.say({ type: "delete", mid });
    assert.equal(fiero.events("deleted")[0].mid, mid);
    // (mutes for 10 minutes: told, and refused with the time left)
    await mod.say({ type: "mute", nickname: "KURUVEGA", minutes: 10 });
    assert.equal(kuru.events("muted")[0].minutes, 10);
    await kuru.say({ type: "say", text: "hola?" });
    const refused = kuru.events("refused").at(-1);
    assert.match(refused.error, /muted/);
    assert.ok(refused.wait > 590 && refused.wait <= 600);
    assert.equal(fiero.events("message").length, 1);
    // (kicks: out of the lobby)
    await mod.say({ type: "kick", nickname: "KURUVEGA" });
    assert.equal(kuru.ws.closed.code, 4004);
    const log = await (await l.call("POST", "/v1/admin/log", { session: l.admin })).json();
    assert.deepEqual(log.log.filter((x) => ["delete", "mute", "kick"].includes(x.action)).map((x) => x.action), ["delete", "mute", "kick"]);
  } finally {
    restore();
  }
});

test("a revoked member's socket is closed, and they can't come back", async () => {
  const restore = fakeDiscord();
  try {
    const l = await lobby([["KURUVEGA", "CL"], ["HOKUTO", "BR"]]);
    const kuru = await l.connect("KURUVEGA"), hokuto = await l.connect("HOKUTO");
    await l.call("POST", "/v1/admin/revoke", { session: l.admin, id: l.people.KURUVEGA.id });
    assert.equal(kuru.ws.closed.code, 4003);
    assert.deepEqual(hokuto.events("presence").at(-1).presence.map((p) => p.nickname), ["HOKUTO"]);
    assert.equal((await l.connect("KURUVEGA")).status, 401);
  } finally {
    restore();
  }
});

test("the game being played: a short id only, with playing only (community-three-panel-layout)", async () => {
  const restore = fakeDiscord();
  try {
    const l = await lobby([["FI3RO", "MX"], ["HOKUTO", "BR"]]);
    const a = await l.connect("FI3RO"), b = await l.connect("HOKUTO");
    const hokuto = () => a.events("presence").at(-1).presence.find((p) => p.nickname === "HOKUTO");
    await b.say({ type: "state", state: "playing", game: "ssf2t" });
    assert.deepEqual([hokuto().state, hokuto().game], ["playing", "ssf2t"]);
    // (markup, too long, upper case, not a string: playing with no game, and none of the text sent on)
    // (three: with the first and the last, five events, the burst the lobby allows)
    for (const game of ["<b>ssf2t</b>", "x".repeat(200), 42]) {
      await b.say({ type: "state", state: "playing", game });
      assert.deepEqual([hokuto().state, hokuto().game], ["playing", undefined]);
      assert.ok(!JSON.stringify(a.events("presence").at(-1)).includes("<b>"));
    }
    // (a game with the lobby state is dropped; back to the lobby clears it)
    await b.say({ type: "state", state: "lobby", game: "ssf2t" });
    assert.deepEqual([hokuto().state, hokuto().game], ["lobby", undefined]);
  } finally {
    restore();
  }
});
