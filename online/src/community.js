// SPDX-License-Identifier: GPL-3.0-only
// (this fork, ForkAde's discord-community-lobby) The public lobby every signed-in member shares: its general chat and
// who is there. One Durable Object (Community) with WebSocket Hibernation: a socket is a member's (checked with
// Members when it connects, its session in the Sec-WebSocket-Protocol header, never in the URL), events are sent as
// they happen (no polling), and the history (500 messages at most, nothing older than 24 hours) is kept in storage
// and trimmed by an alarm, also when nobody is connected. Moderators and administrators can delete a message, mute
// someone and kick them out; members only talk.

export const kHistory = 500;
const kDay = 24 * 3600 * 1000;
const kRecheck = 30 * 1000; // (the sockets' members asked about again, at most this often: revocation within 60 s)
const kBucket = { rate: 1, burst: 5 }; // (messages a second, sustained; at once)
export const kMuteMinutes = [10, 60, 1440];
export const kProtocol = "forkade.v1";

// A message's text as the lobby keeps it: NFC; no control, zero-width or direction-override characters; runs of
// spaces as one; at most 500 characters ("" if nothing is left)
export function cleanChat(text) {
  if (typeof text !== "string") {
    return "";
  }
  return text
    .normalize("NFC")
    .replace(/[\u0000-\u0008\u000b-\u001f\u007f-\u009f]/g, "")
    .replace(/[​-‏‪-‮⁠-⁤⁦-⁩﻿]/g, "")
    .replace(/[ \t\n\r]+/g, " ")
    .trim()
    .slice(0, 500);
}

// (a 101 answer with the client's end of the socket; a runtime without status 101 Responses, the tests', gets the
// same as a plain object)
function upgraded(client) {
  try {
    return new Response(null, { status: 101, webSocket: client, headers: { "sec-websocket-protocol": kProtocol } });
  } catch {
    return { status: 101, webSocket: client, headers: new Headers({ "sec-websocket-protocol": kProtocol }) };
  }
}

function json(body, status = 200) {
  return new Response(JSON.stringify(body), { status, headers: { "content-type": "application/json" } });
}

export class Community {
  constructor(state, env) {
    this.state = state;
    this.env = env;
    this.buckets = new Map(); // member id -> {tokens, at}
    this.checkedAt = 0;
  }

  async history() {
    if (!this.kept) {
      this.kept = (await this.state.storage.get("history")) || [];
      this.mutes = (await this.state.storage.get("mutes")) || {};
      this.next = (await this.state.storage.get("next")) || 1;
    }
    return this.kept;
  }

  // (the older than a day, and past the last 500, let go; an alarm to do it again while anything is kept)
  async trim(now) {
    const kept = await this.history();
    const fresh = kept.filter((m) => now - m.at < kDay).slice(-kHistory);
    if (fresh.length !== kept.length) {
      this.kept = fresh;
      await this.state.storage.put("history", fresh);
    }
    if (this.kept.length) {
      const oldest = this.kept[0].at;
      await this.state.storage.setAlarm(Math.max(now + 60 * 1000, oldest + kDay));
    }
  }

  sockets() {
    return this.state.getWebSockets();
  }

  who(ws) {
    try {
      return ws.deserializeAttachment() || {};
    } catch {
      return {};
    }
  }

  presence(except = null) {
    const seen = new Map();
    for (const ws of this.sockets()) {
      if (ws === except) {
        continue;
      }
      const w = this.who(ws);
      if (w.id && !seen.has(w.id)) {
        seen.set(w.id, { nickname: w.nickname, flag: w.flag, role: w.role, state: w.state || "lobby", ...(w.game ? { game: w.game } : {}),
                        ...(w.watch ? { watch: w.watch } : {}) });
      }
    }
    return [...seen.values()].sort((a, b) => a.nickname.localeCompare(b.nickname));
  }

  broadcast(event, except = null) {
    const text = JSON.stringify(event);
    for (const ws of this.sockets()) {
      if (ws === except) {
        continue;
      }
      try {
        ws.send(text);
      } catch {
        // (a socket going away: its close says so)
      }
    }
  }

