// SPDX-License-Identifier: GPL-3.0-only
// (this fork, ForkAde's discord-community-lobby 2.1-2.4) Membership: nickname rules, the Discord sign-in (a fake
// Discord), asking to join, approval, roles, the launcher's sign-in request and session, revocation.
import assert from "node:assert/strict";
import test from "node:test";
import { nicknameProblem } from "../src/members.js";
import { service } from "./fake.js";

const ADMIN = "100000000000000001";
const env = { ADMIN_DISCORD_ID: ADMIN, DISCORD_CLIENT_ID: "123", DISCORD_CLIENT_SECRET: "s3cret", PUBLIC_URL: "https://online.test",
              PAGES_ORIGIN: "https://pages.test", PAGES_URL: "https://pages.test/join.html", DISCORD_API: "https://discord.fake/api" };

// A fake Discord: codes "code-<id>" sign in as <id>; anything else is refused
function fakeDiscord() {
  const real = globalThis.fetch;
  const seen = [];
  globalThis.fetch = async (url, init = {}) => {
    const u = String(url);
    seen.push(u);
    if (u === "https://discord.fake/api/oauth2/token") {
      const form = new URLSearchParams(String(init.body));
      const code = form.get("code") || "";
      if (!code.startsWith("code-") || form.get("client_secret") !== "s3cret") {
        return new Response("{}", { status: 400 });
      }
      return new Response(JSON.stringify({ access_token: `tok-${code.slice(5)}` }));
    }
    if (u === "https://discord.fake/api/users/@me") {
      const auth = (init.headers && init.headers.authorization) || "";
      return new Response(JSON.stringify({ id: auth.replace("Bearer tok-", ""), email: "secret@example.com", avatar: "abc" }));
    }
    return real(url, init);
  };
  return { seen, restore: () => (globalThis.fetch = real) };
}

// The whole Discord round trip for the pages: a web session for <id>
async function webSession(call, id) {
  const start = await call("GET", "/v1/auth/discord/start?for=web");
  assert.equal(start.status, 302);
  const state = new URL(start.headers.get("location")).searchParams.get("state");
  const back = await call("GET", `/v1/auth/discord/callback?state=${state}&code=code-${id}`);
  assert.equal(back.status, 302);
  return new URL(back.headers.get("location")).hash.replace("#session=", "");
}

// The launcher's: a sign-in request, the browser's round trip, and the answer to its question
async function launcherSignIn(call, id) {
  const made = await (await call("POST", "/v1/auth/request", "")).json();
  const start = await call("GET", `/v1/auth/discord/start?for=${made.id}`);
  const state = new URL(start.headers.get("location")).searchParams.get("state");
  const page = await call("GET", `/v1/auth/discord/callback?state=${state}&code=code-${id}`);
  return { made, page, poll: async () => (await call("GET", `/v1/auth/request/${made.id}?secret=${made.secret}`)).json() };
}

test("nickname rules", () => {
  assert.equal(nicknameProblem("FI3RO"), "");
  assert.equal(nicknameProblem("DJ_FRIJOLES-1"), "");
  assert.match(nicknameProblem("ab"), /3 to 16/);
  assert.match(nicknameProblem("x".repeat(17)), /3 to 16/);
  assert.match(nicknameProblem("HOKUTO!"), /only letters/);
  assert.match(nicknameProblem("ñandú"), /only letters/);
});

test("Discord's authorize URL asks for identify only, with a state", async () => {
  const { call } = service(env);
  const start = await call("GET", "/v1/auth/discord/start?for=web");
  const go = new URL(start.headers.get("location"));
  assert.equal(go.origin + go.pathname, "https://discord.com/oauth2/authorize");
  assert.equal(go.searchParams.get("scope"), "identify");
  assert.equal(go.searchParams.get("redirect_uri"), "https://online.test/v1/auth/discord/callback");
  assert.ok(go.searchParams.get("state").length >= 32);
});

test("a wrong or reused state is refused, and nothing from Discord but the id is kept", async () => {
  const discord = fakeDiscord();
  try {
    const { env: e, call } = service(env);
    assert.equal((await call("GET", "/v1/auth/discord/callback?state=nope&code=code-200000000000000002")).status, 400);
    const start = await call("GET", "/v1/auth/discord/start?for=web");
    const state = new URL(start.headers.get("location")).searchParams.get("state");
    assert.equal((await call("GET", `/v1/auth/discord/callback?state=${state}&code=code-200000000000000002`)).status, 302);
    assert.equal((await call("GET", `/v1/auth/discord/callback?state=${state}&code=code-200000000000000002`)).status, 400);
    const kept = JSON.stringify([...e.MEMBERS.objects.values()].map((o) => o.data));
    assert.ok(!kept.includes("secret@example.com") && !kept.includes("tok-") && !kept.includes("abc"));
  } finally {
    discord.restore();
  }
});

