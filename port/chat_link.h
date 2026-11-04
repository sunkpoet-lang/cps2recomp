// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy and the fork's contributors. Part of cps2recomp (this fork).
//
// (this fork) The in-game chat's link to the match: Tommy's netplay (netplay.h) or ForkAde's GekkoNet session
// (online.h), so that the chat's screen and keys (Port::Say, Port::HearChat, the window's) work over either
// (ForkAde's openspec change in-game-chat; the protocol: ForkAde's docs/chat-protocol.md)
#pragma once

#include <string>
#include <vector>

namespace port {

class ChatLink {
public:
    virtual ~ChatLink() = default;
    enum class Said { Sent, Empty, Wait };
    // A typed line: sent unless empty or over the rate; `line` is what was sent (cleaned)
    virtual Said ChatSay(const std::string &text, std::string &line) = 0;
    // The other side's lines that have come since the last call, cleaned
    virtual std::vector<std::string> ChatHeard() = 0;
    // A player's name ("" for none given), which player is this side's, and whether it hosts
    virtual std::string ChatName(int player) const = 0;
    virtual int ChatLocal() const = 0;
    virtual bool ChatHost() const = 0;
    // The match is on and the chat can be used; the match is over (its lines not acknowledged: not delivered)
    virtual bool ChatReady() const = 0;
    virtual bool ChatOver() const = 0;
    virtual std::vector<std::string> ChatUndelivered() const = 0;
};

} // namespace port
