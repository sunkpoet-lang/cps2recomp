// SPDX-License-Identifier: GPL-3.0-only
// (this fork, ForkAde's discord-community-lobby) Who may use the social layer: people who signed in with Discord,
// asked to join with a nickname and a flag, and were approved by an administrator. Kept by one Durable Object
// (Members): the requests, the members with their roles, the sessions (only their SHA-256), the launchers' sign-in
// requests, the Discord sign-ins under way (their `state`), and the moderation log. Nothing else from Discord is
// kept: no email, no avatar, no server list.

export const kRoles = ["member", "moderator", "admin"];
const kSessionLife = 30 * 24 * 3600 * 1000; // (30 days after last use)
const kWebLife = 12 * 3600 * 1000; // (the registration and admin pages' session)
const kSignInLife = 10 * 60 * 1000; // (a launcher's sign-in request, and a Discord sign-in under way)
const kLogLife = 30 * 24 * 3600 * 1000;

// A nickname: 3 to 16 letters, digits, _ and -; the reason if not
export function nicknameProblem(nickname) {
  if (typeof nickname !== "string" || nickname.length < 3 || nickname.length > 16) {
    return "A nickname has 3 to 16 characters";
  }
  if (!/^[A-Za-z0-9_-]+$/.test(nickname)) {
    return "A nickname has only letters, digits, _ and -";
  }
  return "";
}

// A flag: an ISO 3166-1 alpha-2 country code, as the pages and the launcher show it
export function cleanFlag(flag) {
  return typeof flag === "string" && /^[A-Z]{2}$/.test(flag) ? flag : "";
}

function hex(bytes) {
  return Array.from(bytes, (b) => b.toString(16).padStart(2, "0")).join("");
}

export function newToken() {
  return hex(crypto.getRandomValues(new Uint8Array(32)));
}

export async function hashOf(token) {
  return hex(new Uint8Array(await crypto.subtle.digest("SHA-256", new TextEncoder().encode(String(token)))));
}

function json(body, status = 200) {
  return new Response(JSON.stringify(body), { status, headers: { "content-type": "application/json" } });
}

export class Members {
  constructor(state, env) {
    this.state = state;
    this.env = env;
    this.data = null;
  }

  // Everything, read once and written back whole: the beta is about 50 people, a few kilobytes
  async load() {
    if (!this.data) {
      this.data = (await this.state.storage.get("members")) || {
        requests: {}, // discordId -> {nickname, flag, asked, refused?}
        members: {}, // discordId -> {nickname, flag, role, joined}
        sessions: {}, // sha256 -> {id, used, kind: "launcher" | "web"}
        signIns: {}, // launcher sign-in id -> {secret, made, id?, session?}
        states: {}, // Discord `state` -> {for, made}
        log: [], // {at, by, action, who, detail}
      };
    }
    return this.data;
  }

  async save() {
    await this.state.storage.put("members", this.data);
  }

  // (the expired let go: sessions, sign-ins, states, log)
  sweep(d, now) {
    for (const [hash, s] of Object.entries(d.sessions)) {
      if (now - s.used > (s.kind === "web" ? kWebLife : kSessionLife)) {
        delete d.sessions[hash];
      }
    }
    for (const [id, s] of Object.entries(d.signIns)) {
      if (now - s.made > kSignInLife) {
        delete d.signIns[id];
      }
    }
    for (const [state, s] of Object.entries(d.states)) {
      if (now - s.made > kSignInLife) {
        delete d.states[state];
      }
    }
    d.log = d.log.filter((entry) => now - entry.at < kLogLife);
  }

  isFirstAdmin(id) {
    return Boolean(this.env && this.env.ADMIN_DISCORD_ID && id === this.env.ADMIN_DISCORD_ID);
  }

  roleOf(d, id) {
    if (this.isFirstAdmin(id)) {
      return "admin";
    }
    return d.members[id] ? d.members[id].role : "";
  }

  nicknameTaken(d, nickname, exceptId) {
    const wanted = nickname.toLowerCase();
    for (const [id, m] of Object.entries(d.members)) {
      if (id !== exceptId && m.nickname.toLowerCase() === wanted) {
        return true;
      }
    }
    for (const [id, r] of Object.entries(d.requests)) {
      if (id !== exceptId && !r.refused && r.nickname.toLowerCase() === wanted) {
        return true;
      }
    }
    return false;
  }

  async sessionOf(d, token, now) {
    const s = d.sessions[await hashOf(token)];
    if (!s) {
      return null;
    }
    s.used = now;
    return s;
  }

