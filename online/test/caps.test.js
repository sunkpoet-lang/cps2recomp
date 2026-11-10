// SPDX-License-Identifier: GPL-3.0-only
// (this fork, ForkAde's online-abuse-protection 2.3) Caps: open matches by a network, invitations waiting for one
// player, and the lists' ceilings.
import assert from "node:assert/strict";
import test from "node:test";
import { kOpenHosts, kPendingInvitations } from "../src/index.js";
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

test("a network's matches past the cap are refused; its open ones stay", quiet(async () => {
  const { call } = service({ LIMITS: "enforce" });
  const made = [];
  for (let i = 0; i < kOpenHosts; i++) {
    const answer = await call("POST", "/v1/host", host(`Host ${i}`), { from: "203.0.113.7" });
    assert.equal(answer.status, 200);
    made.push(await answer.json());
  }
  const extra = await call("POST", "/v1/host", host("One more"), { from: "203.0.113.9" });
  assert.equal(extra.status, 429);
  assert.ok(Number(extra.headers.get("retry-after")) > 60);
  // (the open ones still there, and a host asking for its own code again is not refused)
  const listed = await (await call("GET", "/v1/lobby?protocol=3", undefined, { from: "192.0.2.1" })).json();
  assert.equal(listed.matches.length, kOpenHosts);
  const again = await call("POST", "/v1/host", { ...host("Host 0"), code: made[0].code, secret: made[0].secret }, { from: "203.0.113.7" });
  assert.equal(again.status, 200);
  // (another network is not affected)
  assert.equal((await call("POST", "/v1/host", host("Elsewhere"), { from: "198.51.100.20" })).status, 200);
}));

test("a joined match frees its place", quiet(async () => {
  const { call } = service({ LIMITS: "enforce" });
  const made = [];
  for (let i = 0; i < kOpenHosts; i++) {
    made.push(await (await call("POST", "/v1/host", host(`Host ${i}`), { from: "203.0.113.7" })).json());
  }
  const joined = await call("POST", `/v1/join/${made[1].code}`, { game: "ssf2t", build: "b1", version: "0.1", addresses: [] }, { from: "198.51.100.20" });
  assert.equal(joined.status, 200);
  assert.equal((await call("POST", "/v1/host", host("Next"), { from: "203.0.113.7" })).status, 200);
}));

test("before limits are enforced, the open cap only logs", quiet(async () => {
  const { call } = service();
  for (let i = 0; i < kOpenHosts + 2; i++) {
    assert.equal((await call("POST", "/v1/host", host(`Host ${i}`), { from: "203.0.113.7" })).status, 200);
  }
}));

test("at most kPendingInvitations wait for one player", async () => {
  const { env, call } = service();
  const me = { secret: "m".repeat(32), name: "Daniel", protocol: 3, version: "0.1" };
  const id = (await (await call("POST", "/v1/room", me)).json()).id;
  const room = env.ROOMS.get("all");
  const given = [];
  for (let i = 0; i < kPendingInvitations + 5; i++) {
    const code = `K7QM${String(i).padStart(2, "2").replace(/[01]/g, "3")}`;
    const said = await (await room.fetch("https://room/invite", {
      method: "POST",
      body: JSON.stringify({ to: id, from: "abcdefabcdef", code: `${code}${i}`, game: "ssf2t", protocol: 3, version: "0.1", name: "Inviter" }),
    })).json();
    given.push(said.given);
  }
  assert.equal(given.filter(Boolean).length, kPendingInvitations);
  const seen = await (await call("POST", "/v1/room", me)).json();
  assert.equal(seen.invitations.length, kPendingInvitations);
});
