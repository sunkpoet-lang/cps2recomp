// Where the ForkAde online service is (discord-community-lobby). The pages are served by the service's own Worker
// (its static assets: /join and /admin), so the service is this page's own origin, wherever it is published.
window.FORKADE_API = window.FORKADE_API || location.origin;
// The community's Discord server (an invitation anyone with the link can use)
window.FORKADE_DISCORD_INVITE = window.FORKADE_DISCORD_INVITE || "https://discord.gg/z2ZyzRfCk";