  async fetch(request) {
    const url = new URL(request.url);
    const now = Date.now();
    let body = {};
    if (request.method === "POST") {
      try {
        body = JSON.parse(await request.text()) || {};
      } catch {
        return json({ error: "not JSON" }, 400);
      }
    }
    const d = await this.load();
    this.sweep(d, now);
    const done = async (answer, status = 200) => {
      await this.save();
      return json(answer, status);
    };
    const log = (by, action, who, detail = "") => d.log.push({ at: now, by, action, who, detail });

    switch (url.pathname) {
      // --- Discord sign-ins under way: a `state` for the authorize URL, taken back once by the callback
      case "/state/new": {
        const state = newToken();
        d.states[state] = { for: String(body.for || "web").slice(0, 64), made: now };
        return done({ state });
      }
      case "/state/take": {
        const s = d.states[String(body.state || "")];
        if (!s) {
          return done({ error: "That sign-in has expired: try again." }, 400);
        }
        delete d.states[String(body.state)];
        return done({ for: s.for });
      }

      // --- a launcher's sign-in request: made, completed by the Discord callback, asked about with its secret
      case "/signin/new": {
        const id = newToken().slice(0, 24), secret = newToken();
        d.signIns[id] = { secret: await hashOf(secret), made: now };
        return done({ id, secret });
      }
      case "/signin/complete": {
        const s = d.signIns[String(body.signIn || "")];
        if (!s || s.id) {
          return done({ error: "That sign-in has expired, or was used already: try again from the launcher." }, 400);
        }
        s.id = String(body.id);
        return done({});
      }
      case "/signin/poll": {
        const s = d.signIns[String(body.signIn || "")];
        if (!s || s.secret !== (await hashOf(body.secret || ""))) {
          return done({ state: "expired" }, 404);
        }
        if (!s.id) {
          return done({ state: "pending" });
        }
        if (!d.members[s.id] && this.roleOf(d, s.id) !== "admin") {
          const r = d.requests[s.id];
          return done({ state: r && !r.refused ? "waiting" : "not-member" });
        }
        // (once: the session handed over, the request gone)
        const session = newToken();
        d.sessions[await hashOf(session)] = { id: s.id, used: now, kind: "launcher" };
        delete d.signIns[String(body.signIn)];
        return done({ state: "done", session, ...this.profile(d, s.id) });
      }

      // --- the pages' session (registration, admin), made by the Discord callback
      case "/web/new": {
        const session = newToken();
        d.sessions[await hashOf(session)] = { id: String(body.id), used: now, kind: "web" };
        return done({ session });
      }

      // --- who a session is
      case "/whoami": {
        const s = await this.sessionOf(d, body.session, now);
        if (!s) {
          return done({ error: "Sign in again." }, 401);
        }
        const r = d.requests[s.id];
        return done({ id: s.id, ...this.profile(d, s.id), request: r ? (r.refused ? "refused" : "pending") : "" });
      }

      // --- asking to join (the registration page)
      case "/request": {
        const s = await this.sessionOf(d, body.session, now);
        if (!s || s.kind !== "web") {
          return done({ error: "Sign in again." }, 401);
        }
        if (d.members[s.id]) {
          return done({ error: "You are a member already." }, 409);
        }
        const nickname = String(body.nickname || ""), flag = cleanFlag(body.flag);
        const problem = nicknameProblem(nickname);
        if (problem) {
          return done({ error: problem }, 400);
        }
        if (!flag) {
          return done({ error: "Choose a flag" }, 400);
        }
        if (this.nicknameTaken(d, nickname, s.id)) {
          return done({ error: "That nickname is taken" }, 409);
        }
        // (the first administrator, named by ADMIN_DISCORD_ID, is one before having a nickname: theirs is taken as
        // asked, with no approval to wait for)
        if (this.roleOf(d, s.id) === "admin") {
          d.members[s.id] = { nickname, flag, role: "member", joined: now };
          delete d.requests[s.id];
          log(s.id, "joined", s.id, nickname);
          return done({ request: "", member: true, ...this.profile(d, s.id) });
        }
        d.requests[s.id] = { nickname, flag, asked: now };
        return done({ request: "pending" });
      }

      // --- administrators (the admin page)
      case "/admin/list":
      case "/admin/approve":
      case "/admin/refuse":
      case "/admin/role":
      case "/admin/revoke":
      case "/admin/log": {
        const s = await this.sessionOf(d, body.session, now);
        if (!s || this.roleOf(d, s.id) !== "admin") {
          return done({ error: "Only an administrator can do that." }, 403);
        }
        const who = String(body.id || "");
        if (url.pathname === "/admin/list") {
          return done({
            requests: Object.entries(d.requests).map(([id, r]) => ({ id, ...r })),
            // (the role each has in effect; `fixed`: the first administrator, named by ADMIN_DISCORD_ID)
            members: Object.entries(d.members).map(([id, m]) => ({ id, ...m, role: this.roleOf(d, id), fixed: this.isFirstAdmin(id) })),
          });
        }
        // (the first administrator's role and membership come from the settings: not changed or revoked from here)
        if ((url.pathname === "/admin/role" || url.pathname === "/admin/revoke") && this.isFirstAdmin(who)) {
          return done({ error: "The first administrator is set in the service's settings" }, 409);
        }
        if (url.pathname === "/admin/log") {
          return done({ log: d.log });
        }
        if (url.pathname === "/admin/approve") {
          const r = d.requests[who];
          if (!r || r.refused) {
            return done({ error: "No such request" }, 404);
          }
          d.members[who] = { nickname: r.nickname, flag: r.flag, role: "member", joined: now };
          delete d.requests[who];
          log(s.id, "approve", who);
          return done({});
        }
        if (url.pathname === "/admin/refuse") {
          if (!d.requests[who]) {
            return done({ error: "No such request" }, 404);
          }
          d.requests[who].refused = now;
          log(s.id, "refuse", who);
          return done({});
        }
        if (url.pathname === "/admin/role") {
          const role = String(body.role || "");
          if (!d.members[who] || !kRoles.includes(role)) {
            return done({ error: "No such member or role" }, 404);
          }
          d.members[who].role = role;
          log(s.id, "role", who, role);
          return done({});
        }
        // (revoke: the member gone, and every session of theirs)
        if (!d.members[who]) {
          return done({ error: "No such member" }, 404);
        }
        delete d.members[who];
        for (const [hash, one] of Object.entries(d.sessions)) {
          if (one.id === who) {
            delete d.sessions[hash];
          }
        }
        log(s.id, "revoke", who);
        await this.save();
        if (this.env && this.env.COMMUNITY) {
          await this.env.COMMUNITY.get(this.env.COMMUNITY.idFromName("all")).fetch("https://community/revoked", { method: "POST", body: JSON.stringify({ id: who }) }).catch(() => {});
        }
        return json({});
      }

      // --- for the lobby: whether a session is a member's (and who), and a moderation action logged
      case "/member": {
        const s = await this.sessionOf(d, body.session, now);
        if (!s || s.kind !== "launcher" || (!d.members[s.id] && this.roleOf(d, s.id) !== "admin")) {
          return done({ error: "Not a member" }, 401);
        }
        return done({ id: s.id, ...this.profile(d, s.id) });
      }
      case "/still": {
        // (the lobby re-checking its sockets' members: those no longer members)
        const ids = Array.isArray(body.ids) ? body.ids.map(String) : [];
        return done({ gone: ids.filter((id) => !d.members[id] && this.roleOf(d, id) !== "admin") });
      }
      case "/log": {
        log(String(body.by || ""), String(body.action || ""), String(body.who || ""), String(body.detail || "").slice(0, 64));
        return done({});
      }
    }
    return json({ error: "not here" }, 404);
  }