  // (a member's bucket: true if this one may go)
  take(id, now) {
    const b = this.buckets.get(id) || { tokens: kBucket.burst, at: now };
    b.tokens = Math.min(kBucket.burst, b.tokens + ((now - b.at) / 1000) * kBucket.rate);
    b.at = now;
    this.buckets.set(id, b);
    if (b.tokens < 1) {
      return Math.max(1, Math.ceil((1 - b.tokens) / kBucket.rate));
    }
    b.tokens -= 1;
    return 0;
  }

  async fetch(request) {
    const url = new URL(request.url);
    const now = Date.now();
    if (url.pathname === "/revoked") {
      // (Members: a member revoked; their sockets closed now)
      const { id } = JSON.parse(await request.text());
      for (const ws of this.sockets()) {
        if (this.who(ws).id === id) {
          ws.close(4003, "Your membership was revoked");
        }
      }
      this.broadcast({ type: "presence", presence: this.presence() });
      return json({});
    }
    if (url.pathname !== "/socket") {
      return json({ error: "not here" }, 404);
    }
    if ((request.headers.get("upgrade") || "").toLowerCase() !== "websocket") {
      return json({ error: "a WebSocket is needed" }, 426);
    }
    // (the session: "forkade.v1, s.<hex>" in the subprotocol header)
    const offered = (request.headers.get("sec-websocket-protocol") || "").split(",").map((p) => p.trim());
    const session = (offered.find((p) => p.startsWith("s.")) || "").slice(2);
    if (!offered.includes(kProtocol) || !/^[0-9a-f]{64}$/.test(session)) {
      return json({ error: "Sign in again." }, 401);
    }
    const members = this.env.MEMBERS.get(this.env.MEMBERS.idFromName("all"));
    const said = await members.fetch("https://members/member", { method: "POST", body: JSON.stringify({ session }) });
    if (!said.ok) {
      return json({ error: "Sign in again." }, 401);
    }
    const me = await said.json();
    const pair = new WebSocketPair();
    const [client, server] = [pair[0], pair[1]];
    this.state.acceptWebSocket(server);
    server.serializeAttachment({ id: me.id, nickname: me.nickname, flag: me.flag, role: me.role, state: "lobby" });
    await this.trim(now);
    server.send(JSON.stringify({ type: "hello", you: { nickname: me.nickname, flag: me.flag, role: me.role }, presence: this.presence(), history: this.kept.map((m) => this.shown(m)) }));
    this.broadcast({ type: "presence", presence: this.presence() });
    return upgraded(client);
  }

  shown(m) {
    return { mid: m.mid, at: m.at, nickname: m.nickname, flag: m.flag, role: m.role, text: m.text };
  }

  refuse(ws, error, wait) {
    ws.send(JSON.stringify({ type: "refused", error, ...(wait ? { wait } : {}) }));
  }