test("ask, approve, sign in from the launcher; pending waits; taken nicknames refused", async () => {
  const discord = fakeDiscord();
  try {
    const { call } = service(env);
    const fiero = "200000000000000002", hokuto = "300000000000000003";
    const web = await webSession(call, fiero);
    const asked = await call("POST", "/v1/members/request", { session: web, nickname: "FI3RO", flag: "MX" });
    assert.equal(asked.status, 200);
    // (case does not make another nickname)
    const other = await webSession(call, hokuto);
    const taken = await call("POST", "/v1/members/request", { session: other, nickname: "fi3ro", flag: "BR" });
    assert.equal(taken.status, 409);
    assert.match((await taken.json()).error, /taken/);
    // (pending: the launcher is told it waits)
    const early = await launcherSignIn(call, fiero);
    assert.match(await early.page.text(), /Signed in/);
    assert.equal((await early.poll()).state, "waiting");
    // (the administrator approves)
    const admin = await webSession(call, ADMIN);
    const list = await (await call("POST", "/v1/admin/list", { session: admin })).json();
    assert.deepEqual(list.requests.map((r) => r.nickname), ["FI3RO"]);
    assert.equal((await call("POST", "/v1/admin/approve", { session: admin, id: fiero })).status, 200);
    // (now the launcher gets its session, once)
    const late = await launcherSignIn(call, fiero);
    const done = await late.poll();
    assert.equal(done.state, "done");
    assert.equal(done.nickname, "FI3RO");
    assert.equal(done.flag, "MX");
    assert.equal(done.role, "member");
    assert.match(done.session, /^[0-9a-f]{64}$/);
    assert.equal((await late.poll()).state, "expired");
    // (the pages' answer to who I am)
    const me = await (await call("POST", "/v1/members/me", { session: web })).json();
    assert.equal(me.role, "member");
  } finally {
    discord.restore();
  }
});

test("only administrators act; roles; revocation ends sessions; the log records each", async () => {
  const discord = fakeDiscord();
  try {
    const { env: e, call } = service(env);
    const dj = "400000000000000004";
    const web = await webSession(call, dj);
    await call("POST", "/v1/members/request", { session: web, nickname: "DJFRIJOLES", flag: "MX" });
    const admin = await webSession(call, ADMIN);
    await call("POST", "/v1/admin/approve", { session: admin, id: dj });
    // (a member can't)
    assert.equal((await call("POST", "/v1/admin/list", { session: web })).status, 403);
    assert.equal((await call("POST", "/v1/admin/role", { session: web, id: dj, role: "admin" })).status, 403);
    // (an administrator makes a moderator)
    assert.equal((await call("POST", "/v1/admin/role", { session: admin, id: dj, role: "moderator" })).status, 200);
    const launcher = await (await launcherSignIn(call, dj)).poll();
    assert.equal(launcher.role, "moderator");
    const members = e.MEMBERS.get("all");
    const ok = await (await members.fetch("https://members/member", { method: "POST", body: JSON.stringify({ session: launcher.session }) })).json();
    assert.equal(ok.nickname, "DJFRIJOLES");
    // (revoked: the launcher's session no longer a member's)
    assert.equal((await call("POST", "/v1/admin/revoke", { session: admin, id: dj })).status, 200);
    const after = await members.fetch("https://members/member", { method: "POST", body: JSON.stringify({ session: launcher.session }) });
    assert.equal(after.status, 401);
    const log = await (await call("POST", "/v1/admin/log", { session: admin })).json();
    assert.deepEqual(log.log.map((l) => l.action), ["approve", "role", "revoke"]);
  } finally {
    discord.restore();
  }
});

test("a sign-in request expires after 10 minutes, a session 30 days after last use", async () => {
  const discord = fakeDiscord();
  const realNow = Date.now;
  try {
    const { env: e, call } = service(env);
    let now = realNow();
    Date.now = () => now;
    const made = await (await call("POST", "/v1/auth/request", "")).json();
    now += 11 * 60 * 1000;
    assert.equal((await (await call("GET", `/v1/auth/request/${made.id}?secret=${made.secret}`)).json()).state, "expired");
    // (the administrator's own launcher session, then 31 days of silence)
    const admin = await (await launcherSignIn(call, ADMIN)).poll();
    assert.equal(admin.state, "done");
    const members = e.MEMBERS.get("all");
    now += 31 * 24 * 3600 * 1000;
    assert.equal((await members.fetch("https://members/member", { method: "POST", body: JSON.stringify({ session: admin.session }) })).status, 401);
  } finally {
    Date.now = realNow;
    discord.restore();
  }
});

test("the pages are answered with CORS for their origin only", async () => {
  const { call } = service(env);
  const pre = await call("OPTIONS", "/v1/members/me");
  assert.equal(pre.status, 204);
  assert.equal(pre.headers.get("access-control-allow-origin"), "https://pages.test");
});