  profile(d, id) {
    const m = d.members[id];
    const role = this.roleOf(d, id);
    if (m) {
      return { nickname: m.nickname, flag: m.flag, role };
    }
    const r = d.requests[id];
    return { nickname: r ? r.nickname : "", flag: r ? r.flag : "", role };
  }
}


// ---- The public routes of membership (the Worker's fetch hands /v1/auth/..., /v1/members/... and /v1/admin/... here)

const kDiscordApi = "https://discord.com/api/v10";

function cors(env) {
  return env.PAGES_ORIGIN ? { "access-control-allow-origin": env.PAGES_ORIGIN, "access-control-allow-headers": "content-type", "access-control-allow-methods": "GET, POST", vary: "origin" } : {};
}

function answer(env, body, status = 200, extra = {}) {
  return new Response(JSON.stringify(body), { status, headers: { "content-type": "application/json", ...cors(env), ...extra } });
}

function page(text, status = 200) {
  const html = `<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>ForkAde</title>
<style>body{font:16px system-ui;margin:48px auto;max-width:520px;padding:0 16px;background:#16171a;color:#e8e8ea}</style><p>${text}</p>`;
  return new Response(html, { status, headers: { "content-type": "text/html; charset=utf-8", "cache-control": "no-store" } });
}

// What Discord says about who signed in: their id, from the code its authorize page gave back. The access token is
// used once and not kept.
async function discordId(env, code) {
  const api = env.DISCORD_API || kDiscordApi;
  const form = new URLSearchParams({
    client_id: env.DISCORD_CLIENT_ID || "",
    client_secret: env.DISCORD_CLIENT_SECRET || "",
    grant_type: "authorization_code",
    code,
    redirect_uri: `${env.PUBLIC_URL}/v1/auth/discord/callback`,
  });
  const token = await fetch(`${api}/oauth2/token`, { method: "POST", body: form, headers: { "content-type": "application/x-www-form-urlencoded" } });
  if (!token.ok) {
    return null;
  }
  const { access_token: access } = await token.json();
  const me = await fetch(`${api}/users/@me`, { headers: { authorization: `Bearer ${access}` } });
  if (!me.ok) {
    return null;
  }
  const { id } = await me.json();
  return typeof id === "string" && /^\d{5,24}$/.test(id) ? id : null;
}

