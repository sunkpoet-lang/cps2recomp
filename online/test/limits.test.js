// SPDX-License-Identifier: GPL-3.0-only
// (this fork, ForkAde's online-abuse-protection 2.2) Per-action limits: floods refused once enforced, only logged
// before, other players untouched, and an honest evening never refused.
import assert from "node:assert/strict";
import test from "node:test";
import { Limiter, networkOf } from "../src/index.js";
import { service } from "./fake.js";

const host = (name) => ({ game: "ssf2t", build: "b1", version: "0.1", addresses: ["198.51.100.4:6000"], listed: true, name, protocol: 3 });
const quiet = (fn) => async () => {
  const log = console.log;
  console.log = () => {};
  try {
    await fn();
  } finally {
    console.log = log;
  }
};

test("networks: an IPv4 address to its /24, IPv6 to its /48", () => {
  const of = (a) => networkOf(new Request("https://x/", { headers: { "cf-connecting-ip": a } }));
  assert.equal(of("203.0.113.7"), "203.0.113.0/24");
  assert.equal(of("2001:db8:1:2::5"), "2001:db8:1::/48");
  assert.equal(of(""), "unknown");
});

test("the limiter: a sliding minute, and the seconds to wait", () => {
  const l = new Limiter();
  for (let i = 0; i < 3; i++) {
    assert.equal(l.hit("k", 3, 1000), 0);
  }
  assert.equal(l.hit("k", 3, 2000), 59);
  assert.equal(l.hit("k", 3, 61000), 0);
});

test("a 200-request host flood is refused past the limit; another network still hosts", quiet(async () => {
  const { call } = service({ LIMITS: "enforce" });
  let made = 0, refused = 0, retry = null;
  for (let i = 0; i < 200; i++) {
    const answer = await call("POST", "/v1/host", host("Flood"), { from: "203.0.113.7" });
    if (answer.status === 200) {
      made++;
    } else if (answer.status === 429) {
      refused++;
      retry = answer.headers.get("retry-after");
    }
  }
  // (the open-match cap, three at once by a network, comes first: caps.test.js; the rate limit stops the rest)
  assert.equal(made, 3);
  assert.equal(refused, 197);
  assert.ok(Number(retry) >= 1);
  const other = await call("POST", "/v1/host", host("Daniel"), { from: "198.51.100.20" });
  assert.equal(other.status, 200);
  // (and the lobby shows the honest host among the flood's 3, not 200)
  const listed = await (await call("GET", "/v1/lobby?protocol=3", undefined, { from: "192.0.2.1" })).json();
  assert.equal(listed.matches.length, 4);
}));

test("before limits are enforced, the flood goes on and each would-be refusal is logged", async () => {
  const { call } = service();
  const lines = [];
  const log = console.log;
  console.log = (line) => lines.push(line);
  try {
    for (let i = 0; i < 25; i++) {
      assert.equal((await call("POST", "/v1/host", host("Flood"), { from: "203.0.113.7" })).status, 200);
    }
  } finally {
    console.log = log;
  }
  // (22 would-be refusals: the open-match cap from the fourth, then the rate limit past the twentieth)
  assert.equal(lines.length, 22);
  const said = JSON.parse(lines[0]);
  assert.deepEqual(said, { limit: "host", by: "open", wait: said.wait, enforced: false });
  // (no address, no name, no secret in the log)
  assert.ok(!lines.join("").includes("203.0.113"));
});

test("an honest evening is never refused", quiet(async () => {
  const { call } = service({ LIMITS: "enforce" });
  const me = { secret: "d".repeat(32), name: "Daniel", games: ["ssf2t"], protocol: 3, version: "0.1" };
  for (let evening = 0; evening < 3; evening++) {
    // (the Online page open: a check-in, the lobby looked at)
    for (let i = 0; i < 6; i++) {
      assert.equal((await call("POST", "/v1/room", { ...me, lobby: true }, { from: "203.0.113.7" })).status, 200);
      assert.equal((await call("GET", "/v1/lobby?protocol=3", undefined, { from: "203.0.113.7" })).status, 200);
    }
    // (hosting, asked about every 2 seconds for a minute, then joined from another home)
    const made = await (await call("POST", "/v1/host", host("Daniel"), { from: "203.0.113.7" })).json();
    for (let i = 0; i < 30; i++) {
      assert.equal((await call("GET", `/v1/host/${made.code}?secret=${made.secret}`, undefined, { from: "203.0.113.7" })).status, 200);
    }
    assert.equal((await call("GET", `/v1/code/${made.code}`, undefined, { from: "198.51.100.20" })).status, 200);
    assert.equal((await call("POST", `/v1/join/${made.code}`, { game: "ssf2t", build: "b1", version: "0.1", addresses: [] }, { from: "198.51.100.20" })).status, 200);
  }
}));

test("one player checking in too often is slowed; another in the same home is not", quiet(async () => {
  const { call } = service({ LIMITS: "enforce" });
  const a = { secret: "a".repeat(32), name: "A", protocol: 3, version: "0.1" };
  const b = { secret: "b".repeat(32), name: "B", protocol: 3, version: "0.1" };
  const statuses = [];
  for (let i = 0; i < 30; i++) {
    statuses.push((await call("POST", "/v1/room", a, { from: "203.0.113.7" })).status);
  }
  assert.equal(statuses.filter((s) => s === 429).length, 10);
  assert.equal((await call("POST", "/v1/room", b, { from: "203.0.113.8" })).status, 200);
}));

test("sign-in floods are refused per network once enforced; an honest sign-in and other homes are not", quiet(async () => {
  const { call } = service({ LIMITS: "enforce", DISCORD_CLIENT_ID: "1", PUBLIC_URL: "https://online.test" });
  // (an honest sign-in from the launcher: its request, then the browser at Discord's start)
  assert.equal((await call("POST", "/v1/auth/request", "", { from: "198.51.100.20" })).status, 200);
  assert.equal((await call("GET", "/v1/auth/discord/start?for=web", undefined, { from: "198.51.100.20" })).status, 302);
  // (a flood of sign-in starts and requests from one network: refused past 20 a minute, with the wait)
  const statuses = [];
  for (let i = 0; i < 60; i++) {
    const r = i % 2 ? await call("POST", "/v1/auth/request", "", { from: "203.0.113.9" }) : await call("GET", "/v1/auth/discord/start?for=web", undefined, { from: "203.0.113.9" });
    statuses.push(r.status);
  }
  assert.equal(statuses.filter((s) => s !== 429).length, 20);
  const refused = await call("POST", "/v1/auth/request", "", { from: "203.0.113.10" });
  assert.equal(refused.status, 429);
  assert.ok(Number(refused.headers.get("retry-after")) > 0);
  // (another home still signs in; reading one's own state is not counted)
  assert.equal((await call("GET", "/v1/auth/discord/start?for=web", undefined, { from: "192.0.2.50" })).status, 302);
  assert.equal((await call("POST", "/v1/members/me", { session: "0".repeat(64) }, { from: "203.0.113.9" })).status, 401);
}));