  async webSocketMessage(ws, data) {
    const now = Date.now();
    const me = this.who(ws);
    if (!me.id) {
      ws.close(4001, "Sign in again.");
      return;
    }
    await this.history();
    // (the sockets' members asked about again now and then: a revocation whose notice was lost still lands)
    if (now - this.checkedAt > kRecheck) {
      this.checkedAt = now;
      const ids = [...new Set(this.sockets().map((s) => this.who(s).id).filter(Boolean))];
      const members = this.env.MEMBERS.get(this.env.MEMBERS.idFromName("all"));
      const { gone } = await (await members.fetch("https://members/still", { method: "POST", body: JSON.stringify({ ids }) })).json();
      if (gone.length) {
        for (const s of this.sockets()) {
          if (gone.includes(this.who(s).id)) {
            s.close(4003, "Your membership was revoked");
          }
        }
        if (gone.includes(me.id)) {
          return;
        }
      }
    }
    let event;
    try {
      event = JSON.parse(typeof data === "string" ? data : new TextDecoder().decode(data));
    } catch {
      return this.refuse(ws, "Not understood");
    }
    if (!event || typeof event !== "object") {
      return this.refuse(ws, "Not understood");
    }
    const wait = this.take(me.id, now);
    if (wait) {
      return this.refuse(ws, "Slow down", wait);
    }
    if (event.type === "say") {
      const until = this.mutes[me.id] || 0;
      if (until > now) {
        return this.refuse(ws, "You are muted", Math.ceil((until - now) / 1000));
      }
      const text = cleanChat(event.text);
      if (!text) {
        return;
      }
      const message = { mid: this.next++, at: now, id: me.id, nickname: me.nickname, flag: me.flag, role: me.role, text };
      this.kept.push(message);
      this.kept = this.kept.slice(-kHistory);
      await this.state.storage.put("history", this.kept);
      await this.state.storage.put("next", this.next);
      if (this.kept.length === 1) {
        await this.state.storage.setAlarm(now + kDay);
      }
      this.broadcast({ type: "message", message: this.shown(message) });
      return;
    }
    if (event.type === "state") {
      const state = ["lobby", "playing", "watching"].includes(event.state) ? event.state : "lobby";
      // (community-three-panel-layout: the game being played, a short id only; anything else is no game)
      const game = state === "playing" && typeof event.game === "string" && /^[a-z0-9_]{1,24}$/.test(event.game) ? event.game : "";
      // (match-spectators 6.4: the match's watch id, from the match service, so that others can watch from here; only
      // with a game being played, and only in its shape: never a code)
      const watch = game && typeof event.watch === "string" && /^[a-z2-7]{16}$/.test(event.watch) ? event.watch : "";
      for (const s of this.sockets()) {
        if (this.who(s).id === me.id) {
          s.serializeAttachment({ ...this.who(s), state, game, watch });
        }
      }
      this.broadcast({ type: "presence", presence: this.presence() });
      return;
    }
    // Moderation: moderators and administrators only, each action logged by Members
    if (["delete", "mute", "kick"].includes(event.type)) {
      if (me.role !== "moderator" && me.role !== "admin") {
        return this.refuse(ws, "Only moderators can do that");
      }
      const members = this.env.MEMBERS.get(this.env.MEMBERS.idFromName("all"));
      const target = this.kept.find((m) => m.mid === event.mid);
      const whoByName = (nickname) => {
        for (const s of this.sockets()) {
          const w = this.who(s);
          if (w.nickname === nickname) {
            return w.id;
          }
        }
        const said = this.kept.find((m) => m.nickname === nickname);
        return said ? said.id : "";
      };
      if (event.type === "delete") {
        if (!target) {
          return this.refuse(ws, "No such message");
        }
        this.kept = this.kept.filter((m) => m.mid !== event.mid);
        await this.state.storage.put("history", this.kept);
        await members.fetch("https://members/log", { method: "POST", body: JSON.stringify({ by: me.id, action: "delete", who: target.id, detail: String(target.mid) }) });
        this.broadcast({ type: "deleted", mid: event.mid });
        return;
      }
      const id = whoByName(String(event.nickname || ""));
      if (!id) {
        return this.refuse(ws, "No such member here");
      }
      if (event.type === "mute") {
        const minutes = kMuteMinutes.includes(event.minutes) ? event.minutes : 10;
        this.mutes[id] = now + minutes * 60 * 1000;
        await this.state.storage.put("mutes", this.mutes);
        await members.fetch("https://members/log", { method: "POST", body: JSON.stringify({ by: me.id, action: "mute", who: id, detail: `${minutes} min` }) });
        for (const s of this.sockets()) {
          if (this.who(s).id === id) {
            s.send(JSON.stringify({ type: "muted", minutes }));
          }
        }
        return;
      }
      // (kick: out of the lobby until they connect again)
      await members.fetch("https://members/log", { method: "POST", body: JSON.stringify({ by: me.id, action: "kick", who: id }) });
      for (const s of this.sockets()) {
        if (this.who(s).id === id) {
          s.close(4004, "A moderator removed you from the lobby");
        }
      }
      this.broadcast({ type: "presence", presence: this.presence() });
      return;
    }
    this.refuse(ws, "Not understood");
  }

  async webSocketClose(ws) {
    try {
      ws.close(1000, "bye");
    } catch {
      // (already closed)
    }
    // (the closing one may still be listed while this runs)
    this.broadcast({ type: "presence", presence: this.presence(ws) }, ws);
  }

  async webSocketError(ws) {
    await this.webSocketClose(ws);
  }

  async alarm() {
    await this.trim(Date.now());
  }
}