export async function membershipFetch(request, env, url, parts, text) {
  if (request.method === "OPTIONS") {
    return new Response(null, { status: 204, headers: cors(env) });
  }
  const members = env.MEMBERS.get(env.MEMBERS.idFromName("all"));
  const ask = async (path, body) => {
    const said = await members.fetch(`https://members${path}`, { method: "POST", body: JSON.stringify(body || {}) });
    return { status: said.status, body: await said.json() };
  };
  const relay = async (path, body) => {
    const { status, body: said } = await ask(path, body);
    return answer(env, said, status);
  };
  const body = request.method === "POST" ? JSON.parse(text || "{}") : {};
  const route = parts.slice(1).join("/");

  // Discord's authorize page, for a launcher's sign-in request (for=<its id>) or the pages (for=web)
  if (request.method === "GET" && route === "auth/discord/start") {
    const wanted = (url.searchParams.get("for") || "web").slice(0, 64);
    const { body: made } = await ask("/state/new", { for: wanted });
    // (DISCORD_AUTHORIZE_URL: a local stand-in's, for the dev server; Discord's otherwise)
    const go = new URL(env.DISCORD_AUTHORIZE_URL || "https://discord.com/oauth2/authorize");
    go.searchParams.set("client_id", env.DISCORD_CLIENT_ID || "");
    go.searchParams.set("response_type", "code");
    go.searchParams.set("scope", "identify");
    go.searchParams.set("redirect_uri", `${env.PUBLIC_URL}/v1/auth/discord/callback`);
    go.searchParams.set("state", made.state);
    go.searchParams.set("prompt", "none");
    return Response.redirect(go.toString(), 302);
  }
  if (request.method === "GET" && route === "auth/discord/callback") {
    const taken = await ask("/state/take", { state: url.searchParams.get("state") || "" });
    if (taken.status !== 200) {
      return page("That sign-in has expired. Try again.", 400);
    }
    const code = url.searchParams.get("code");
    const id = code ? await discordId(env, code) : null;
    if (!id) {
      return page("Discord didn't confirm who you are. Try again.", 400);
    }
    // (the pages: back to the join page, or the admin page for for=web-admin, with the session in the hash)
    if (taken.body.for === "web" || taken.body.for === "web-admin") {
      const { body: web } = await ask("/web/new", { id });
      const back = new URL(taken.body.for === "web-admin" ? env.PAGES_ADMIN_URL || `${env.PAGES_ORIGIN || ""}/admin.html`
                                                          : env.PAGES_URL || `${env.PAGES_ORIGIN || ""}/join.html`);
      back.hash = `session=${web.session}`;
      return Response.redirect(back.toString(), 302);
    }
    const completed = await ask("/signin/complete", { signIn: taken.body.for, id });
    return completed.status === 200 ? page("Signed in. You can go back to ForkAde.") : page(completed.body.error, 400);
  }
  // A launcher's sign-in request, and its question while it waits
  if (request.method === "POST" && route === "auth/request") {
    return relay("/signin/new");
  }
  if (request.method === "GET" && parts[1] === "auth" && parts[2] === "request" && parts.length === 4) {
    return relay("/signin/poll", { signIn: parts[3], secret: url.searchParams.get("secret") || "" });
  }
  // The pages: who I am, asking to join, and the administrators' actions
  if (request.method === "POST" && route === "members/me") {
    return relay("/whoami", { session: body.session });
  }
  if (request.method === "POST" && route === "members/request") {
    return relay("/request", { session: body.session, nickname: body.nickname, flag: body.flag });
  }
  if (request.method === "POST" && parts[1] === "admin" && parts.length === 3 && ["list", "approve", "refuse", "role", "revoke", "log"].includes(parts[2])) {
    return relay(`/admin/${parts[2]}`, { session: body.session, id: body.id, role: body.role });
  }
  return answer(env, { error: "not here" }, 404);
}
